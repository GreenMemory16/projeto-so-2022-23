#include "logging.h"
#include "operations.h"
#include "protocol.h"
#include "unistd.h"
#include "utils.h"
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>


int main(int argc, char **argv) {
    char *registerPipeName;
    char *pipeName;
    char *boxName;

    if (argc < 3) {
        printf("Failed: Publisher need 3 arguments");
        return EXIT_FAILURE;
    }

    registerPipeName = argv[1];
    pipeName = argv[2];
    boxName = argv[3];

    int registerPipe = open(registerPipeName, O_WRONLY);
    if (registerPipe < 0) {
        perror("Failed to open register pipe");
        return EXIT_FAILURE;
    }

    packet_t register_packet;
    register_packet.opcode = REGISTER_PUBLISHER;
    memcpy(register_packet.client_pipe, pipeName, strlen(pipeName) + 1);
    memcpy(register_packet.box_name, boxName, strlen(boxName) + 1);

    // Create pipe (and delete first if it exists)
    if (unlink(pipeName) != 0 && errno != ENOENT) {
        perror("Failed to delete pipe");
        return EXIT_FAILURE;
    }

    if (mkfifo(pipeName, 0666) < 0) {
        perror("Failed to create pipe");
        return EXIT_FAILURE;
    }

    if (write(registerPipe, &register_packet, sizeof(packet_t)) < 0) {
        perror("Failed to write to register pipe");
        return EXIT_FAILURE;
    }

    // send new message for every new line until EOF is reached
    // char buffer[MESSAGE_SIZE + 1];
    // while (fgets(buffer, MESSAGE_SIZE + 1, stdin) != NULL) {
    //     int pipe = open(pipeName, O_WRONLY);
    //     if (pipe < 0) {
    //         perror("Failed to open pipe");
    //         return EXIT_FAILURE;
    //     }

    //     packet_t packet;
    //     packet.opcode = PUBLISH_MESSAGE;
    //     memcpy(packet.box_name, boxName, strlen(boxName) + 1);
    //     memcpy(packet.message, buffer, strlen(buffer) + 1);

    //     if (write(pipe, &packet, sizeof(packet_t)) < 0) {
    //         perror("Failed to write to pipe");
    //         return EXIT_FAILURE;
    //     }

    //     if (close(pipe) < 0) {
    //         perror("Failed to close pipe");
    //         return EXIT_FAILURE;
    //     }
    // }

    // TODO: close and delete pipe
    return 0;
}
