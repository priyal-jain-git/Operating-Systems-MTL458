#include <stdio.h>
#include <stdlib.h>
// My headers
#include <sys/mman.h>   // Provides mmap and munmap functions for memory management
#include <unistd.h>     // Provides sbrk and getpagesize functions
#include <string.h>     // Provides memset function
#include <stdint.h>     // Provides fixed-width integer types

// Constants to define memory management rules
#define ALIGNMENT 16                     // Alignment requirement for memory blocks
#define MAGIC 1234567                    // Magic number for validating memory blocks during free
#define MMAP_THRESHOLD (8 * 1024)      // Size threshold for mmap vs. heap allocation (8 KB)
#define HEAP_EXPANSION_SIZE (16 * 1024) // Expansion size for the heap (16 MB)

// Structure representing the header of a memory block
typedef struct block_header {
    size_t size;                         // Size of the block
    int is_free;                         // Indicates whether the block is free (1) or allocated (0)
    int is_mmap;                         // Indicates if the block was allocated using mmap (1)
    struct block_header* next;           // Pointer to the next block in the free list
    int magic;                           // Magic number for validation
} block_header_t;

// Global variables for heap management
static block_header_t* free_list = NULL; // Head of the free list
static void* heap_start = NULL;          // Start of the current heap space
static void* heap_end = NULL;            // End of the current heap space

// Alignment functions 
// Aligns the requested size to the nearest multiple of ALIGNMENT
static size_t align_size(size_t size) {
    return (size + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
}

// Aligns the requested size to the system's page size
static size_t align_to_page(size_t size) {
    size_t page_size = getpagesize();
    return ((size + page_size - 1) / page_size) * page_size;
}

// Free list management functions
// Appends a block to the free list, ensuring the list remains linked properly
static void append_block_to_free_list(block_header_t* block) {
    block->next = NULL;
    if (free_list == NULL) {
        free_list = block;
    } else {
        block_header_t* current = free_list;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = block;
    }
}

// Finds the previous block in the free list (useful for merging operations)
static block_header_t* find_prev_block(block_header_t* block) {
    block_header_t* current = free_list;
    while (current != NULL && current->next != block) {
        current = current->next;
    }
    return current;
}

// Searches for a free block that can satisfy a memory request of a given size
static block_header_t* find_free_block(size_t size) {
    block_header_t* current = free_list;
    while (current != NULL) {
        if (current->is_free && current->size >= size) {
            return current;  // Return the first free block that fits the size
        }
        current = current->next;
    }
    return NULL;  // No suitable block found
}

// Block management functions 
// Splits a larger block into two smaller blocks if there's extra space after allocation
static block_header_t* split_block(block_header_t* block, size_t size) {
    size_t min_block_size = sizeof(block_header_t) + ALIGNMENT; // Minimum size for a block
    if (block->size >= size + min_block_size) {
        // Create a new block from the excess memory
        block_header_t* new_block = (block_header_t*)((char*)(block + 1) + size);
        new_block->size = block->size - size - sizeof(block_header_t);
        new_block->is_free = 1;   // Mark new block as free
        new_block->is_mmap = 0;   // Not allocated via mmap
        new_block->next = block->next; // Link the new block to the next one in the list
        new_block->magic = MAGIC; // Set magic number for validation
        block->size = size;       // Update original block's size
        block->next = new_block;  // Link to the new block
    }
    return block;  // Return the (possibly split) block
}

// Merges the current block with the next block if it's free
static void merge_with_next_block(block_header_t* block) {
    if (block->next != NULL && block->next->is_free) {
        // Merge with the next block by updating the size and skipping over the next block
        block->size += sizeof(block_header_t) + block->next->size;
        block->next = block->next->next;
    }
}

// Merges the current block with the previous block if it's free
static void merge_with_prev_block(block_header_t** block) {
    block_header_t* prev = find_prev_block(*block);
    if (prev != NULL && prev->is_free) {
        // Merge with the previous block by updating the size and skipping over the current block
        prev->size += sizeof(block_header_t) + (*block)->size;
        prev->next = (*block)->next;
        *block = prev;  // Update block pointer to point to the merged block
    }
}

// Merges adjacent free blocks together to reduce fragmentation
static void merge_adjacent_free_blocks(block_header_t** block) {
    merge_with_next_block(*block);  // First, try to merge with the next block
    merge_with_prev_block(block);   // Then, try to merge with the previous block
}

// Memory allocation functions 
// Requests a new block of memory from the heap, expanding the heap if necessary
static block_header_t* request_new_heap_block(size_t size) {
    // Align total requested size to the page size (including block header)
    size_t total_size = align_to_page(size + sizeof(block_header_t));

    // If there is no heap space left or the remaining heap space is insufficient, expand the heap
    if (heap_start == NULL || heap_end - heap_start < total_size) {
        // Expand the heap by requesting a large chunk of memory using mmap
        size_t expansion_size = HEAP_EXPANSION_SIZE;
        void* new_heap = mmap(
            NULL,
            expansion_size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0
        );
        if (new_heap == MAP_FAILED) {
            return NULL;  // mmap failed
        }
        heap_start = new_heap;                 // Set heap_start to the beginning of the newly allocated chunk
        heap_end = heap_start + expansion_size; // Set heap_end to the end of the newly allocated chunk
    }

    // Carve out a block from the newly expanded heap
    block_header_t* block = heap_start;
    heap_start = (void*)((char*)heap_start + total_size); // Move heap_start forward by the size of the allocated block

    // Initialize the block's metadata (header)
    block->size = total_size - sizeof(block_header_t);
    block->is_free = 0;         // Mark the block as allocated
    block->is_mmap = 0;         // This block was not directly allocated via mmap
    block->next = NULL;         // Not yet part of the free list
    block->magic = MAGIC;       // Set the magic number for validation

    // Add the remaining space (if any) to the free list
    if (heap_end - heap_start > sizeof(block_header_t)) {
        // There is remaining space after the allocation, so create a new free block
        block_header_t* remaining_block = (block_header_t*)heap_start;
        remaining_block->size = heap_end - heap_start - sizeof(block_header_t);
        remaining_block->is_free = 1;        // Mark it as free
        remaining_block->is_mmap = 0;        // It is part of the heap, not mmap
        remaining_block->next = NULL;        // Not linked yet
        remaining_block->magic = MAGIC;      // Set the magic number

        // Move heap_start to the end of the remaining space
        heap_start = heap_end;

        // Add the remaining block to the free list
        append_block_to_free_list(remaining_block);
    }

    // Append the allocated block to the free list 
    append_block_to_free_list(block);
    
    return block;
}

// Requests a new memory block using mmap for large allocations (>= MMAP_THRESHOLD)
static block_header_t* request_new_mmap_block(size_t size) {
    size_t total_size = align_to_page(size + sizeof(block_header_t));  // Align to page size
    // Allocate memory using mmap
    block_header_t* block = mmap(
        NULL, 
        total_size, 
        PROT_READ | PROT_WRITE, 
        MAP_PRIVATE | MAP_ANONYMOUS, 
        -1, 
        0
    );
    if (block == MAP_FAILED) {
        return NULL;  // mmap failed
    }

    // Initialize the block header
    block->size = total_size - sizeof(block_header_t);
    block->is_free = 0;        // Mark as allocated
    block->is_mmap = 1;        // Indicate that mmap was used
    block->next = NULL;        // Not part of the free list
    block->magic = MAGIC;      // Set the magic number for validation

    return block;
}

// Function to allocate memory
void* my_malloc(size_t size) {
    if (size == 0) {
        return NULL;  // Cannot allocate 0 bytes
    }

    size_t aligned_size = align_size(size);  // Align the requested size
    block_header_t* block;

    if (aligned_size >= MMAP_THRESHOLD) {
        // For large allocations, use mmap
        block = request_new_mmap_block(aligned_size);
        if (block == NULL) {
            return NULL;  // mmap allocation failed
        }
    } else {
        // Try to find a suitable free block in the free list
        block = find_free_block(aligned_size);
        if (block != NULL) {
            block = split_block(block, aligned_size);  // Split the block if necessary
            block->is_free = 0;      // Mark block as allocated
            block->magic = MAGIC;    // Set the magic number
        } else {
            // No suitable free block found, request a new block from the heap
            block = request_new_heap_block(aligned_size);
            if (block == NULL) {
                return NULL;  // Heap allocation failed
            }
        }
    }
    return (void*)(block + 1);  // Return the memory after the block header
}

// Function to allocate and initialize memory to zero
void* my_calloc(size_t nelem, size_t size) {
    size_t total_size = nelem * size;
    if (size != 0 && total_size / size != nelem) {
        return NULL;  // Check for overflow in multiplication
    }
    void* ptr = my_malloc(total_size);  // Allocate memory
    if (ptr != NULL) {
        memset(ptr, 0, total_size);  // Initialize the allocated memory to zero
    }
    return ptr;
}

// Function to release memory allocated using my_malloc and my_calloc
void my_free(void* ptr) {
    if (ptr == NULL) {
        return;  // Nothing to free
    }

    // Get the block header from the user pointer
    block_header_t* block = ((block_header_t*)ptr) - 1;

    // Validate the magic number to detect invalid free operations
    if (block->magic != MAGIC) {
        fprintf(stderr, "Invalid free detected at block %p!\n", block);
        return;
    }

    if (block->is_mmap) {
        // The block was allocated using mmap, so use munmap to deallocate it
        size_t total_size = block->size + sizeof(block_header_t);
        if (munmap(block, total_size) == -1) {
            perror("munmap failed");
        }
    } else {
        // The block was allocated from the internal heap
        block->is_free = 1;     // Mark the block as free
        block->magic = 0;       // Invalidate the magic number

        // Attempt to merge with adjacent free blocks
        merge_adjacent_free_blocks(&block);
    }
}
