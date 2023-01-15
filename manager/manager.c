#include "list.h"
#include "logging.h"
#include "pipes.h"
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
static List list;

static void print_usage() {
    fprintf(stderr,
            "usage: \n"
            "   manager <register_pipe_name> <pipe_name> create <box_name>\n"
            "   manager <register_pipe_name> <pipe_name> remove <box_name>\n"
            "   manager <register_pipe_name> <pipe_name> list\n");
}

void close_manager() {
    INFO("Closing manager...");
    pipe_close(registerPipe);
    pipe_close(clientPipe);
    pipe_destroy(clientPipeName);
}

void handle_response(packet_t response) {
    if (response.opcode != CREATE_MAILBOX_ANSWER &&
        response.opcode != REMOVE_MAILBOX_ANSWER &&
        response.opcode != LIST_MAILBOXES_ANSWER) {
        WARN("Unexpected response from server\n");
        return;
    }
    if (response.payload.answer_data.return_code == 0) {
        fprintf(stdout, "OK\n");
    } else if (response.payload.answer_data.return_code == -1) {
        fprintf(stdout, "ERROR %s\n",
                response.payload.answer_data.error_message);
    } else {
        PANIC("Unexpected response from server\n");
    }
}

void send_packet(packet_t packet) {
    pipe_create(clientPipeName);

    INFO("Registering pipe: %s", clientPipeName);
    registerPipe = pipe_open(registerPipeName, O_WRONLY);
    pipe_write(registerPipe, &packet);

    INFO("Waiting for confirmation");
    clientPipe = pipe_open(clientPipeName, O_RDONLY);
    handle_response(pipe_read(clientPipe));
}

int createBox(char *boxName) {
    packet_t packet;
    registration_data_t payload;
    packet.opcode = CREATE_MAILBOX;
    strcpy(payload.box_name, boxName);
    strcpy(payload.client_pipe, clientPipeName);
    packet.payload.registration_data = payload;

    send_packet(packet);

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

    send_packet(packet);

    close_manager();

    return 0;
}

int listBoxes() {
    packet_t packet;
    packet.opcode = LIST_MAILBOXES;
    list_box_data_t payload;
    strcpy(payload.client_pipe, clientPipeName);
    packet.payload.list_box_data = payload;

    pipe_create(clientPipeName);

    INFO("Registering pipe: %s", clientPipeName);

    registerPipe = pipe_open(registerPipeName, O_WRONLY);
    pipe_write(registerPipe, &packet);

    INFO("Waiting for list of boxes");
    clientPipe = pipe_open(clientPipeName, O_RDONLY);
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
        new_file.box_size = data.box_size;

        list_add(&list, new_file);

        if (data.last == 1) {
            INFO("Last Box reached");
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
