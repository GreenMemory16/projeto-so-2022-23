#include "logging.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "protocol.h"

#define RETRY_COUNT 10

void wait_retry() {
    struct timespec ts;
    // wait 50ms
    ts.tv_nsec = 50000000;
    nanosleep(&ts, NULL);
}

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
    int pipe;
    int i;
    for (i = 0; i < RETRY_COUNT; i++) {
        pipe = open(pipeName, mode);
        if (pipe >= 0) {
            return pipe;
        }
        WARN("Failed to open pipe, retrying...");
        wait_retry();
    }
    PANIC("Failed to open pipe");
}

void pipe_write(int pipe, packet_t *packet) {
    int i;
    for (i = 0; i < RETRY_COUNT; i++) {
        if (write(pipe, packet, sizeof(packet_t)) >= 0) {
            return;
        }
        WARN("Failed to write to pipe, retrying...");
        wait_retry();
    }
    PANIC("Failed to write to pipe");
}

packet_t pipe_read(int pipe) {
    packet_t packet;
    int i;
    for (i = 0; i < RETRY_COUNT; i++) {
        if (read(pipe, &packet, sizeof(packet_t)) >= 0) {
            return packet;
        }
        WARN("Failed to read from pipe, retrying...");
        wait_retry();
    }
    PANIC("Failed to read from pipe");
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