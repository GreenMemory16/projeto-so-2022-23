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
#include "list.h"

static char *registerPipeName;
static int registerPipe;
static char *clientPipeName;
static int clientPipe;
static List list;

static void print_usage() {
    fprintf(stderr,
            "usage: \n"
            "   manager <register_pipe_name> <pipe_name> create <box_name>\n"
            "   manager <register_pipe_name> <pipe_name> remove <box_name>\n"
            "   manager <register_pipe_name> <pipe_name> list\n");
}

void close_manager() {
    printf("Closing manager...\n");
    // TODO: error handling
    close(registerPipe);
    close(clientPipe);
    unlink(clientPipeName);
}

void create_client_pipe() {
    // Create pipe (and delete first if it exists)
    if (unlink(clientPipeName) != 0 && errno != ENOENT) {
        perror("Failed to delete pipe");
        exit(EXIT_FAILURE);
    }

    if (mkfifo(clientPipeName, 0666) != 0) {
        perror("Failed to create pipe");
        exit(EXIT_FAILURE);
    }

    // open client pipe
    printf("Opening client pipe %s\n", clientPipeName);
    clientPipe = open(clientPipeName, O_RDONLY);
    if (clientPipe == -1) {
        fprintf(stderr, "Error opening client pipe");
        exit(EXIT_FAILURE);
    }
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

    create_client_pipe();

    // read from client pipe
    packet_t response;
    if (read(clientPipe, &response, sizeof(packet_t)) == -1) {
        fprintf(stderr, "Error reading from client pipe");
        return EXIT_FAILURE;
    }

    if (response.payload.answer_data.return_code == 0) {
        printf("Mailbox %s created\n", boxName);
    } else if (response.payload.answer_data.return_code == -1) {
        printf("Error creating mailbox %s\n", boxName);
        printf("Error message: %s\n",
               response.payload.answer_data.error_message);
    } else {
        printf("Unexpected response from server\n");
    }

    close_manager();

    return 0;
}

int removeBox(char *boxName) {
    packet_t packet;
    registration_data_t payload;
    packet.opcode = REMOVE_MAILBOX;
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

    create_client_pipe();

    // read from client pipe

    packet_t response;
    if (read(clientPipe, &response, sizeof(packet_t)) == -1) {
        fprintf(stderr, "Error reading from client pipe");
        return EXIT_FAILURE;
    }

    if (response.payload.answer_data.return_code == 0) {
        printf("Mailbox %s removed\n", boxName);
    } else if (response.payload.answer_data.return_code == -1) {
        printf("Error removing mailbox %s\n", boxName);
        printf("Error message: %s\n",
               response.payload.answer_data.error_message);
    } else {
        printf("Unexpected response from server\n");
    }

    close_manager();

    return 0;
}

int listBoxes() {
    packet_t packet;
    packet.opcode = LIST_MAILBOXES;
    list_box_data_t payload;
    strcpy(payload.client_pipe, clientPipeName);
    packet.payload.list_box_data = payload;

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

    create_client_pipe();

    list_init(&list);

    // read from client pipe
    packet_t response;
    while (read(clientPipe, &response, sizeof(packet_t)) > 0) {
        mailbox_data_t data = response.payload.mailbox_data;

        if (response.payload.mailbox_data.box_name[0] == '\0') {
            fprintf(stdout, "NO BOXES FOUND\n");
            break;
        }

        tfs_file new_file;
        strcpy(new_file.box_name, data.box_name);
        new_file.n_subscribers = data.n_subscribers;
        new_file.n_publishers = data.n_publishers;

        list_add(&list, new_file);

        if (data.last == 1) {
            printf("Last Box reached\n");
            break;
        }
    }

    list_sort(&list);

    list_print(&list);

    list_destroy(&list);

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
        char *boxName = argv[4];
        removeBox(boxName);
    } else if (strcmp(operation, "list") == 0) {
        listBoxes();
    } else {
        print_usage();
        return EXIT_FAILURE;
    }
    return 0;
}
