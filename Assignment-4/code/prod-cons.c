// prod-cons.c
// A Producer-Consumer Problem implementation with mutex locks and condition variables

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define MAX 100  // Maximum buffer size

// Shared buffer and related variables
__uint32_t buffer[MAX];
int fill = 0;               // Index to place produced items
int use = 0;                // Index to consume items
int count = 0;              // Number of items currently in the buffer
int producer_done = 0;      // Flag to indicate producer is done

// Synchronization primitives
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t empty_cond = PTHREAD_COND_INITIALIZER;
pthread_cond_t fill_cond = PTHREAD_COND_INITIALIZER;


// HELPER FUNCTIONS FOR PRODUCER
// Adds a value to the buffer.
void put(__uint32_t value) {
    buffer[fill] = value;        // Place value in the buffer
    fill = (fill + 1) % MAX;     // Advance fill index (circular buffer)
    count++;                     // Increment buffer item count
}

// Opens the input file for the producer.
FILE *open_input_file() {
    FILE *input_file = fopen("input-part1.txt", "r");
    if (!input_file) {
        perror("Error opening input file");
        exit(1);
    }
    return input_file;
}

// Produces values from input file, placing them into the buffer. 
void produce_values(FILE *input_file) {
    __uint32_t value;
    while (fscanf(input_file, "%u", &value) == 1) {
        pthread_mutex_lock(&mutex);  // Lock mutex for buffer access

        while (count == MAX) {       // Wait if buffer is full
            pthread_cond_wait(&empty_cond, &mutex);
        }

        if (value == 0) {  // End production if value is zero
            producer_done = 1;
            pthread_cond_broadcast(&fill_cond); // Notify consumer(s)
            pthread_mutex_unlock(&mutex);
            break;
        }

        put(value);                     // Place item in buffer
        pthread_cond_signal(&fill_cond); // Notify consumer
        pthread_mutex_unlock(&mutex);
    }
}


// HELPER FUNCTIONS FOR CONSUMER
// Retrieves a value from the buffer. 
__uint32_t get() {
    __uint32_t tmp = buffer[use];  // Retrieve item from buffer
    use = (use + 1) % MAX;           // Advance use index (circular buffer)
    count--;                         // Decrement buffer item count
    return tmp;
}

// Opens the output file for the consumer.
FILE *open_output_file() {
    FILE *output_file = fopen("output-part1.txt", "w");
    if (!output_file) {
        perror("Error opening output file");
        exit(1);
    }
    return output_file;
}

// Logs buffer state and consumed value to the output file.
void log_buffer_state(FILE *output_file, __uint32_t value) {
    fprintf(output_file, "Consumed:[%u],Buffer-State:[", value);

    // Print buffer contents from 'use' to 'fill'
    int i = use;
    int c = count;
    for (int j = 0; j < c; j++) {
        int idx = (i + j) % MAX;
        fprintf(output_file, "%u", buffer[idx]);
        if (j < c - 1) {
            fprintf(output_file, ",");
        }
    }
    fprintf(output_file, "]\n");
}

// Consumes values from the buffer and writes to output file.
void consume_values(FILE *output_file) {
    while (1) {
        pthread_mutex_lock(&mutex); // Lock mutex for buffer access

        while (count == 0 && !producer_done) {
            pthread_cond_wait(&fill_cond, &mutex); // Wait if buffer is empty and producer is active
        }

        if (count == 0 && producer_done) { // Exit if no items left and producer is done
            pthread_mutex_unlock(&mutex);
            break;
        }

        __uint32_t tmp = get();         // Consume item from buffer
        log_buffer_state(output_file, tmp); // Log buffer state and consumed item
        pthread_cond_signal(&empty_cond);      // Notify producer if buffer has space
        pthread_mutex_unlock(&mutex);     // Unlock mutex after writing
    }
}

// MAIN PRODUCER FUNCTION
// Producer thread function to produce values.
void *producer(void *arg) {
    FILE *input_file = open_input_file(); // Open input file
    produce_values(input_file);           // Produce values
    fclose(input_file);                   // Close the input file
    return NULL;
}


// MAIN CONSUMER FUNCTION
// Consumer thread function to consume values and log buffer state.
void *consumer(void *arg) {
    FILE *output_file = open_output_file(); // Open output file
    consume_values(output_file);            // Consume values
    fclose(output_file);                    // Close the output file
    return NULL;
}


// MAIN FUNCTION
// Main function to initialize and join producer and consumer threads.
int main() {
    pthread_t p, c; // Thread identifiers

    // Create producer and consumer threads
    pthread_create(&p, NULL, producer, NULL);
    pthread_create(&c, NULL, consumer, NULL);

    // Wait for both threads to finish
    pthread_join(p, NULL);
    pthread_join(c, NULL);

    return 0;
}
