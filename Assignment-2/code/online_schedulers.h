#pragma once

// Include necessary header files for system and standard library functions
#include <stdio.h>      // For standard input/output functions
#include <stdlib.h>     // For general utilities (e.g., malloc, exit)
#include <unistd.h>     // For POSIX operating system API (e.g., fork, pipe)
#include <sys/wait.h>   // For process wait functions (e.g., waitpid)
#include <signal.h>     // For signal handling (e.g., kill)
#include <sys/time.h>   // For time functions (e.g., gettimeofday)
#include <stdbool.h>    // For boolean type (e.g., true, false)
#include <string.h>     // For string manipulation (e.g., strtok, strcpy)
#include <fcntl.h>      // For file control options (e.g., fcntl)
#include <stdint.h>     // For fixed-width integer types (e.g., uint64_t)

#define MAX_PROCESSES 100   // Maximum number of processes supported
#define COMMAND_LENGTH 1000 // Maximum length of a command string

// Global variable to keep track of the number of commands
int map_size = 0;

// Define the Process structure
typedef struct {
    char *command;              // Command to be executed
    uint64_t start_time;       // Start time of the process in milliseconds
    uint64_t burst_time;  // Completion time of the process in milliseconds
    uint64_t turnaround_time;  // Turnaround time (completion time - arrival time)
    uint64_t waiting_time;     // Waiting time (start time - arrival time)
    uint64_t response_time;    // Response time (waiting time)
    int completed;             // Flag indicating if the process is completed
    int error;                 // Flag indicating if there was an error in execution
    int command_id;            // Unique identifier for the command
} Process;

// Function prototypes
void ShortestJobFirst();
void ShortestRemainingTimeFirst();
void MultiLevelFeedbackQueue(int quantum0, int quantum1, int quantum2, int boostTime);

// Define the Command structure
typedef struct {
    char command[COMMAND_LENGTH]; // Command string
    int index;                    // Index of the command in the command_map
    uint64_t burst_time;          // Average burst time of the command
    int count;                    // Number of times the command has been executed
} Command;

// Array to store the commands
Command command_map[MAX_PROCESSES];

// Function to find the index of a command in the command map
int command_index(char *command) {
    for (int i = 0; i < map_size; i++) {
        if (strcmp(command_map[i].command, command) == 0) {
            return command_map[i].index; // Return the index if command is found
        }
    }
    return -1; // Return -1 if command is not found
}

// Function to find the burst time of a command
uint64_t command_burst_time(char *command) {
    for (int i = 0; i < map_size; i++) {
        if (strcmp(command_map[i].command, command) == 0) {
            return command_map[i].burst_time; // Return the burst time if command is found
        }
    }
    return 1000;  // Return default burst time if command is not found
}

// Function to add a new command to the command map
void append_command(char *command, int index) {
    if (map_size < MAX_PROCESSES) {
        strncpy(command_map[map_size].command, command, COMMAND_LENGTH - 1);
        command_map[map_size].command[COMMAND_LENGTH - 1] = '\0'; // Null-terminate the command string
        command_map[map_size].index = index; // Set the command index
        command_map[map_size].burst_time = 1000;  // Default burst time for new commands
        command_map[map_size].count = 0; // Initialize count
        map_size++; // Increment map size
    } else {
        fprintf(stderr, "No more space\n"); // Print error if the map is full
    }
}

// Function to get the current time in milliseconds
uint64_t get_current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL); // Get the current time
    return (uint64_t)(tv.tv_sec) * 1000 + (uint64_t)(tv.tv_usec) / 1000; // Convert to milliseconds
}

// Function to execute a command and manage pipes and process creation
int execute_command(char* command, pid_t* pid, int* pipefd) {
    int status_pipe[2];
    // Create two pipes: one for standard output and error, and one for status
    if (pipe(pipefd) == -1 || pipe(status_pipe) == -1) {
        perror("pipe"); // Print error if pipe creation fails
        return -1;
    }

    *pid = fork(); // Create a new process
    if (*pid == 0) {
        // Child process
        close(pipefd[0]); // Close the read end of the pipe
        close(status_pipe[0]); // Close the read end of the status pipe
        dup2(pipefd[1], STDOUT_FILENO); // Redirect standard output to the pipe
        dup2(pipefd[1], STDERR_FILENO); // Redirect standard error to the pipe
        close(pipefd[1]); // Close the write end of the pipe

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

        execvp(args[0], args); // Execute the command

        // If execvp fails, write error status to status pipe
        int error_status = 1;
        write(status_pipe[1], &error_status, sizeof(error_status));
        close(status_pipe[1]); // Close the write end of the status pipe

        free(args);  // Free allocated memory
        perror("execvp failed"); // Print error if execvp fails
        exit(EXIT_FAILURE);
    } else if (*pid < 0) {
        perror("fork failed"); // Print error if fork fails
        return -1;
    } else {
        // Parent process
        close(pipefd[1]); // Close the write end of the pipe
        close(status_pipe[1]); // Close the write end of the status pipe

        // Wait for the child process to finish
        int status;
        waitpid(*pid, &status, 0);  // This will block until the child completes

        if (WIFEXITED(status)) {
            // Check if the child process exited normally
            int error_status = 0;
            read(status_pipe[0], &error_status, sizeof(error_status)); // Read the error status
            close(status_pipe[0]); // Close the read end of the status pipe
            return error_status == 0 ? 0 : -1; // Return 0 if no error, -1 otherwise
        } else {
            close(status_pipe[0]); // Close the read end of the status pipe
            return -1;  // Return -1 if the child didn't exit normally
        }
    }
}

// Function to implement Shortest Job First (SJF) scheduling
void ShortestJobFirst() {
    Process processes[MAX_PROCESSES]; // Array to store processes
    int process_number = 0;          // Count of processes
    char buffer_command[2048];       // Buffer to read commands from stdin

    // Open the CSV file to write results
    FILE *fp = fopen("result_online_SJF.csv", "w");
    if (fp == NULL) {
        perror("Error opening file"); // Print error if file opening fails
        exit(1);
    }
    fprintf(fp, "Command,Finished,Error,Burst Time,Turnaround Time,Waiting Time,Response Time\n");

    fflush(fp);  // Ensure file is flushed after each write
    // Set stdin to non-blocking mode
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
    uint64_t arrival_time = get_current_time_ms(); // Record the arrival time

    while (1) {
        // Read commands from stdin
        while (fgets(buffer_command, sizeof(buffer_command), stdin)) {
            buffer_command[strcspn(buffer_command, "\n")] = 0; // Remove newline character

            // Exit condition
            if (strcmp(buffer_command, "exit") == 0) {
                fclose(fp); // Close the file
                exit(0); // Exit the program
            }

            // Store the process information
            processes[process_number].command = strdup(buffer_command); // Duplicate the command
            processes[process_number].start_time = get_current_time_ms(); // Record start time
            processes[process_number].completed = 0; // Mark as not completed
            processes[process_number].error = 0; // No error initially
            processes[process_number].command_id = process_number; // Set command ID

            // Add command to the command map if it's not already present
            if (command_index(buffer_command) == -1) {
                append_command(buffer_command, process_number);
            }

            process_number++; // Increment process number
        }

        // Find the shortest job
        int shortest_process = -1;
        uint64_t shortest_time = UINT64_MAX;
        for (int i = 0; i < process_number; i++) {
            uint64_t burst = command_burst_time(processes[i].command);
            if (!processes[i].completed && burst < shortest_time) {
                shortest_time = burst; // Update shortest time
                shortest_process = i; // Update index of shortest process
            }
        }

        if (shortest_process != -1) {
            Process *p = &processes[shortest_process];
            uint64_t start_time = get_current_time_ms(); // Record start time
            
            pid_t pid;
            int pipefd[2]; // Pipe for standard output and error
            int error_status = execute_command(p->command, &pid, pipefd); // Execute the command
            if (error_status == -1) {
                p->error = 1; // Mark process as having an error
            }
            
            uint64_t end_time = get_current_time_ms(); // Record end time
            p->burst_time = end_time - start_time; // Calculate completion time
            p->turnaround_time = end_time - p->start_time; // Calculate turnaround time
            p->waiting_time = start_time - p->start_time; // Calculate waiting time
            p->response_time = start_time - p->start_time; // Response time is same as waiting time
            p->completed = 1; // Mark process as completed

            // Print process information to stdout
            printf("%s|%lu|%lu\n", p->command, start_time - arrival_time, end_time - arrival_time);
            fflush(stdout); // Ensure stdout is flushed

            // Update the command map with new burst time
            int idx = command_index(p->command);
            if (idx != -1) {
                command_map[idx].burst_time = (command_map[idx].burst_time * command_map[idx].count +
                    p->burst_time) / (command_map[idx].count + 1);
                command_map[idx].count++;
            }

            // Write process information to the CSV file
            fprintf(fp, "%s,%s,%s,%lu,%lu,%lu,%lu\n", 
                    p->command, 
                    p->error ? "No" : "Yes", 
                    p->error ? "Yes" : "No", 
                    p->burst_time, 
                    p->turnaround_time, 
                    p->waiting_time, 
                    p->response_time);
            fflush(fp);  // Ensure file is flushed after each write
        }

        usleep(100000);  // Sleep for 100 milliseconds before checking again
    }
    fclose(fp); // Close the file
}


