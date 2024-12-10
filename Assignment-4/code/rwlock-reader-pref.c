// rwlock-reader-pref.c
// A Reader-Writer Lock with Reader Preference implementation

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

// Define the Reader-Writer lock structure
typedef struct _rwlock_t {
    sem_t lock;       // Semaphore for mutual exclusion when updating reader count
    sem_t writelock;  // Semaphore for writer access control
    int readers;      // Counter for the number of readers accessing the resource
} rwlock_t;

// Global variables
rwlock_t rwlock;      // Reader-writer lock instance
FILE *output_file;    // File pointer for output logging
sem_t output_lock;    // Semaphore to control access to the output file


//INITIALISATION FUNCTION
// Initialize the reader-writer lock and output file
void initialize_rwlock(rwlock_t *rw) {
    // Initialize the reader count to zero
    rw->readers = 0;

    // Initialize semaphores: rw->lock for reader count, rw->writelock for writer access
    sem_init(&rw->lock, 0, 1);       // Binary semaphore for reader count access
    sem_init(&rw->writelock, 0, 1);  // Binary semaphore to allow one writer at a time

    // Initialize the output file lock
    sem_init(&output_lock, 0, 1);

    // Open the output file to log activities
    output_file = fopen("output-reader-pref.txt", "w");
    if (output_file == NULL) {
        perror("Failed to open output file");  // Print error message if file opening fails
        exit(EXIT_FAILURE);
    }
}


//READER HELPER FUNCTIONS
// Log reader activity to the output file
void log_reader_activity(int num_readers) {
    // Write to output file with synchronized access
    sem_wait(&output_lock);
    fprintf(output_file, "Reading,Number-of-readers-present:[%d]\n", num_readers);
    fflush(output_file);
    sem_post(&output_lock);
}

// Reader operation: reads the shared file
void perform_read() {
    // Open the shared file to read contents (simulating the read operation)
    FILE *shared_file = fopen("shared-file.txt", "r");
    if (shared_file != NULL) {
        // Perform a read operation (e.g., read contents here, if needed)
        fclose(shared_file);  // Close file after reading
    }
}

// Acquire the read lock for readers
void rwlock_acquire_readlock(rwlock_t *rw) {
    // Lock to update readers count
    sem_wait(&rw->lock);
    rw->readers++;
    if (rw->readers == 1) {
        // If this is the first reader, lock the writelock to block writers
        sem_wait(&rw->writelock);
    }
    // Release lock after updating readers count
    sem_post(&rw->lock);
}

// Release the read lock for readers
void rwlock_release_readlock(rwlock_t *rw) {
    // Lock to update readers count
    sem_wait(&rw->lock);
    rw->readers--;
    if (rw->readers == 0) {
        // If this is the last reader, release writelock to allow writers
        sem_post(&rw->writelock);
    }
    // Release lock after updating readers count
    sem_post(&rw->lock);
}


//WRITER HELPER FUNCTIONS
// Log writer activity to the output file
void log_writer_activity(int num_readers) {
    // Log writer activity to output file with synchronized access
    sem_wait(&output_lock);
    fprintf(output_file, "Writing,Number-of-readers-present:[%d]\n", num_readers);
    fflush(output_file);
    sem_post(&output_lock);
}

// Writer operation: writes to the shared file
void perform_write() {
    // Open the shared file to append contents (simulating the write operation)
    FILE *shared_file = fopen("shared-file.txt", "a");
    if (shared_file != NULL) {
        fprintf(shared_file, "Hello world!\n");  // Append a line to the shared file
        fclose(shared_file);                     // Close file after writing
    }
}

// Acquire the write lock for writers
void rwlock_acquire_writelock(rwlock_t *rw) {
    // Lock to provide exclusive access to writers
    sem_wait(&rw->writelock);
}

// Release the write lock for writers
void rwlock_release_writelock(rwlock_t *rw) {
    // Release exclusive access lock for writers
    sem_post(&rw->writelock);
}


//MAIN READER FUNCTION
// Reader thread function
void *reader(void *arg) {
    // Acquire read lock for safe reading
    rwlock_acquire_readlock(&rwlock);

    // Get the current number of readers
    int num_readers;

    // Safely retrieve the readers count with mutual exclusion
    sem_wait(&rwlock.lock);
    num_readers = rwlock.readers;
    sem_post(&rwlock.lock);

    // Log reader activity to output file
    log_reader_activity(num_readers);

    // Perform the read operation
    perform_read();

    // Release the read lock after finishing reading
    rwlock_release_readlock(&rwlock);
    return NULL;
}


//MAIN WRITER FUNCTION
// Writer thread function
void *writer(void *arg) {
    // Acquire write lock to ensure exclusive access for writing
    rwlock_acquire_writelock(&rwlock);

    // Retrieve the current number of readers
    int num_readers;

    // Safely access the readers count
    sem_wait(&rwlock.lock);
    num_readers = rwlock.readers;
    sem_post(&rwlock.lock);

    // Log writer activity to output file
    log_writer_activity(num_readers);

    // Perform the write operation
    perform_write();

    // Release the write lock after finishing writing
    rwlock_release_writelock(&rwlock);
    return NULL;
}


// Main function
int main(int argc, char **argv) {
    if (argc != 3) return 1;
    int n = atoi(argv[1]);
    int m = atoi(argv[2]);

    // Perform initialization of locks, files, and other resources
    initialize_rwlock(&rwlock);

    pthread_t readers[n], writers[m];

    // Create reader and writer threads
    for (int i = 0; i < n; i++) pthread_create(&readers[i], NULL, reader, NULL);        
    for (int i = 0; i < m; i++) pthread_create(&writers[i], NULL, writer, NULL);

    // Wait for all threads to complete
    for (int i = 0; i < n; i++) pthread_join(readers[i], NULL);
    for (int i = 0; i < m; i++) pthread_join(writers[i], NULL);

    // Close the output file after all operations are complete
    fclose(output_file);
    return 0;
}
