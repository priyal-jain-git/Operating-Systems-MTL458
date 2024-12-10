#include <stdio.h>      // Standard I/O functions (e.g., printf, fgets)
#include <stdlib.h>     // Standard library functions (e.g., getenv, exit)
#include <string.h>     // String manipulation functions (e.g., strtok, strcpy)
#include <unistd.h>     // UNIX standard functions (e.g., fork, execvp, chdir)
#include <sys/wait.h>   // Wait for process termination (e.g., wait)


#define MAX_INPUT_LENGTH 2048  // Maximum length of user input
#define HISTORY_SIZE 2048      // Size of the command history
#define MAX_ARGS 100            // Maximum number of arguments for a command


// Greeting shell interface on starting, commented to avoid test script failure
void init_shell()
{
    printf("\n\t************************************************************\n");
    printf("\t\tMTL458 Shell by Priyal Jain");
    char* username = getenv("USER");
    printf("\n\t\tWelcome @%s", username);
    printf("\n\t************************************************************\n");
}

// Array to store command history
char history[HISTORY_SIZE][MAX_INPUT_LENGTH];
int history_count = 0;  // Counter for the number of commands in history

// Function to add a command to history
void add_to_history(const char* command) {
    // Copy the command to the history array at the current index, wrapping around if needed
    strncpy(history[history_count % HISTORY_SIZE], command, MAX_INPUT_LENGTH);
    history_count++;  // Increment history counter
}

// Function to display the command history
void show_history(int limit) {
    // Calculate the starting index for displaying history
    int start = (history_count > limit) ? history_count - limit : 0;
    for (int i = start; i < history_count; i++) {
        printf("%s\n", history[i]);  // Print each command
    }
}


// Function to parse a command string into arguments
int parse_command(char* input, char** args, const char* delimiter) {
    int count = 0;              // Counter for the number of arguments
    int in_quotes = 0;         // Flag to track if we are inside quotes
    char* start = input;       // Start of the current argument

    // Iterate through each character in the input string
    for (char* p = input; *p; p++) {
        // Toggle the in_quotes flag if a quote is encountered
        if (*p == '"') {
            in_quotes = !in_quotes;
        }

        // If not inside quotes and the current character is a delimiter
        if (!in_quotes && strchr(delimiter, *p)) {
            *p = '\0';         // Replace delimiter with null terminator
            if (start != p) {  // Check if there is a valid argument to add
                args[count++] = start; // Store the token in the args array
                if (count >= MAX_ARGS) break; // Stop if the maximum number of arguments is reached
            }
            start = p + 1;     // Move start to the next position after the delimiter
        }
    }

    // Add the last argument if there's any text left and we haven't reached the max count
    if (*start && count < MAX_ARGS) {
        args[count++] = start;
    }

    // NULL-terminate the args array
    args[count] = NULL;

    // Remove surrounding quotes from arguments
    if (strcmp(args[0], "echo") != 0) {
    for (int i = 0; i < count; i++) {
        int len = strlen(args[i]);
        // If the argument starts and ends with a quote, remove the quotes
        if (len >= 2 && args[i][0] == '"' && args[i][len-1] == '"') {
            args[i][len-1] = '\0'; // Remove the ending quote
            args[i]++;             // Skip the starting quote
        }
    }
    }
    

    return count;  // Return the number of arguments
}

// Function to handle built-in commands like cd, history, echo, and exit
int handle_builtin_commands(char** args) {
    if (strcmp(args[0], "cd") == 0) {
        const char* path;
        char expanded_path[MAX_INPUT_LENGTH];

        if (args[1] == NULL || strcmp(args[1], "~") == 0) {
            // Change to the home directory
            path = getenv("HOME");
        } else if (strcmp(args[1], "-") == 0) {
            // Change to the previous directory
            path = getenv("OLDPWD");
            if (path != NULL) {
                printf("%s\n", path);  // Print the previous directory path
            } else {
                printf("Invalid Command\n");
                return 1;  // Indicate failure
            }
        } else {
            // Expand ~ to home directory if present
            if (args[1][0] == '~' && (args[1][1] == '/' || args[1][1] == '\0')) {
                snprintf(expanded_path, sizeof(expanded_path), "%s%s", getenv("HOME"), args[1] + 1);
                path = expanded_path;
            }
            // Change to the specified directory
            else{
                path = args[1];
            }
        }

        if (path == NULL || chdir(path) != 0) {
            // If path is NULL or chdir fails, print an error
            printf("Invalid Command\n");
        } else {
            // Update the environment variables PWD and OLDPWD
            const char* oldpwd = getenv("PWD");
            if (oldpwd != NULL) {
                setenv("OLDPWD", oldpwd, 1);  // Set OLDPWD to the current directory
            }
            char newpwd[MAX_INPUT_LENGTH];
            if (getcwd(newpwd, sizeof(newpwd)) != NULL) {
                setenv("PWD", newpwd, 1);  // Set PWD to the new directory
            }
        }
        return 1;  // Indicate that a built-in command was handled
    } else if (strcmp(args[0], "history") == 0) {
        if (args[1] != NULL) {
            if (strcmp(args[1], "-c") == 0) {
                // Clear the command history if '-c' option is provided
                history_count = 0;
            } else if (atoi(args[1]) > 0) {
                // Display the command history with a limit
                int limit = atoi(args[1]);
                show_history(limit);
            } else {
                // Invalid argument provided
                printf("Invalid Command\n");
            }
        } else {
            // Display the full command history if no argument is provided
            show_history(HISTORY_SIZE);
        }
        return 1;
    } else if (strcmp(args[0], "help") == 0) {
    // Display help information

    printf("\n\t************************************************************\n");
    printf("\t\tMTL458 Shell by Priyal Jain - Available Commands\n\n");

    // Custom Built-in Commands
    printf("Built-in Commands:\n");
    printf("  cd [dir]        : Change the current directory to [dir]. \n\t\t\tUse 'cd -' or 'cd ..' to go to the previous directory. \n\t\t\tUse 'cd ~' or 'cd' to go to the home directory.\n");
    printf("  history [n]     : Display the last [n] commands in the command history.\n");
    printf("  history -c      : Clear the command history.\n");
    printf("  exit            : Exit the shell.\n");
    printf("  help            : Display this help message.\n\n");

    // Standard Linux Commands
    printf("Standard Linux Commands:\n");
    printf("  ls [dir]        : List directory contents.\n");
    printf("  pwd             : Print the current working directory.\n");
    printf("  cat [file]      : Display the contents of [file].\n");
    printf("  sleep [number][suffix] : Pause for [number] seconds. [suffix] may be 's',' m', 'h', 'd'.\n");
    printf("  echo [text]     : Display [text].\n");
    printf("  clear           : Clear the terminal screen.\n");
    printf("  date            : Display the current date and time.\n");
    printf("  whoami          : Display the current username.\n");
    printf("  mkdir [dir]     : Create a new directory named [dir].\n");
    printf("  rmdir [dir]     : Remove an empty directory named [dir].\n");
    printf("  rm [file]       : Remove (delete) [file].\n");
    printf("  mv [src] [dest] : Move or rename [src] to [dest].\n");
    printf("  cp [src] [dest] : Copy [src] to [dest].\n");
    printf("  touch [file]    : Create an empty file named [file] or update its timestamp.\n");
    printf("  chmod [mode] [file] : Change the permissions of [file] to [mode].\n");
    printf("  chown [owner] [file] : Change the owner of [file] to [owner].\n");
    printf("  grep [pattern] [file]: Search for [pattern] in [file] or standard input.\n");
    printf("  find [dir] [options] : Search for files in [dir] according to [options].\n");
    printf("  wc [file]       : Print newline, word, and byte counts for [file].\n");
    printf("  head [file]     : Display the first 10 lines of [file].\n");
    printf("  tail [file]     : Display the last 10 lines of [file].\n");
    printf("  diff [file1] [file2] : Show differences between [file1] and [file2].\n");
    printf("  dd[operand]..   : Copy a file, converting and formatting according to the operands.\n");
    printf("  ps              : Display currently running processes.\n");
    printf("  df              : Display disk space usage for the filesystem.\n");
    printf("  du [dir]        : Display disk usage of [dir] and its contents.\n");
    printf("  free            : Display memory usage.\n");
    printf("  uname [options] : Print system information.\n\n");

    // Pipes
    printf("Pipes:\n");
    printf("  command1 | command2 : Pipe the output of command1 to command2.\n");

    printf("\n\t************************************************************\n");
    
    return 1;
} else if (strcmp(args[0], "exit") == 0) {
        // Exit the shell
        exit(0);
    }
    return 0;  // Indicate that this is not a built-in command
}

// Function to execute a command in a new process
void execute_command(char** args) {
    pid_t pid = fork();  // Create a new process
    if (pid == 0) {
        // In child process
        execvp(args[0], args);  // Replace the child process with the new command
        // If execvp fails, print an error and exit
        printf("Invalid Command\n");
        exit(1);
    } else if (pid > 0) {
        // In parent process
        wait(NULL);  // Wait for the child process to finish
    } else {
        // Fork failed
        printf("Invalid Command\n");
    }
}

// Function to execute commands connected by pipes
void execute_piped_commands(char* input) {
    char* commands[MAX_ARGS];
    int num_pipes = parse_command(input, commands, "|");  // Split input by pipes

    int fd[2];
    int in_fd = STDIN_FILENO;  // Initial input file descriptor

    for (int i = 0; i < num_pipes; i++) {
        pipe(fd);  // Create a pipe

        pid_t pid = fork();  // Create a new process
        if (pid == 0) {
            // In child process
            dup2(in_fd, STDIN_FILENO);  // Redirect input
            if (i < num_pipes - 1) {
                dup2(fd[1], STDOUT_FILENO);  // Redirect output
            }
            close(fd[0]);
            close(fd[1]);
            char* args[MAX_ARGS];
            parse_command(commands[i], args, " ");
            if (!handle_builtin_commands(args)) { //check if it's builtin command, if so execute it otherwise proceed
                execvp(args[0], args);  // Execute the standard linux command
                // If execvp fails, print an error and exit
                printf("Invalid Command\n");
            }
            exit(1);
        } else if (pid < 0) {
            // Fork failed
            printf("Invalid Command\n");
        } else {
            // In parent process
            close(fd[1]);  // Close the write end of the pipe
            in_fd = fd[0];  // Update input file descriptor for next command
            wait(NULL);  // Wait for the child process to finish
        }
    }
}

// Main function
int main() {

    //initialising interface for commands, commented to avoid test script failure

    // init_shell();

    char input[MAX_INPUT_LENGTH];
    while (1) {
        printf("MTL458 > ");  // Print the shell prompt
        fflush(stdout);  // Flush the output buffer to ensure prompt is displayed
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;  // Exit loop on error or end-of-file
        }
        input[strcspn(input, "\n")] = '\0';  // Remove trailing newline character

        if (input[0] == '\0') {
            continue;  // Skip empty input
        }

        add_to_history(input);  // Add command to history

        if (strchr(input, '|')) {
            // If the command contains a pipe, execute piped commands
            execute_piped_commands(input);
        } else {
            char* args[MAX_ARGS];
            parse_command(input, args, " ");  // Parse the command into arguments
            if (!handle_builtin_commands(args)) { //check if it's builtin command, if so execute it otherwise proceed
                // If not a built-in command, execute the command
                execute_command(args);
            }
        }
    }
    return 0;  // Return 0 to indicate successful completion
}