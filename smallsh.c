/* Sean Clayton : Operating Systems I
   Assignment 3 : smallsh
*/


// Imports
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

// Define max input size and max number of arguments (given in assignment description)
#define MAX_INPUT_SIZE 2048
#define MAX_ARG_COUNT 512

// Global variables to track foreground-only mode and the status of the last command
int is_foreground_only = 0;
int last_exit_status = 0;

// Function to handle SIGTSTP (Ctrl+Z)
void handle_SIGTSTP(int signal_number) {
	// Enter foreground only mode
    if (is_foreground_only == 0) {
        char *message = "\nEntering foreground-only mode (& is now ignored)\n";
        write(STDOUT_FILENO, message, 50); // Write the message to the standard output
        is_foreground_only = 1; // Set the flag to indicate foreground-only mode
	// Exit foreground only mode
    } else {
        char *message = "\nExiting foreground-only mode\n";
        write(STDOUT_FILENO, message, 30); // Write the message to the standard output
        is_foreground_only = 0; // Reset the flag to indicate normal mode
    }
}

// Function to handle SIGINT (Ctrl+C)
void handle_SIGINT(int signal_number) {
    // Ignore SIGINT in the shell itself
	// Function can be empty so that the SIGINT signal does not exit the shell
}

// Function to expand $$ to the process ID in the command
void expand_pid_variable(char *input, char *output) {
    char *pid_str = malloc(10 * sizeof(char)); // Allocate memory for the PID string
    sprintf(pid_str, "%d", getpid()); // Convert the PID to a string
    char *input_ptr = input;
    char *output_ptr = output;

    while (*input_ptr) {
        if (*input_ptr == '$' && *(input_ptr + 1) == '$') {
            strcpy(output_ptr, pid_str); // Replace $$ with the PID string
            output_ptr += strlen(pid_str);
            input_ptr += 2;
        } else {
            *output_ptr++ = *input_ptr++; // Copy characters one by one
        }
    }
    *output_ptr = '\0'; // Null-terminate the output string
    free(pid_str); // Free the allocated memory for PID string
}

// Function to parse the user input into arguments, input/output files, and background flag
void parse_input(char *input, char **arguments, char *input_path, char *output_path, int *is_background) {
    char *token;
    int arg_index = 0;

    // Initialize input/output paths and background flag
    input_path[0] = '\0';
    output_path[0] = '\0';
    *is_background = 0;

    token = strtok(input, " ");
    while (token != NULL && arg_index < MAX_ARG_COUNT - 1) {
		// Handles input file
        if (strcmp(token, "<") == 0) {
            token = strtok(NULL, " ");
			// Tokenize input file
            if (token != NULL) {
                strcpy(input_path, token); // Set input file path
            }
		// Handles output file
        } else if (strcmp(token, ">") == 0) {
            token = strtok(NULL, " ");
			// Tokenize output file
            if (token != NULL) {
                strcpy(output_path, token); // Set output file path
            }
        } else if (strcmp(token, "&") == 0 && strtok(NULL, " ") == NULL) {
            *is_background = 1; // Set background flag if & is at the end
		// Handles all other arguments/commands
        } else {
            arguments[arg_index] = token; // Add token to arguments array
            arg_index++; // Increment the arguments array to efficiently iterate through it later
        }
        token = strtok(NULL, " ");
    }
    arguments[arg_index] = NULL; // Null-terminate the arguments array
}

// Function to execute a command
void run_command(char **arguments, char *input_path, char *output_path, int is_background) {
    pid_t pid = fork();  // Create a new process
    int child_status;

    switch (pid) {
        case -1:
            // Fork failed
            perror("fork() failed");
            exit(1);
            break;
        case 0:
            // In the child process
            if (!is_background || is_foreground_only) {
                // Set the default action for SIGINT in the child process if it's not a background process
                struct sigaction default_SIGINT_action = {0};
                default_SIGINT_action.sa_handler = SIG_DFL; // Default action for SIGINT
                sigaction(SIGINT, &default_SIGINT_action, NULL);
            }

            // Handle input redirection
            if (input_path[0] != '\0') {
                // If an input file is specified, open it
                int input_fd = open(input_path, O_RDONLY);
                if (input_fd == -1) {
                    // Error opening input file
                    perror("Input Error");
                    exit(1);
                }
                dup2(input_fd, 0); // Redirect standard input to input file
                close(input_fd);    // Close the file descriptor
            } else if (is_background) {
                // If no input file and it's a background process, redirect input from /dev/null
                int dev_null = open("/dev/null", O_RDONLY);
                dup2(dev_null, 0); // Redirect standard input to /dev/null
                close(dev_null);   // Close the file descriptor
            }

            // Handle output redirection
            if (output_path[0] != '\0') {
                // If an output file is specified, open it
                int output_fd = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_fd == -1) {
                    // Error opening output file
                    perror("Output File Error");
                    exit(1);
                }
                dup2(output_fd, 1); // Redirect standard output to output file
                close(output_fd);    // Close the file descriptor
            } else if (is_background) {
                // If no output file and it's a background process, redirect output to /dev/null
                int dev_null = open("/dev/null", O_WRONLY);
                dup2(dev_null, 1); // Redirect standard output to /dev/null
                close(dev_null);   // Close the file descriptor
            }

            // Execute the command
            if (execvp(arguments[0], arguments) == -1) {
                // Error executing the command
                perror("File/Command Error");
                exit(1);
            }
            break;
        default:
            // In the parent process
            if (is_background && !is_foreground_only) {
                // If it's a background process, print the PID
                printf("Background pid is %d\n", pid);
                fflush(stdout);
            } else {
                // If it's a foreground process, wait for it to complete
                pid = waitpid(pid, &last_exit_status, 0);
                if (WIFSIGNALED(last_exit_status)) {
                    // If the process was terminated by a signal, print the signal number
                    printf("terminated by signal %d\n", WTERMSIG(last_exit_status));
                    fflush(stdout); // Flush buffer
                }
            }
            break;
    }
}


int main() {
	// Set up signal handlers for SIGINT and SIGTSTP
	struct sigaction SIGINT_action = {0}; // Struct to specify the SIGINT action
	struct sigaction SIGTSTP_action = {0}; // Struct to specify the SIGTSTP action

	SIGINT_action.sa_handler = handle_SIGINT; // Set the handler function for SIGINT
	sigfillset(&SIGINT_action.sa_mask); // Block all signals while the handler executes
	SIGINT_action.sa_flags = 0; // No special flags

	SIGTSTP_action.sa_handler = handle_SIGTSTP; // Set the handler function for SIGTSTP
	sigfillset(&SIGTSTP_action.sa_mask); // Block all signals while the handler executes
	SIGTSTP_action.sa_flags = 0; // No special flags

	// Apply the SIGINT action
	sigaction(SIGINT, &SIGINT_action, NULL);

	// Apply the SIGTSTP action
	sigaction(SIGTSTP, &SIGTSTP_action, NULL); 


    char *arguments[MAX_ARG_COUNT]; // Holds the command arguments
    char input_path[256]; // Holds the input file path
    char output_path[256]; // Holds the output file path
    int is_background = 0; // Flag to check if the command is to be run in the background
    char input[MAX_INPUT_SIZE]; // Holds the user input
    char expanded_input[MAX_INPUT_SIZE]; // Holds the expanded input after replacing $$ with PID

    while (1) {
		// Print command line and flush buffer
        printf(": ");
        fflush(stdout);

		// Added to ensure everything is clear and that user/script can provide input
		// Program gets stuck in an infinite loop without
        if (fgets(input, sizeof(input), stdin) == NULL) {
            clearerr(stdin);
            continue;
        }

        input[strcspn(input, "\n")] = 0; // Remove newline character

        if (input[0] == '#' || input[0] == '\0') {
            // Ignore comments and blank lines
            continue;
        }

        // Expand $$ to PID
        expand_pid_variable(input, expanded_input);

		// Parse input provided by the user/script
        parse_input(expanded_input, arguments, input_path, output_path, &is_background);

        if (arguments[0] == NULL) {
            // Empty command after parsing
            continue;
        }

		// Handles three built in commands
        if (strcmp(arguments[0], "exit") == 0) {
            // Exit the shell
            exit(0);
        } else if (strcmp(arguments[0], "cd") == 0) {
            // Change directory
            if (arguments[1] == NULL) {
                chdir(getenv("HOME")); // Change to home directory if no argument is provided
			// Change directory to provided argument
            } else {
                if (chdir(arguments[1]) != 0) {
                    perror("cd"); // Error if directory does not exist
                }
            }
        } else if (strcmp(arguments[0], "status") == 0) {
            // Print the last status
            if (WIFEXITED(last_exit_status)) {
                printf("exit value %d\n", WEXITSTATUS(last_exit_status)); // Exit value
            } else {
                printf("terminated by signal %d\n", WTERMSIG(last_exit_status)); // Terminated by signal
            }
            fflush(stdout); // Flush the stdout buffer to print to the screen
        } else {
            // Execute other commands
            run_command(arguments, input_path, output_path, is_background);
        }

        // Check for any completed background processes
        while ((is_background = waitpid(-1, &last_exit_status, WNOHANG)) > 0) {
			// Background progress is done
            printf("Background pid %d is done: ", is_background);
			// Process exited with an exit value
            if (WIFEXITED(last_exit_status)) {
                printf("exit value %d\n", WEXITSTATUS(last_exit_status));
			// Process terminated by a signal
            } else {
                printf("terminated by signal %d\n", WTERMSIG(last_exit_status));
            }
		}
	}
}