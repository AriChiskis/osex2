#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>

// Function Prototypes
int prepare(void);
int process_arglist(int count, char** arglist);
int finalize(void);
int is_background_command(int count, char** arglist);
void execute_command(char** arglist, int background);

// Ignore SIGINT in the parent process
int prepare(void) {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN; // Ignore signal
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to ignore SIGINT");
        return -1;
    }
    return 0;
}

// Determine if the command should run in the background
int is_background_command(int count, char** arglist) {
    if (count > 0 && strcmp(arglist[count - 1], "&") == 0) {
        arglist[count - 1] = NULL; // Remove "&" from arglist
        return 1; // True for background command
    }
    return 0; // False for foreground command
}

// Execute the given command, taking into account background execution
void execute_command(char** arglist, int background) {
    pid_t pid = fork();

    if (pid == 0) { // Child process
        // Child processes should not ignore SIGINT
        signal(SIGINT, SIG_DFL);
        if (execvp(arglist[0], arglist) == -1) {
            perror("execvp failed");
            exit(EXIT_FAILURE);
        }
    } else if (pid > 0) { // Parent process
        if (!background) {
            int status;
            waitpid(pid, &status, 0); // Wait for the child process to complete
        } else {
            printf("Started background job %d\n", pid);
        }
    } else {
        perror("fork failed");
        exit(EXIT_FAILURE);
    }
}

// Main function to process command line arguments
int process_arglist(int count, char** arglist) {
    int background = is_background_command(count, arglist);
    execute_command(arglist, background);
    return 1; // Indicate to continue the loop
}

// Restore any modified signal handlers if needed
int finalize(void) {
    // Placeholder for any cleanup needed before shell exit
    return 0;
}

// Add your main function that calls prepare, repeatedly calls process_arglist for each command, and calls finalize at the end.
