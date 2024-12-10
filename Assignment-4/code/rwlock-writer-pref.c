// rwlock-writer-pref.c
// A Reader-Writer Lock with Writer Preference implementation

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>

// Define the Reader-Writer lock structure with writer preference
typedef struct _rwlock_t {
    sem_t lock;         // Protects access to reader and writer counts
    sem_t writelock;    // Ensures mutual exclusion for writers
    sem_t readlock;      // Prevents new readers from entering if a writer is waiting
    int readers;        // Current count of active readers
    int writers;        // Current count of active or waiting writers
} rwlock_t;

// Global variables
rwlock_t rwlock;        // Instance of reader-writer lock
FILE *output_file;      // File pointer for logging output
sem_t output_lock;      // Semaphore for exclusive access to the output file


//INITIALISATION FUNCTION
// Initialize the reader-writer lock and output file
void initialize_rwlock(rwlock_t *rw) {
    // Initialize the reader and writer counts
    rw->readers = 0;
    rw->writers = 0;

    // Initialize semaphores: rw->lock for reader count, rw->writelock for writer access
    sem_init(&rw->lock, 0, 1);       // Binary semaphore for reader count access
    sem_init(&rw->writelock, 0, 1);  // Binary semaphore to allow one writer at a time
    sem_init(&rw->readlock, 0, 1);      // Semaphore to block new readers if a writer is waiting

    // Initialize the output file lock
    sem_init(&output_lock, 0, 1);

    // Open the output file to log activities
    output_file = fopen("output-writer-pref.txt", "w");
    if (output_file == NULL) {
        perror("Failed to open output file");  // Print error message if file opening fails
        exit(EXIT_FAILURE);
    }
}


//READER HELPER FUNCTIONS
// Log reader activity to the output file
void log_reader_activity(int num_readers) {
    sem_wait(&output_lock);            // Lock output file access
    fprintf(output_file, "Reading,Number-of-readers-present:[%d]\n", num_readers);
    fflush(output_file);
    sem_post(&output_lock);            // Release output file lock
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

// Acquires the read lock with writer preference handling
void rwlock_acquire_readlock(rwlock_t *rw) {
    sem_wait(&rw->readlock);        // Prevents new readers from entering if a writer is waiting
    sem_wait(&rw->lock);           // Protects readers count increment
    rw->readers++;
    if (rw->readers == 1) {        // If this is the first reader
        sem_wait(&rw->writelock);  // First reader locks out writers
    }
    sem_post(&rw->lock);           // Release access to readers count
    sem_post(&rw->readlock);        // Allow other readers if no writers are waiting
}

// Releases the read lock after reading
void rwlock_release_readlock(rwlock_t *rw) {
    sem_wait(&rw->lock);           // Protect readers count decrement
    rw->readers--;
    if (rw->readers == 0) {        // If this was the last reader
        sem_post(&rw->writelock);  // Allow writers access
    }
    sem_post(&rw->lock);           // Release access to readers count
}


//WRITER HELPER FUNCTIONS
// Log writer activity to the output file
void log_writer_activity(int num_readers) {
    sem_wait(&output_lock);             // Lock output file access
    fprintf(output_file, "Writing,Number-of-readers-present:[%d]\n", num_readers);
    fflush(output_file);
    sem_post(&output_lock);             // Release output file lock
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

// Acquires the write lock, blocking new readers until writers are done
void rwlock_acquire_writelock(rwlock_t *rw) {
    sem_wait(&rw->lock);           // Protect writers count increment
    rw->writers++;
    if (rw->writers == 1) {        // If this is the first writer waiting
        sem_wait(&rw->readlock);    // Block new readers from entering
    }
    sem_post(&rw->lock);           // Release access to writers count
    sem_wait(&rw->writelock);      // Wait for exclusive write access
}

// Releases the write lock after writing
void rwlock_release_writelock(rwlock_t *rw) {
    sem_post(&rw->writelock);      // Release exclusive access
    sem_wait(&rw->lock);           // Protect writers count decrement
    rw->writers--;
    if (rw->writers == 0) {        // If no more writers are waiting
        sem_post(&rw->readlock);    // Allow new readers to enter
    }
    sem_post(&rw->lock);           // Release access to writers count
}

//MAIN READER FUNCTION
// Reader thread function: executes read operation and logs activity
void *reader(void *arg) {
    rwlock_acquire_readlock(&rwlock);  // Acquire read lock

    // Get the current number of readers
    int num_readers;
    sem_wait(&rwlock.lock);            // Safely access readers count
    num_readers = rwlock.readers;
    sem_post(&rwlock.lock);

    // Log reader activity
    log_reader_activity(num_readers);

    // Perform the read operation
    perform_read();

    rwlock_release_readlock(&rwlock);  // Release read lock
    return NULL;
}


//MAIN WRITER FUNCTION
// Writer thread function: executes write operation and logs activity
void *writer(void *arg) {
    rwlock_acquire_writelock(&rwlock);  // Acquire write lock

    // Get the current number of readers
    int num_readers;
    sem_wait(&rwlock.lock);             // Safely access readers count
    num_readers = rwlock.readers;
    sem_post(&rwlock.lock);

    // Log writer activity
    log_writer_activity(num_readers);

    // Perform the write operation
    perform_write();

    rwlock_release_writelock(&rwlock);  // Release write lock
    return NULL;
}

// Main function
int main(int argc, char **argv) {
    // Check command-line arguments for reader and writer count
    if (argc != 3) return 1;
    int n = atoi(argv[1]);
    int m = atoi(argv[2]);

    // Initialize the reader-writer lock and output file
    initialize_rwlock(&rwlock);

    pthread_t readers[n], writers[m];

    // Create reader and writer threads
    for (long i = 0; i < n; i++) pthread_create(&readers[i], NULL, reader, NULL);
    for (long i = 0; i < m; i++) pthread_create(&writers[i], NULL, writer, NULL);

    // Wait for all threads to finish
    for (int i = 0; i < n; i++) pthread_join(readers[i], NULL);
    for (int i = 0; i < m; i++) pthread_join(writers[i], NULL);

    // Close the output file after all threads complete
    fclose(output_file);
    return 0;
}
