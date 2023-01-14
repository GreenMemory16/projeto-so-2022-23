#include "logging.h"
#include "operations.h"
#include "protocol.h"
#include "utils.h"
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int registerPipe;
static char *clientPipeName;
static int clientPipe;

void close_publisher() {
    INFO("Closing publisher...\n");
    // TODO: error handling
    close(registerPipe);
    close(clientPipe);
    unlink(clientPipeName);
    exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
    char *registerPipeName;
    char *boxName;

    if (argc < 3) {
        fprintf(stderr, "usage: pub <register_pipe_name> <box_name>\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, close_publisher);
    signal(SIGPIPE, close_publisher);

    registerPipeName = argv[1];
    clientPipeName = argv[2];
    boxName = argv[3];

    registerPipe = open(registerPipeName, O_WRONLY);
    if (registerPipe < 0) {
        perror("Failed to open register pipe");
        return EXIT_FAILURE;
    }

    packet_t register_packet;
    registration_data_t registration_data;
    printf("Registering publisher...\n");
    printf("Publisher name: %s\n", clientPipeName);
    printf("Box name: %s\n", boxName);

    register_packet.opcode = REGISTER_PUBLISHER;
    memcpy(registration_data.client_pipe, clientPipeName,
           strlen(clientPipeName) + 1);
    memcpy(registration_data.box_name, boxName, strlen(boxName) + 1);
    register_packet.payload.registration_data = registration_data;

    // Create pipe (and delete first if it exists)
    if (unlink(clientPipeName) != 0 && errno != ENOENT) {
        perror("Failed to delete pipe");
        return EXIT_FAILURE;
    }

    if (mkfifo(clientPipeName, 0666) < 0) {
        perror("Failed to create pipe");
        return EXIT_FAILURE;
    }

    if (write(registerPipe, &register_packet, sizeof(packet_t)) < 0) {
        perror("Failed to write to register pipe");
        return EXIT_FAILURE;
    }

    printf("Publisher registered\n");

    printf("Now waiting for user input:\n");

    clientPipe = open(clientPipeName, O_WRONLY);

    // check if pipe is open
    if (clientPipe < 0) {
        perror("Failed to open pipe");
        return EXIT_FAILURE;
    }

    // send new message for every new line until EOF is reached
    char buffer[MESSAGE_SIZE + 1];
    while (fgets(buffer, MESSAGE_SIZE + 1, stdin) != NULL) {
        printf("Sending message: %s", buffer);
        if (clientPipe < 0) {
            perror("Failed to open pipe");
            return EXIT_FAILURE;
        }

        packet_t packet;
        message_data_t message_data;
        packet.opcode = PUBLISH_MESSAGE;
        memcpy(message_data.message, buffer, strlen(buffer) + 1);
        packet.payload.message_data = message_data;

        if (write(clientPipe, &packet, sizeof(packet_t)) < 0) {
            perror("Failed to write to pipe");
            return EXIT_FAILURE;
        }
    }

    close_publisher();
    return 0;
}
