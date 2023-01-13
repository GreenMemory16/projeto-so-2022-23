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
static int messagesReceived;

void close_subscriber() {
    printf("Received %d messages\n", messagesReceived);
    printf("Closing subscriber...\n");
    // TODO: error handling
    close(registerPipe);
    close(clientPipe);
    unlink(clientPipeName);
}

int main(int argc, char **argv) {
    char *boxName;
    char *registerPipeName;

    if (argc < 3) {
        fprintf(stderr, "usage: sub <register_pipe_name> <box_name>\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, close_subscriber);

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
    printf("Registering subscriber...\n");
    printf("Subscriber name: %s\n", clientPipeName);
    printf("Box name: %s\n", boxName);

    register_packet.opcode = REGISTER_SUBSCRIBER;
    memcpy(registration_data.client_pipe, clientPipeName,
            strlen(clientPipeName) + 1);
    memcpy(registration_data.box_name, boxName,
            strlen(boxName) + 1);
    register_packet.payload.registration_data = registration_data;

    // Create pipe (and delete first if it exists)
    if (unlink(clientPipeName) != 0 && errno != ENOENT) {
        perror("Failed to delete pipe");
        return EXIT_FAILURE;
    }

    if (mkfifo(clientPipeName, 0666) != 0) {
        perror("Failed to create pipe");
        return EXIT_FAILURE;
    }

    if (write(registerPipe, &register_packet, sizeof(packet_t)) < 0) {
        perror("Failed to write to register pipe");
        return EXIT_FAILURE;
    }

    printf("Subscriber registered!\n");

    printf("Now Listening for Publisher messages\n");

    clientPipe = open(clientPipeName, O_RDONLY);

    while (true) {
        if (clientPipe < 0) {
            perror("Failed to open client pipe");
            return EXIT_FAILURE;
        }

        packet_t packet;
        if (read(clientPipe, &packet, sizeof(packet_t)) <= 0) {
            perror("Failed to read from client pipe");
            return EXIT_FAILURE;
        }

        printf("Reading from mailbox: %s\n", packet.payload.message_data.message);

        messagesReceived++;
    }

    close_subscriber();

    return 0;
}
