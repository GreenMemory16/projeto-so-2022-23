#include "logging.h"
#include "operations.h"
#include "protocol.h"
#include "unistd.h"
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

static int registerPipe;
static char *clientPipeName;
static int clientPipe;

void close_publisher() {
    printf("Closing publisher...\n");
    // TODO: error handling
    close(registerPipe);
    close(clientPipe);
    unlink(clientPipeName);
}

int main(int argc, char **argv) {
    char *registerPipeName;
    char *boxName;

    if (argc < 3) {
        printf("Failed: Publisher need 3 arguments");
        return EXIT_FAILURE;
    }

    signal(SIGINT, close_publisher);

    registerPipeName = argv[1];
    clientPipeName = argv[2];
    boxName = argv[3];

    registerPipe = open(registerPipeName, O_WRONLY);
    if (registerPipe < 0) {
        perror("Failed to open register pipe");
        return EXIT_FAILURE;
    }

    packet_t register_packet;
    printf("Registering publisher...\n");
    printf("Publisher name: %s\n", clientPipeName);
    printf("Box name: %s\n", boxName);

    register_packet.opcode = REGISTER_PUBLISHER;
    memcpy(register_packet.client_pipe, clientPipeName,
           strlen(clientPipeName) + 1);
    memcpy(register_packet.box_name, boxName, strlen(boxName) + 1);
    memset(register_packet.message, 0, MESSAGE_SIZE);

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

    // send new message for every new line until EOF is reached
    char buffer[MESSAGE_SIZE + 1];
    while (fgets(buffer, MESSAGE_SIZE + 1, stdin) != NULL) {
        printf("Sending message: %s", buffer);
        if (clientPipe < 0) {
            perror("Failed to open pipe");
            return EXIT_FAILURE;
        }

        packet_t packet;
        packet.opcode = PUBLISH_MESSAGE;
        memcpy(packet.box_name, boxName, strlen(boxName) + 1);
        memcpy(packet.message, buffer, strlen(buffer) + 1);

        if (write(clientPipe, &packet, sizeof(packet_t)) < 0) {
            perror("Failed to write to pipe");
            return EXIT_FAILURE;
        }
    }

    close_publisher();
    return 0;
}
