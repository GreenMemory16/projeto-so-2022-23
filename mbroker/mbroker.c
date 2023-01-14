#include "../producer-consumer/producer-consumer.h"
#include "list.h"
#include "logging.h"
#include "operations.h"
#include "pipes.h"
#include "protocol.h"
#include "pthread.h"
#include "utils.h"
#include <fcntl.h>
#include <semaphore.h>
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
static char *registerPipeName;
static size_t maxSessions;
static worker_t *workers;
static sem_t hasNewMessage;
static pc_queue_t queue;
static List list;
static int n_boxes;

const tfs_params params = {
    .max_inode_count = 64,
    .max_block_count = 1024,
    .max_open_files_count = MAX_FILES,
    .block_size = 1024,
};

ssize_t try_read(int fd, void *buf, size_t count) {
    ssize_t bytes_read;
    do {
        bytes_read = read(fd, buf, count);
    } while (bytes_read < 0 && errno == EINTR);
    return bytes_read;
}

char *formatBoxName(char *boxName) {
    // add a slash to the beginning of the box name
    char *formattedBoxName = malloc(strlen(boxName) + 2);
    formattedBoxName[0] = '/';
    strcpy(formattedBoxName + 1, boxName);
    return formattedBoxName;
}

void *session_worker(void *args) {
    worker_t *worker = (worker_t *)args;
    while (true) {
        sem_wait(&hasNewMessage);
        INFO("Worker %d has new message", worker->id);
        packet_t packet = *(packet_t *)pcq_dequeue(&queue);

        switch (packet.opcode) {
        case REGISTER_PUBLISHER: {
            INFO("Registering Publisher\n");
            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            int pipe = pipe_open(pipeName, O_RDONLY);

            // increment box publisher
            ListNode *node = search_node(&list, payload.box_name);
            if (node != NULL) {
                node->file.n_publishers++;
            }

            // open TFS box
            int box = tfs_open(formatBoxName(payload.box_name), TFS_O_EXISTS);
            if (box == -1) {
                WARN("Failed to open box\n");
                pipe_close(pipe);
                break;
            }

            // wait for new message
            packet_t new_packet;
            while (try_read(pipe, &new_packet, sizeof(packet_t)) > 0) {
                // make non-utilized characters null
                size_t message_size =
                    strlen(new_packet.payload.message_data.message);
                if (tfs_write(box, new_packet.payload.message_data.message,
                              message_size) == -1) {
                    fprintf(stderr, "Failed to write to box\n");
                }
                printf("Writing %s\n", new_packet.payload.message_data.message);
            }

            // decrement box publisher
            node->file.n_publishers--;

            break;
        }
        case REGISTER_SUBSCRIBER: {
            INFO("Registering subscriber\n");
            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            int pipe = pipe_open(pipeName, O_WRONLY);

            // open TFS box
            int box = tfs_open(formatBoxName(payload.box_name), TFS_O_EXISTS);
            if (box == -1) {
                WARN("Failed to open box\n");
                pipe_close(pipe);
                break;
            }

            // increment box subscribers
            ListNode *node = search_node(&list, payload.box_name);
            if (node != NULL) {
                node->file.n_subscribers++;
            }

            // get all messages from box
            char buffer[MESSAGE_SIZE];
            while (tfs_read(box, buffer, MESSAGE_SIZE) > 0) {
                printf("Reading %s\n", buffer);
                packet_t new_packet;
                message_data_t message_payload;
                new_packet.opcode = SEND_MESSAGE;
                strcpy(message_payload.message, buffer);
                new_packet.payload.message_data = message_payload;

                // write to client pipe
                if (write(pipe, &new_packet, sizeof(packet_t)) < 0) {
                    perror("Failed to write to pipe");
                }
            }

            // TODO: listen for new messages by publisher

            pipe_close(pipe);

            // decrement box subscribers
            node->file.n_subscribers--;

            break;
        }
        case CREATE_MAILBOX: {
            INFO("Creating Mailbox\n");

            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            int pipe = pipe_open(pipeName, O_WRONLY);

            packet_t new_packet;
            new_packet.opcode = CREATE_MAILBOX_ANSWER;

            // Creates Mailbox
            int box = tfs_open(formatBoxName(payload.box_name), TFS_O_CREAT);
            if (box == -1) {
                WARN("Failed to create box\n");
                new_packet.payload.answer_data.return_code = -1;
                strcpy(new_packet.payload.answer_data.error_message,
                       "Failed to create box");
                write(pipe, &new_packet, sizeof(packet_t));
                pipe_close(pipe);
                break;
            }

            if (list_find(&list, payload.box_name) != 0) {
                WARN("Box already exists\n");
                new_packet.payload.answer_data.return_code = -1;
                strcpy(new_packet.payload.answer_data.error_message,
                       "Box already exists");
                write(pipe, &new_packet, sizeof(packet_t));
                pipe_close(pipe);
                break;
            }

            // adds file to the list
            tfs_file new_file;
            strcpy(new_file.box_name, payload.box_name);
            new_file.n_publishers = 0;
            new_file.n_subscribers = 0;

            list_add(&list, new_file);

            n_boxes++;

            // Sends "OK" message to manager
            new_packet.payload.answer_data.return_code = 0;
            write(pipe, &new_packet, sizeof(packet_t));

            tfs_close(box);
            pipe_close(pipe);

            break;
        }
        case REMOVE_MAILBOX: {
            INFO("Removing Mailbox\n");

            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            int pipe = pipe_open(pipeName, O_WRONLY);

            packet_t new_packet;
            new_packet.opcode = CREATE_MAILBOX_ANSWER;

            // Deletes Mailbox
            if (tfs_unlink(payload.box_name) == -1) {
                WARN("Failed to delete box");
                new_packet.payload.answer_data.return_code = -1;
                strcpy(new_packet.payload.answer_data.error_message,
                       "Failed to delete box");
                write(pipe, &new_packet, sizeof(packet_t));
                break;
            }

            // removes tfs_file from tfs_list
            ListNode *prev = search_prev_node(&list, payload.box_name);
            if (prev != NULL) {
                list_remove(&list, prev, prev->next);
            } else {
                list_remove(&list, NULL, list.head);
            }

            n_boxes--;

            // Sends "OK" message to manager
            new_packet.payload.answer_data.return_code = 0;
            write(pipe, &new_packet, sizeof(packet_t));

            pipe_close(pipe);

            break;
        }
        case LIST_MAILBOXES: {
            INFO("Listing Mailboxes\n");
            list_box_data_t payload = packet.payload.list_box_data;
            char *pipeName = payload.client_pipe;

            int pipe = pipe_open(pipeName, O_WRONLY);

            packet_t new_packet;
            new_packet.opcode = LIST_MAILBOXES_ANSWER;

            if (n_boxes == 0) {
                new_packet.payload.mailbox_data.last = 1;
                memset(new_packet.payload.mailbox_data.box_name, 0,
                       sizeof(new_packet.payload.mailbox_data.box_name));
                write(pipe, &new_packet, sizeof(packet_t));
            } else {
                // Send message for each mailbox
                ListNode *node = list.head;
                while (node->next != NULL) {
                    strcpy(new_packet.payload.mailbox_data.box_name,
                           node->file.box_name);
                    new_packet.payload.mailbox_data.n_publishers =
                        node->file.n_publishers;
                    new_packet.payload.mailbox_data.n_subscribers =
                        node->file.n_subscribers;
                    new_packet.payload.mailbox_data.last = 0;
                    write(pipe, &new_packet, sizeof(packet_t));
                    node = node->next;
                }
                new_packet.payload.mailbox_data.last = 1;
                strcpy(new_packet.payload.mailbox_data.box_name,
                       node->file.box_name);
                write(pipe, &new_packet, sizeof(packet_t));
            }

            // packet_t new_packet;
            // new_packet.opcode = LIST_MAILBOXES_ANSWER;
            /*
            if (tfs_index == 0) {
                new_packet.payload.mailbox_data.last = 1;
                memset(new_packet.payload.mailbox_data.box_name, 0,
                       sizeof(new_packet.payload.mailbox_data.box_name));
                write(pipe, &new_packet, sizeof(packet_t));
            } else {
                // Send message for each mailbox
                for (int i = 0; i < tfs_index; i++) {
                    strcpy(new_packet.payload.mailbox_data.box_name,
                           file_list[i].box_name);
                    new_packet.payload.mailbox_data.n_publishers =
                        file_list[i].n_publishers;
                    new_packet.payload.mailbox_data.n_subscribers =
                        file_list[i].n_subscribers;
                    new_packet.payload.mailbox_data.last =
                        (i == tfs_index ? 1 : 0);

                    new_packet.payload.mailbox_data.box_size = 0;
                    //) TODO: get size of box
                    // tfs_getsize(file_list[i].box_name);

                    write(pipe, &new_packet, sizeof(packet_t));
                }
            }
            */

            pipe_close(pipe);
            break;
        }
        default: {
            break;
        }
        }

        INFO("Worker %d finished", worker->id);
    }
}

int start_server() {
    list_init(&list);

    workers = malloc(sizeof(worker_t) * maxSessions);
    for (int i = 0; i < maxSessions; ++i) {
        workers[i].id = i;
        if (pthread_mutex_init(&workers[i].lock, NULL) != 0) {
            return -1;
        }
        if (pthread_cond_init(&workers[i].cond, NULL) != 0) {
            return -1;
        }
        if (pthread_create(&workers[i].thread, NULL, session_worker,
                           &workers[i]) != 0) {
            return -1;
        }
    }
    return 0;
}

void close_server(int status) {
    list_destroy(&list);

    pipe_close(registerPipe);
    pipe_destroy(registerPipeName);

    free(workers);

    INFO("Successfully ended the server.");
    exit(status);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: mbroker <pipename>\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, close_server);

    registerPipeName = argv[1];
    maxSessions = strtoul(argv[2], NULL, 10);
    INFO("Starting server with pipe named %s", registerPipeName);

    if (start_server() != 0) {
        printf("Failed: Couldn't start server\n");
        return EXIT_FAILURE;
    }

    // create queue
    pcq_create(&queue, maxSessions);

    // initialize semaphore
    if (sem_init(&hasNewMessage, 0, 0) != 0) {
        perror("Failed to initialize semaphore\n");
        exit(EXIT_FAILURE);
    }

    // start TFS filesystem
    if (tfs_init(&params) != 0) {
        printf("Failed to init tfs\n");
        return EXIT_FAILURE;
    }

    pipe_create(registerPipeName);
    registerPipe = pipe_open(registerPipeName, O_RDONLY);

    while (true) {
        int tempPipe = pipe_open(registerPipeName, O_RDONLY);
        pipe_close(tempPipe);

        packet_t packet;
        while (try_read(registerPipe, &packet, sizeof(packet_t)) > 0) {
            pcq_enqueue(&queue, &packet);
            sem_post(&hasNewMessage);
        }
    }

    return -1;
}
