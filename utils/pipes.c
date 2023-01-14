#include "logging.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

void pipe_create(char *pipeName) {
    // Create pipe (and delete first if it exists)
    if (unlink(pipeName) != 0 && errno != ENOENT) {
        PANIC("Failed to delete existing pipe");
    }

    if (mkfifo(pipeName, 0666) < 0) {
        PANIC("Failed to create pipe");
    }
}

int pipe_open(char *pipeName, int mode) {
    int pipe = open(pipeName, mode);
    if (pipe < 0) {
        PANIC("Failed to open pipe");
    }
    return pipe;
}

void pipe_close(int pipe) {
    if (close(pipe) < 0) {
        PANIC("Failed to close pipe");
    }
}

void pipe_destroy(char *pipeName) {
    if (unlink(pipeName) != 0) {
        PANIC("Failed to delete pipe");
    }
}