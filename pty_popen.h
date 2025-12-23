
#ifndef POPEN2_H
#define POPEN2_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <termios.h>
#if defined(__APPLE__) && defined(__MACH__)
#include <util.h>
#else
#include <pty.h>
#endif

pid_t static inline pty_popen(const char** command, int* master_fd, struct termios *term_mode, struct winsize *size) {
    if (setenv("TERM", "ansi", 1) == -1) {
        perror("setenv error");
        exit(EXIT_FAILURE);
    }

    pid_t pid = forkpty(master_fd, NULL, term_mode, size);

    if (pid == -1) {
        perror("forkpty");
        exit(1);
    }

    if (pid == 0) {
        /* child process: the PTY slave is already its stdin/stdout/stderr */
        execvp(command[0], (char * const *)command);

        /* if execvp returns, an error occurred */
        perror("execvp");
        exit(1);
    } else {
        return pid;
    }
}

#endif

