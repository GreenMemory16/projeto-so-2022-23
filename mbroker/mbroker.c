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
        INFO("Worker %d waiting for new message", worker->id);
        sem_wait(&hasNewMessage);
        INFO("Worker %d has new message", worker->id);
        packet_t packet = *(packet_t *)pcq_dequeue(&queue);

        switch (packet.opcode) {
        case REGISTER_PUBLISHER: {
            INFO("Registering Publisher");
            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            INFO("Verifying box exists");
            ListNode *node = search_node(&list, payload.box_name);
            if (node == NULL) {
                WARN("Box does not exist");
                break;
            }

            if (node->file.n_publishers > 0) {
                WARN("Too many publishers");
                break;
            }

            INFO("Opening box in TFS");
            int box = tfs_open(formatBoxName(payload.box_name), TFS_O_APPEND);
            if (box == -1) {
                WARN("Failed to open box");
                break;
            }

            increment_publishers(&list, payload.box_name);
            DEBUG("Publishers: %ld", node->file.n_publishers);

            INFO("Waiting to receive messages in %s", pipeName);
            int pipe = pipe_open(pipeName, O_RDONLY);

            packet_t new_packet;
            char *message;
            while (try_read(pipe, &new_packet, sizeof(packet_t)) > 0) {
                message = new_packet.payload.message_data.message;
                if (tfs_write(box, message, strlen(message) + 1) == -1) {
                    fprintf(stderr, "Failed to write to box\n");
                }
                node->file.box_size += strlen(message) + 1;
                INFO("Writing %s", new_packet.payload.message_data.message);
            }

            decrement_publishers(&list, payload.box_name);
            break;
        }
        case REGISTER_SUBSCRIBER: {
            INFO("Registering subscriber");
            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            INFO("Verifying box exists");
            ListNode *node = search_node(&list, payload.box_name);
            if (node == NULL) {
                WARN("Box does not exist");
                break;
            }

            INFO("Opening box in TFS");
            int box = tfs_open(formatBoxName(payload.box_name), 0);
            if (box == -1) {
                WARN("Failed to open box");
                // the pipe is opened and then closed to notify the client that
                // the box does not exist
                int pipe = pipe_open(pipeName, O_WRONLY);
                pipe_close(pipe);
                break;
            }

            increment_subscribers(&list, payload.box_name);

            INFO("Waiting to write messages");
            int pipe = pipe_open(pipeName, O_WRONLY);

            // send messages to subscriber
            char buffer[MESSAGE_SIZE];
            memset(buffer, 0, MESSAGE_SIZE);
            char *message = buffer;
            packet_t new_packet;
            new_packet.opcode = SEND_MESSAGE;
            tfs_read(box, buffer, MESSAGE_SIZE);
            while (strlen(message) > 0) {
                INFO("Sending %s", message);
                memset(new_packet.payload.message_data.message, 0,
                       MESSAGE_SIZE);
                strcpy(new_packet.payload.message_data.message, message);
                pipe_write(pipe, &new_packet);
                message += strlen(message) + 1;
            }

            // TODO: listen for new messages by publisher

            pipe_close(pipe);

            decrement_subscribers(&list, payload.box_name);
            break;
        }
        case CREATE_MAILBOX: {
            INFO("Creating Mailbox");

            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            int pipe = pipe_open(pipeName, O_WRONLY);

            packet_t new_packet;
            new_packet.opcode = CREATE_MAILBOX_ANSWER;

            INFO("Checking if box already exists");

            if (search_node(&list, payload.box_name) != NULL) {
                WARN("Box already exists");
                new_packet.payload.answer_data.return_code = -1;
                strcpy(new_packet.payload.answer_data.error_message,
                       "Box already exists");
                pipe_write(pipe, &new_packet);
                pipe_close(pipe);
                break;
            }

            INFO("Creating box in TFS");

            // Creates Mailbox
            int box = tfs_open(formatBoxName(payload.box_name), TFS_O_CREAT);
            if (box == -1) {
                WARN("Failed to create box");
                new_packet.payload.answer_data.return_code = -1;
                strcpy(new_packet.payload.answer_data.error_message,
                       "Failed to create box");
                pipe_write(pipe, &new_packet);
                pipe_close(pipe);
                break;
            }

            INFO("Adding box to list");

            // adds file to the list
            tfs_file new_file;
            strcpy(new_file.box_name, payload.box_name);
            new_file.n_publishers = 0;
            new_file.n_subscribers = 0;
            new_file.box_size = 0;

            list_add(&list, new_file);

            // Sends "OK" message to manager
            new_packet.payload.answer_data.return_code = 0;
            pipe_write(pipe, &new_packet);

            tfs_close(box);
            pipe_close(pipe);

            break;
        }
        case REMOVE_MAILBOX: {
            INFO("Removing Mailbox");

            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            int pipe = pipe_open(pipeName, O_WRONLY);

            packet_t new_packet;
            new_packet.opcode = CREATE_MAILBOX_ANSWER;

            // Deletes Mailbox
            if (tfs_unlink(formatBoxName(payload.box_name)) == -1) {
                WARN("Failed to delete box");
                new_packet.payload.answer_data.return_code = -1;
                strcpy(new_packet.payload.answer_data.error_message,
                       "Failed to delete box");
                pipe_write(pipe, &new_packet);
                break;
            }

            // removes tfs_file from tfs_list
            ListNode *prev = search_prev_node(&list, payload.box_name);
            if (prev != NULL) {
                list_remove(&list, prev, prev->next);
            } else {
                list_remove(&list, NULL, list.head);
            }

            // Sends "OK" message to manager
            new_packet.payload.answer_data.return_code = 0;
            pipe_write(pipe, &new_packet);

            pipe_close(pipe);

            break;
        }
        case LIST_MAILBOXES: {
            INFO("Listing Mailboxes");
            list_box_data_t payload = packet.payload.list_box_data;
            char *pipeName = payload.client_pipe;

            int pipe = pipe_open(pipeName, O_WRONLY);

            packet_t new_packet;
            new_packet.opcode = LIST_MAILBOXES_ANSWER;

            if (list.size == 0) {
                new_packet.payload.mailbox_data.last = 1;
                memset(new_packet.payload.mailbox_data.box_name, 0,
                       sizeof(new_packet.payload.mailbox_data.box_name));
                pipe_write(pipe, &new_packet);
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
                    pipe_write(pipe, &new_packet);
                    node = node->next;
                }
                new_packet.payload.mailbox_data.last = 1;
                strcpy(new_packet.payload.mailbox_data.box_name,
                       node->file.box_name);
                pipe_write(pipe, &new_packet);
            }

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
        // if (pthread_mutex_init(&workers[i].lock, NULL) != 0) {
        //     return -1;
        // }
        // if (pthread_cond_init(&workers[i].cond, NULL) != 0) {
        //     return -1;
        // }
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
            INFO("Received packet with opcode %d", packet.opcode);
            pcq_enqueue(&queue, &packet);
            sem_post(&hasNewMessage);
        }
    }

    return -1;
}
