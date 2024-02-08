#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h> // Required for file operations


void sigchld_handler(int signum) {
    // Wait for all children without blocking
    while (waitpid((pid_t)(-1), 0, WNOHANG) > 0) {}
}

// Ignore SIGINT in the parent process
int prepare(void) {
    struct sigaction sa_ignore, sa_sigchld;

    // Ignore SIGINT in the shell
    sa_ignore.sa_handler = SIG_IGN;
    sigemptyset(&sa_ignore.sa_mask);
    sa_ignore.sa_flags = 0;
    if (sigaction(SIGINT, &sa_ignore, NULL) == -1) {
        perror("Failed to ignore SIGINT");
        return -1;
    }

    // Handle SIGCHLD to reap zombie processes automatically
    sa_sigchld.sa_handler = sigchld_handler; // Assuming sigchld_handler is defined
    sigemptyset(&sa_sigchld.sa_mask);
    sa_sigchld.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa_sigchld, NULL) == -1) {
        perror("Failed to handle SIGCHLD");
        return -1;
    }

    return 0;
}


int find_pipe_index(char** arglist, int count) {
    for (int i = 0; i < count; ++i) {
        if (strcmp(arglist[i], "|") == 0) {
            return i;
        }
    }
    return -1; // No pipe found
}

// Determine if the command should run in the background
int is_background_command(int count, char** arglist) {
    if (count > 0 && strcmp(arglist[count - 1], "&") == 0) {
        arglist[count - 1] = NULL; // Remove "&" from arglist
        return 1; // True for background command
    }
    return 0; // False for foreground command
}


int find_input_redirection_index(char** arglist, int count) {
    for (int i = 0; i < count - 1; ++i) { // Ensure there's at least one argument after "<"
        if (strcmp(arglist[i], "<") == 0) {
            return i; // Return the index of "<"
        }
    }
    return -1; // Indicate no input redirection symbol was found
}

int find_output_redirection_index(char** arglist, int count) {
    for (int i = 0; i < count - 1; ++i) { // Ensure there's at least one argument after "<"
        if (strcmp(arglist[i], ">") == 0) {
            return i; // Return the index of "<"
        }
    }
    return -1; // Indicate no input redirection symbol was found
}

int setup_output_redirection(char** arglist) {
    for (int i = 0; arglist[i] != NULL; i++) {
        if (strcmp(arglist[i], ">") == 0 && arglist[i + 1] != NULL) {
            int fd = open(arglist[i + 1], O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
            if (fd == -1) {
                perror("open");
                return -1; // Indicate failure
            }
            arglist[i] = NULL; // Terminate the arglist here to exclude ">" and the filename from execvp
            return fd; // Return the file descriptor for redirection
        }
    }
    return -2; // Indicate no redirection symbol was found
}


//execusionss
void execute_with_input_redirection(char** arglist, int count) {
    int redirection_index = -1;
    // Find the input redirection symbol and its associated file
    for (int i = 0; i < count; i++) {
        if (strcmp(arglist[i], "<") == 0 && i < (count - 1)) {
            redirection_index = i;
            break;
        }
    }

    if (redirection_index == -1) {
        fprintf(stderr, "Input redirection symbol '<' not found.\n");
        return;
    }

    // Open the input file
    int fd = open(arglist[redirection_index + 1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return;
    }

    pid_t pid = fork();
    if (pid == 0) { // Child process
        // Redirect stdin from the input file
        if (dup2(fd, STDIN_FILENO) == -1) {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        close(fd); // Close the file descriptor as it's no longer needed

        // Set SIGINT to default action for the child process
        struct sigaction sa;
        sa.sa_handler = SIG_DFL;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }

        // Modify arglist to remove "<" and the filename, then execute the command
        arglist[redirection_index] = NULL;
        execvp(arglist[0], arglist);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // Parent process
        close(fd); // Close the input file descriptor in the parent
        int status;
        waitpid(pid, &status, 0); // Wait for the child process to complete
    } else {
        perror("fork");
        exit(EXIT_FAILURE);
    }
}


void execute_with_pipe(char** arglist, int pipe_index, int count) {
    int fd[2]; // File descriptors for the pipe
    if (pipe(fd) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    struct sigaction sa;
    sa.sa_handler = SIG_DFL; // Set default signal handling
    sigemptyset(&sa.sa_mask); // Clear any blocked signals
    sa.sa_flags = 0; // No special flags

    pid_t pid1 = fork();
    if (pid1 == 0) { // First child process
        // Apply the default signal handling for SIGINT
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }

        close(fd[0]); // Close unused read end
        dup2(fd[1], STDOUT_FILENO); // Redirect stdout to pipe write
        close(fd[1]); // Close write end after dup2

        arglist[pipe_index] = NULL; // Terminate first command list
        execvp(arglist[0], arglist);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    // Close the write end of the pipe in the parent before forking the second child
    close(fd[1]);

    pid_t pid2 = fork();
    if (pid2 == 0) { // Second child process
        // Apply the default signal handling for SIGINT
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }

        close(fd[1]); // Ensure the write end is closed
        dup2(fd[0], STDIN_FILENO); // Redirect stdin to pipe read
        close(fd[0]); // Close read end after dup2

        execvp(arglist[pipe_index + 1], &arglist[pipe_index + 1]);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    // Parent process closes the remaining end of the pipe
    close(fd[0]);

    // Wait for both child processes to complete
    int status1, status2;
    waitpid(pid1, &status1, 0);
    waitpid(pid2, &status2, 0);
}

// Execute the given command, taking into account background execution
void execute_with_output_redirection(char** arglist) {
    int fd = setup_output_redirection(arglist); // Setup redirection and modify arglist if needed

    pid_t pid = fork();

    if (pid == 0) { // Child process
        if (fd == -1) {
            // Output redirection setup failed, exit the child process
            perror("Failed to set up output redirection");
            exit(EXIT_FAILURE);
        } else if (fd >= 0) {
            // Output redirection was successful, redirect stdout
            dup2(fd, STDOUT_FILENO);
            close(fd); // Close the file descriptor as it's no longer needed
        }


        struct sigaction sa;
        sa.sa_handler = SIG_DFL; // Default handling for foreground process
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        if (sigaction(SIGINT, &sa, NULL) == -1) {
            perror("sigaction failed");
            exit(EXIT_FAILURE);
        }


        execvp(arglist[0], arglist);
        perror("execvp failed"); // Execvp only returns on error
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for child to finish

        if (fd >= 0) {
            close(fd); // Ensure the file descriptor is closed in the parent
        }
    } else {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
}


// Execute the given command, taking into account background execution
void execute_command(char** arglist, int background) {
    //int fd = setup_output_redirection(arglist); // Setup redirection and modify arglist if needed

    pid_t pid = fork();

    if (pid == 0) { // Child process
        if (!background) {
            struct sigaction sa;
            sa.sa_handler = SIG_DFL; // Default handling for foreground process
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0;
            if (sigaction(SIGINT, &sa, NULL) == -1) {
                perror("sigaction failed");
                exit(EXIT_FAILURE);
            }
        }

        execvp(arglist[0], arglist);
        perror("execvp failed"); // Execvp only returns on error
        exit(EXIT_FAILURE);
    } else if (pid > 0) { // Parent process
        if (!background) {
            int status;
            waitpid(pid, &status, 0); // Wait for child to finish
        } else {
            //printf("Started background job %d\n", pid); // this is for mentioning background processes started
        }

    } else {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
}



// Main function to process command line arguments

int process_arglist(int count, char** arglist) {
    int pipe_index = find_pipe_index(arglist, count);
    int input_redirection_index = find_input_redirection_index(arglist, count);
    int output_redirection_index = find_output_redirection_index(arglist, count);
    // Handle piping if detected
    if (pipe_index != -1) {
        execute_with_pipe(arglist, pipe_index, count);
    } else if (input_redirection_index != -1) { // Handle input redirection if detected
        execute_with_input_redirection(arglist, count);
    } else if (output_redirection_index != -1) { // Handle output redirection if detected
        execute_with_output_redirection(arglist);
    }else {
        // Handle commands without piping or input redirection
        int background = is_background_command(count, arglist);
        execute_command(arglist, background);
    }
    return 1; // Indicate to continue the loop
}



// Restore any modified signal handlers if needed
int finalize(void) {
    // Placeholder for any cleanup needed before shell exit
    return 0;
}

// Add your main function that calls prepare, repeatedly calls process_arglist for each command, and calls finalize at the end.
