#include "logging.h"
#include "operations.h"
#include "pipes.h"
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
    INFO("Closing publisher...");
    pipe_close(registerPipe);
    pipe_close(clientPipe);
    pipe_destroy(clientPipeName);
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

    // checks is clienePipeName is already in use with access
    if (access (clientPipeName, F_OK) != -1) {
        WARN("Client pipe name already in use");
        exit(EXIT_FAILURE);
    }

    registerPipe = pipe_open(registerPipeName, O_WRONLY);

    packet_t register_packet;
    registration_data_t registration_data;

    INFO("Registering publisher");

    register_packet.opcode = REGISTER_PUBLISHER;
    memcpy(registration_data.client_pipe, clientPipeName,
           strlen(clientPipeName) + 1);
    memcpy(registration_data.box_name, boxName, strlen(boxName) + 1);
    register_packet.payload.registration_data = registration_data;

    pipe_create(clientPipeName);

    pipe_write(registerPipe, &register_packet);

    INFO("Opening session pipe %s", clientPipeName);
    clientPipe = pipe_open(clientPipeName, O_WRONLY);

    INFO("Waiting for user input");
    // send new message for every new line until EOF is reached
    char buffer[MESSAGE_SIZE];
    while (fgets(buffer, MESSAGE_SIZE, stdin) != NULL) {
        packet_t packet;
        message_data_t message_data;
        packet.opcode = PUBLISH_MESSAGE;
        memset(message_data.message, 0, MESSAGE_SIZE);
        // copy the message and remove the newline character
        memcpy(message_data.message, buffer, strcspn(buffer, "\n"));
        packet.payload.message_data = message_data;

        INFO("Sending message: %s", packet.payload.message_data.message);

        pipe_write(clientPipe, &packet);
    }

    close_publisher();
    return 0;
}
