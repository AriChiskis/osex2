//
// Created by ari on 1/31/24.
//
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include "myshell.h"
int prepare(void) {
    struct sigaction sa;

    // Configure the shell to ignore SIGINT
    sa.sa_handler = SIG_IGN; // Ignore SIGINT
    sigemptyset(&sa.sa_mask); // No additional signals to block
    sa.sa_flags = 0; // No special flags

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Failed to ignore SIGINT");
        return 1; // Return an error code if sigaction fails
    }

    // Optionally, set up SIGCHLD handling to automatically reap zombie processes
    sa.sa_handler = SIG_DFL; // Default action for SIGCHLD
    sa.sa_flags = SA_NOCLDSTOP | SA_RESTART; // Don't notify for stopped children, restart syscalls if possible

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Failed to set default SIGCHLD handler");
        return 1; // Return an error code if sigaction fails
    }

    return 0; // Success
}


// arglist - a list of char* arguments (words) provided by the user
// it contains count+1 items, where the last item (arglist[count]) and *only* the last is NULL
// RETURNS - 1 if should continue, 0 otherwise
int process_arglist(int count, char **arglist) {
    pid_t pid;
    int status;

    // Fork a new process
    pid = fork();

    if (pid == -1) {
        // If fork fails, print an error message and return 1 to continue processing commands
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // Child process
        // Execute the command with execvp. If execvp fails, print an error message and exit the child process
        if (execvp(arglist[0], arglist) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process
        // Wait for the child process to finish execution
        do {
            waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }

    // Return 1 to indicate that the shell should continue processing commands
    return 1;
}


// prepare and finalize calls for initialization and destruction of anything required
int finalize(void) {
    // Perform any necessary cleanup before exiting the shell.
    // Since there's no specific cleanup required for the base case, just return 0.
    return 0;
}

