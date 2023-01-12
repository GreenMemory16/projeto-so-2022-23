#include "logging.h"
#include "protocol.h"
#include <fcntl.h>
#include <string.h>

static void print_usage() {
    fprintf(stderr,
            "usage: \n"
            "   manager <register_pipe_name> <pipe_name> create <box_name>\n"
            "   manager <register_pipe_name> <pipe_name> remove <box_name>\n"
            "   manager <register_pipe_name> <pipe_name> list\n");
}

int main(int argc, char **argv) {
    char *clientPipeName;
    char *registerPipeName;
    char *operation;

    if (argc < 4) {
        print_usage();
        return EXIT_FAILURE;
    }

    registerPipeName = argv[1];
    clientPipeName = argv[2];
    operation = argv[3];

    if (strcmp(operation, "create") == 0) {
        if (argc < 5) {
            print_usage();
            return EXIT_FAILURE;
        }
        char *boxName = argv[4];
        createBox(registerPipeName, clientPipeName, boxName);
    } else if (strcmp(operation, "remove") == 0) {
        if (argc < 5) {
            print_usage();
            return EXIT_FAILURE;
        }
        char *boxName = argv[4];
        removeBox(registerPipeName, clientPipeName, boxName);
    } else if (strcmp(operation, "list") == 0) {
        listBoxes(registerPipeName, clientPipeName);
    } else {
        print_usage();
        return EXIT_FAILURE;
    }
    return 0;
}

int createBox(char *registerPipeName, char *clientPipeName, char *boxName) {
    packet_t packet;
    registration_data_t payload;
    packet.opcode = CREATE_MAILBOX;
    strcpy(payload.box_name, boxName);
    strcpy(payload.client_pipe, clientPipeName);
    packet.payload.registration_data = payload;

    // open register pipe
    int registerPipe = open(registerPipeName, O_WRONLY);
    if (registerPipe == -1) {
        fprintf(stderr, "Error opening register pipe");
        return EXIT_FAILURE;
    }

    // write to register pipe
    if (write(registerPipe, &packet, sizeof(packet_t)) == -1) {
        fprintf(stderr, "Error writing to register pipe");
        return EXIT_FAILURE;
    }

    // open client pipe
    int clientPipe = open(clientPipeName, O_RDONLY);
    if (clientPipe == -1) {
        fprintf(stderr, "Error opening client pipe");
        return EXIT_FAILURE;
    }
}