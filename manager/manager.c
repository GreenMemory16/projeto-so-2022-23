#include "logging.h"
#include "protocol.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *registerPipeName;
static int registerPipe;
static char *clientPipeName;
static int clientPipe;

static void print_usage() {
    fprintf(stderr,
            "usage: \n"
            "   manager <register_pipe_name> <pipe_name> create <box_name>\n"
            "   manager <register_pipe_name> <pipe_name> remove <box_name>\n"
            "   manager <register_pipe_name> <pipe_name> list\n");
}

void close_manager() {
    printf("Closing subscriber...\n");
    // TODO: error handling
    close(registerPipe);
    close(clientPipe);
    unlink(clientPipeName);
}

int createBox(char *boxName) {
    packet_t packet;
    registration_data_t payload;
    packet.opcode = CREATE_MAILBOX;
    strcpy(payload.box_name, boxName);
    strcpy(payload.client_pipe, clientPipeName);
    packet.payload.registration_data = payload;

    // open register pipe
    registerPipe = open(registerPipeName, O_WRONLY);
    if (registerPipe == -1) {
        fprintf(stderr, "Error opening register pipe");
        return EXIT_FAILURE;
    }

    // write to register pipe
    if (write(registerPipe, &packet, sizeof(packet_t)) == -1) {
        fprintf(stderr, "Error writing to register pipe");
        return EXIT_FAILURE;
    }

    // Create pipe (and delete first if it exists)
    if (unlink(clientPipeName) != 0 && errno != ENOENT) {
        perror("Failed to delete pipe");
        return EXIT_FAILURE;
    }

    if (mkfifo(clientPipeName, 0666) != 0) {
        perror("Failed to create pipe");
        return EXIT_FAILURE;
    }

    // open client pipe
    printf("Opening client pipe %s\n", clientPipeName);
    clientPipe = open(clientPipeName, O_RDONLY);
    if (clientPipe == -1) {
        fprintf(stderr, "Error opening client pipe");
        return EXIT_FAILURE;
    }

    close_manager();

    return 0;
}

int main(int argc, char **argv) {
    char *operation;

    if (argc < 4) {
        print_usage();
        return EXIT_FAILURE;
    }

    signal(SIGINT, close_manager);

    registerPipeName = argv[1];
    clientPipeName = argv[2];
    operation = argv[3];

    if (strcmp(operation, "create") == 0) {
        if (argc < 5) {
            print_usage();
            return EXIT_FAILURE;
        }
        char *boxName = argv[4];
        createBox(boxName);
    } else if (strcmp(operation, "remove") == 0) {
        if (argc < 5) {
            print_usage();
            return EXIT_FAILURE;
        }
        // char *boxName = argv[4];
        WARN("Not implemented yet");
        // removeBox(registerPipeName, clientPipeName, boxName);
    } else if (strcmp(operation, "list") == 0) {
        // listBoxes(registerPipeName, clientPipeName);
        WARN("Not implemented yet");
    } else {
        print_usage();
        return EXIT_FAILURE;
    }
    return 0;
}
