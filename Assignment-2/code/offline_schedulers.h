#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdint.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define MAX_PROCESSES 1024
#define OUTPUT_BUFFER_SIZE 1024
#define NUM_QUEUES 3

// Structure to hold process information
typedef struct {
    char *command;              // Command string to be executed
    uint64_t arrival_time;     // Time when the process arrives in the system
    uint64_t start_time;       // Time when the process starts execution
    uint64_t completion_time;  // Time when the process completes execution
    uint64_t burst_time;       // Total execution time of the process
    uint64_t turnaround_time;  // Total time from arrival to completion
    uint64_t waiting_time;     // Total waiting time of the process
    uint64_t response_time;    // Time from arrival to first execution
    uint64_t switch_time;      // Time when the process was last switched
    uint64_t switchinto_time;  // Time when the process was switched into
    int error;                 // Error status of the process execution
    pid_t pid;                 // Process ID
    char output[OUTPUT_BUFFER_SIZE]; // Buffer to capture process output
    int completed;            // Flag to indicate if the process has completed execution
    int started;              // Flag to indicate if the process has started execution
    int current_queue;        // Current queue of the process in Multi-Level Feedback Queue (MLFQ)
    uint64_t time_in_queue;   // Time the process has spent in the current queue
} Process;

// Function prototypes
void FCFS(Process p[], int n);
void RoundRobin(Process p[], int n, int quantum);
void MultiLevelFeedbackQueue(Process p[], int n, int quantum0, int quantum1, int quantum2, int boostTime);

// Function to get the current time in milliseconds
uint64_t get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL); // Get the current time of day
    return (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000; // Convert time to milliseconds
}

// Function to execute a command and manage its output and error status
int execute_command(char* command, pid_t* pid, int* pipefd) {
    int status_pipe[2]; // Pipe to communicate the status of command execution

    // Create pipes for communication
    if (pipe(pipefd) == -1 || pipe(status_pipe) == -1) {
        perror("pipe"); // Print error if pipe creation fails
        return -1;
    }

    // Fork a new process
    *pid = fork();
    if (*pid == 0) {
        // Child process
        close(pipefd[0]); // Close unused read end of pipe
        close(status_pipe[0]); // Close unused read end of status pipe
        dup2(pipefd[1], STDOUT_FILENO); // Redirect standard output to pipe
        dup2(pipefd[1], STDERR_FILENO); // Redirect standard error to pipe
        close(pipefd[1]); // Close write end of pipe

        // Count the number of arguments in the command
        int arg_count = 1;
        for (int i = 0; command[i]; i++) {
            if (command[i] == ' ') arg_count++;
        }

        // Allocate memory for arguments
        char **args = malloc((arg_count + 1) * sizeof(char*));
        if (args == NULL) {
            perror("malloc failed"); // Print error if memory allocation fails
            exit(EXIT_FAILURE);
        }

        // Parse the command string into arguments
        int i = 0;
        char *token = strtok(command, " ");
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL; // Null-terminate the argument list

        // Execute the command
        execvp(args[0], args);

        // If execvp fails, write error status to status pipe
        int error_status = 1;
        write(status_pipe[1], &error_status, sizeof(error_status)); // Write error status to pipe
        close(status_pipe[1]); // Close write end of status pipe

        free(args); // Free allocated memory for arguments
        perror("execvp failed"); // Print error if execvp fails
        exit(EXIT_FAILURE);
    } else if (*pid < 0) {
        // Fork failed
        perror("fork failed"); // Print error if fork fails
        return -1;
    } else {
        // Parent process
        close(pipefd[1]); // Close unused write end of pipe
        close(status_pipe[1]); // Close unused write end of status pipe
        fcntl(pipefd[0], F_SETFL, O_NONBLOCK); // Set read end of pipe to non-blocking mode

        // Check if the child process encountered an error
        int error_status = 0;
        struct timeval timeout;
        timeout.tv_sec = 1;  // Set timeout to 1 second
        timeout.tv_usec = 0;

        fd_set readfds;
        FD_ZERO(&readfds); // Initialize the file descriptor set
        FD_SET(status_pipe[0], &readfds); // Add status pipe to the set

        int ready = select(status_pipe[0] + 1, &readfds, NULL, NULL, &timeout); // Wait for data on status pipe
        if (ready > 0 && FD_ISSET(status_pipe[0], &readfds)) {
            read(status_pipe[0], &error_status, sizeof(error_status)); // Read error status from pipe
        } else {
            // Assume no error status if select times out
            error_status = 0;
        }

        close(status_pipe[0]); // Close read end of status pipe
        return error_status;
    }
}

// Function to duplicate commands for each process
void duplicate_commands(Process processes[], int n) {
    for (int i = 0; i < n; i++) {
        processes[i].command = strdup(processes[i].command); // Duplicate command string
        if (processes[i].command == NULL) {
            processes[i].command = strdup(" "); // Set default command if duplication fails
        }
    }
}

// Function to calculate time metrics for a process
void calculate_time_metrics(Process* process) {
    process->turnaround_time = process->completion_time - process->arrival_time; // Calculate turnaround time
    process->waiting_time = process->turnaround_time - process->burst_time; // Calculate waiting time
    process->response_time = process->start_time - process->arrival_time; // Calculate response time
}

// Function to execute processes using First-Come, First-Served (FCFS) scheduling
void FCFS(Process processes[], int n) {
    duplicate_commands(processes, n); // Duplicate commands for each process

    uint64_t current_time = 0; // Track the current time
    int pipefd[MAX_PROCESSES][2]; // Pipes for communication between processes

    // Initialize all processes
    for (int i = 0; i < n; i++) {
        processes[i].arrival_time = 0;  // Set arrival time to 0 for all processes
        processes[i].start_time = 0;    // Initialize start time
        processes[i].completion_time = 0; // Initialize completion time
        processes[i].burst_time = 0;     // Initialize burst time
        processes[i].error = 0;           // Initialize error status
        processes[i].output[0] = '\0';    // Initialize output buffer
        processes[i].completed = 0;       // Mark process as not completed
        processes[i].started = 0;         // Mark process as not started

        // Create a pipe for each process
        if (pipe(pipefd[i]) == -1) {
            perror("pipe"); // Print error if pipe creation fails
            exit(1);
        }
    }

    // Execute processes sequentially
    for (int i = 0; i < n; i++) {
        if (!processes[i].completed) {
            // Set the start time for the process
            processes[i].start_time = current_time;

            // Execute the command and measure burst time
            uint64_t start_exec = get_current_time_ms(); // Record start time of execution
            processes[i].error = execute_command(processes[i].command, &processes[i].pid, pipefd[i]);
            
            int status;
            waitpid(processes[i].pid, &status, 0);  // Wait for the command to finish
            uint64_t end_exec = get_current_time_ms(); // Record end time of execution
            
            processes[i].burst_time = end_exec - start_exec; // Calculate burst time
            current_time += processes[i].burst_time; // Update current time

            // Set completion time and calculate time metrics
            processes[i].completion_time = current_time;
            processes[i].completed = 1; // Mark process as completed
            calculate_time_metrics(&processes[i]); // Calculate time metrics

            // Read the process output
            char buffer[OUTPUT_BUFFER_SIZE];
            ssize_t bytes_read;
            while ((bytes_read = read(pipefd[i][0], buffer, sizeof(buffer) - 1)) > 0) {
                buffer[bytes_read] = '\0'; // Null-terminate the buffer
                strcat(processes[i].output, buffer); // Append buffer content to process output
            }
            close(pipefd[i][0]); // Close the read end of the pipe

            // Print process information
            printf("%s|%lu|%lu\n", processes[i].command, processes[i].start_time, processes[i].completion_time);
        }
    }

    // Write results to CSV file
    FILE *fp = fopen("result_offline_FCFS.csv", "w");
    if (fp == NULL) {
        perror("Error opening file"); // Print error if file opening fails
        exit(1);
    }

    // Write CSV header
    fprintf(fp, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");

    // Write process information to CSV
    for (int i = 0; i < n; i++) {
        fprintf(fp, "%s,%s,%s,%lu,%lu,%lu,%lu\n", 
                processes[i].command, 
                processes[i].error ? "No" : "Yes",
                processes[i].error ? "Yes" : "No",
                processes[i].burst_time, 
                processes[i].turnaround_time, 
                processes[i].waiting_time, 
                processes[i].response_time);
    }

    fclose(fp); // Close the file
}

// Function to execute processes using Round-Robin (RR) scheduling
void RoundRobin(Process processes[], int n, int quantum) {
    duplicate_commands(processes, n); // Duplicate commands for each process

    uint64_t current_time = 0; // Track the current time
    int completed = 0;         // Count of completed processes
    int pipefd[MAX_PROCESSES][2]; // Pipes for communication between processes

    // Initialize all processes
    for (int i = 0; i < n; i++) {
        processes[i].arrival_time = 0;  // Set arrival time to 0 for all processes
        processes[i].start_time = -1;    // Initialize start time
        processes[i].completion_time = 0; // Initialize completion time
        processes[i].burst_time = 0;     // Initialize burst time
        processes[i].error = 0;           // Initialize error status
        processes[i].output[0] = '\0';    // Initialize output buffer
        processes[i].completed = 0;       // Mark process as not completed
        processes[i].started = 0;         // Mark process as not started

        // Create a pipe for each process
        if (pipe(pipefd[i]) == -1) {
            perror("pipe"); // Print error if pipe creation fails
            exit(1);
        }
    }

    // Execute processes in a round-robin manner
    while (completed < n) {
        for (int i = 0; i < n; i++) {
            if (!processes[i].completed) {
                processes[i].switchinto_time = current_time; // Record start time of quantum

                if (!processes[i].started) {
                    processes[i].start_time = current_time; // Set start time for the process
                    processes[i].error = execute_command(processes[i].command, &processes[i].pid, pipefd[i]);
                    processes[i].started = 1; // Mark process as started
                } else {
                    kill(processes[i].pid, SIGCONT); // Continue the process if it was stopped
                }

                usleep(quantum * 1000); // Sleep for the quantum duration (convert milliseconds to microseconds)
                current_time += quantum; // Update current time

                int status;
                pid_t result = waitpid(processes[i].pid, &status, WNOHANG); // Non-blocking wait

                if (result == 0) {
                    // Process is still running, stop it
                    kill(processes[i].pid, SIGSTOP); // Stop the process
                    processes[i].burst_time += quantum; // Update burst time
                } else {
                    // Process finished during this quantum
                    processes[i].completed = 1; // Mark process as completed
                    completed++; // Increment completed processes count
                    processes[i].completion_time = current_time; // Set completion time
                    processes[i].burst_time += (current_time-processes[i].switchinto_time); // Update burst time
                    
                    // Read remaining output
                    char buffer[OUTPUT_BUFFER_SIZE];
                    ssize_t bytes_read;
                    while ((bytes_read = read(pipefd[i][0], buffer, sizeof(buffer) - 1)) > 0) {
                        buffer[bytes_read] = '\0'; // Null-terminate the buffer
                        strcat(processes[i].output, buffer); // Append buffer content to process output
                    }
                    close(pipefd[i][0]); // Close the read end of the pipe
                    
                    calculate_time_metrics(&processes[i]); // Calculate time metrics
                }
                processes[i].switch_time=current_time;
                // Print process information after every context switch
                printf("%s|%lu|%lu\n", processes[i].command, processes[i].switchinto_time, processes[i].switch_time);
            }
        }
    }

    // Write results to CSV file
    FILE *fp = fopen("result_offline_RR.csv", "w");
    if (fp == NULL) {
        perror("Error opening file"); // Print error if file opening fails
        exit(1);
    }

    // Write CSV header
    fprintf(fp, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");

    // Write process information to CSV
    for (int i = 0; i < n; i++) {
        fprintf(fp, "%s,%s,%s,%lu,%lu,%lu,%lu\n", 
                processes[i].command, 
                processes[i].error ? "No" : "Yes",
                processes[i].error ? "Yes" : "No",
                processes[i].burst_time, 
                processes[i].turnaround_time, 
                processes[i].waiting_time, 
                processes[i].response_time);
    }

    fclose(fp); // Close the file
}

// Function to execute processes using Multi-Level Feedback Queue (MLFQ) scheduling
void MultiLevelFeedbackQueue(Process processes[], int n, int quantum0, int quantum1, int quantum2, int S) {
    duplicate_commands(processes, n); // Duplicate commands for each process

    int quantum[3] = {quantum0, quantum1, quantum2}; // Quantum times for each queue

    uint64_t current_time = 0; // Track the current time
    int completed = 0;         // Count of completed processes
    int pipefd[MAX_PROCESSES][2]; // Pipes for communication between processes
    uint64_t last_boost_time = 0; // Track last boost time

    // Initialize all processes
    for (int i = 0; i < n; i++) {
        processes[i].arrival_time = 0;  // Set arrival time to 0 for all processes
        processes[i].start_time = -1;    // Initialize start time
        processes[i].completion_time = 0; // Initialize completion time
        processes[i].switch_time = 0;     // Initialize switch time
        processes[i].switchinto_time = -1; // Initialize time when switched into
        processes[i].burst_time = 0;     // Initialize burst time
        processes[i].error = 0;           // Initialize error status
        processes[i].output[0] = '\0';    // Initialize output buffer
        processes[i].completed = 0;       // Mark process as not completed
        processes[i].started = 0;         // Mark process as not started
        processes[i].current_queue = 0;   // Initialize queue to 0
        processes[i].time_in_queue = 0;   // Initialize time in queue

        // Create a pipe for each process
        if (pipe(pipefd[i]) == -1) {
            perror("pipe"); // Print error if pipe creation fails
            exit(1);
        }
    }

    // Execute processes using MLFQ scheduling
    while (completed < n) {
        // Boost processes to the highest priority queue after time S
        if (current_time - last_boost_time >= S) {
            
            for (int i = 0; i < n; i++) {
                if (!processes[i].completed) {
                    processes[i].current_queue = 0; // Move process to highest priority queue
                    processes[i].time_in_queue = 0; // Reset time in queue
                }
            }
            last_boost_time = current_time; // Update last boost time
        }

        for (int i = 0; i < n; i++) {
            if (!processes[i].completed) {
                processes[i].switchinto_time = current_time; // Record time when process is switched into
                int q = processes[i].current_queue; // Get the current queue of the process

                if (!processes[i].started) {
                    processes[i].start_time = current_time; // Set start time for the process
                    processes[i].error = execute_command(processes[i].command, &processes[i].pid, pipefd[i]);
                    processes[i].started = 1; // Mark process as started
                } else {
                    kill(processes[i].pid, SIGCONT); // Continue the process if it was stopped
                }

                usleep(1000 * quantum[q]); // Sleep for the quantum duration (convert milliseconds to microseconds)
                kill(processes[i].pid, SIGSTOP); // Stop the process

                uint64_t executed_time = quantum[q]; // Time quantum executed
                
                processes[i].time_in_queue += executed_time; // Update time in queue
                processes[i].burst_time += executed_time; // Update burst time
                current_time += executed_time; // Update current time

                int status;
                pid_t result = waitpid(processes[i].pid, &status, WNOHANG); // Non-blocking wait

                if (result == 0) {
                    // Process is still running, move to next queue if not in the lowest queue
                    if (processes[i].current_queue < NUM_QUEUES - 1) {
                        processes[i].current_queue++; // Move to next lower priority queue
                        processes[i].time_in_queue = 0; // Reset time in queue
                    }
                } else {
                    // Process finished during this quantum
                    processes[i].completed = 1; // Mark process as completed
                    completed++; // Increment completed processes count
                    processes[i].completion_time = current_time; // Set completion time

                    // Read remaining output
                    char buffer[OUTPUT_BUFFER_SIZE];
                    ssize_t bytes_read;
                    while ((bytes_read = read(pipefd[i][0], buffer, sizeof(buffer) - 1)) > 0) {
                        buffer[bytes_read] = '\0'; // Null-terminate the buffer
                        strcat(processes[i].output, buffer); // Append buffer content to process output
                    }
                    close(pipefd[i][0]); // Close the read end of the pipe
                    
                    calculate_time_metrics(&processes[i]); // Calculate time metrics
                }
                processes[i].switch_time=current_time;
                // Print process information after every context switch
                printf("%s|%lu|%lu\n", processes[i].command, processes[i].switchinto_time, processes[i].switch_time);

                // Boost processes to the highest priority queue after time S
                if (current_time - last_boost_time >= S) {
                    for (int i = 0; i < n; i++) {
                        if (!processes[i].completed) {
                            processes[i].current_queue = 0; // Move process to highest priority queue
                            processes[i].time_in_queue = 0; // Reset time in queue
                        }
                    }
                    last_boost_time = current_time; // Update last boost time
                }
            }
        }
    }

    // Write results to CSV file
    FILE *fp = fopen("result_offline_MLFQ.csv", "w");
    if (fp == NULL) {
        perror("Error opening file"); // Print error if file opening fails
        exit(1);
    }

    // Write CSV header
    fprintf(fp, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");

    // Write process information to CSV
    for (int i = 0; i < n; i++) {
        fprintf(fp, "%s,%s,%s,%lu,%lu,%lu,%lu\n", 
                processes[i].command, 
                processes[i].error ? "No" : "Yes",
                processes[i].error ? "Yes" : "No",
                processes[i].burst_time, 
                processes[i].turnaround_time, 
                processes[i].waiting_time, 
                processes[i].response_time);
    }

    fclose(fp); // Close the file
}


