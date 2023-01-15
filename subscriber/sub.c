#include "logging.h"
#include "operations.h"
#include "pipes.h"
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
    LOG("Closing subscriber...");
    pipe_close(registerPipe);
    pipe_close(clientPipe);
    pipe_destroy(clientPipeName);
    exit(EXIT_SUCCESS);
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

    // checks is clienePipeName is already in use with access
    if (access (clientPipeName, F_OK) != -1) {
        WARN("Client pipe name already in use");
        exit(EXIT_FAILURE);
    }

    registerPipe = pipe_open(registerPipeName, O_WRONLY);

    packet_t register_packet;
    registration_data_t registration_data;

    LOG("Registering subscriber");

    register_packet.opcode = REGISTER_SUBSCRIBER;
    memcpy(registration_data.client_pipe, clientPipeName,
           strlen(clientPipeName) + 1);
    memcpy(registration_data.box_name, boxName, strlen(boxName) + 1);
    register_packet.payload.registration_data = registration_data;

    pipe_create(clientPipeName);

    pipe_write(registerPipe, &register_packet);

    LOG("Listening for Publisher messages");

    clientPipe = pipe_open(clientPipeName, O_RDONLY);

    while (true) {
        packet_t packet;
        ssize_t bytesRead = read(clientPipe, &packet, sizeof(packet_t));
        if (bytesRead < 0) {
            WARN("Failed to read from client pipe");
            break;
        }

        if (bytesRead == 0) {
            WARN("Client pipe closed");
            break;
        }

        fprintf(stdout, "%s\n", packet.payload.message_data.message);

        messagesReceived++;
    }

    close_subscriber();

    return 0;
}
