#include "../producer-consumer/producer-consumer.h"
#include "list.h"
#include "logging.h"
#include "operations.h"
#include "pipes.h"
#include "protocol.h"
#include "pthread.h"
#include "utils.h"
#include <fcntl.h>
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
static List list;
pthread_t *workers;
pc_queue_t queue;

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

void *session_worker() {
    while (true) {
        INFO("Worker waiting for new message");
        packet_t packet = *(packet_t *)pcq_dequeue(&queue);
        INFO("Worker dequeued message");

        switch (packet.opcode) {
        case REGISTER_PUBLISHER: {
            INFO("Registering Publisher");
            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            INFO("Verifying box exists");
            ListNode *node = search_node(&list, payload.box_name);
            if (node == NULL) {
                WARN("Box does not exist");
                int pipe = pipe_open(pipeName, O_RDONLY);
                pipe_close(pipe);
                break;
            }

            if (node->file.n_publishers > 0) {
                WARN("Too many publishers");
                int pipe = pipe_open(pipeName, O_RDONLY);
                pipe_close(pipe);
                break;
            }

            increment_publishers(&list, payload.box_name);
            DEBUG("Publishers: %ld", node->file.n_publishers);

            INFO("Waiting to receive messages in %s", pipeName);
            int pipe = pipe_open(pipeName, O_RDONLY);

            packet_t new_packet;
            char *message;
            while (true) {
                if (try_read(pipe, &new_packet, sizeof(packet_t)) <= 0) {
                    break;
                }
                message = new_packet.payload.message_data.message;
                INFO("Opening box in TFS");

                int box =
                    tfs_open(formatBoxName(payload.box_name), TFS_O_APPEND);
                if (box == -1) {
                    WARN("Failed to open box");
                    break;
                }

                if (tfs_write(box, message, strlen(message) + 1) == -1) {
                    WARN("Failed to write to box");
                    break;
                }

                // signals condvar to wake up subscribers reading
                INFO("Waking up subscribers");
                pthread_cond_broadcast(&node->file.cond);

                node->file.box_size += strlen(message) + 1;
                if (tfs_close(box) == -1) {
                    WARN("Failed to close box");
                    break;
                }
                INFO("Writing %s", new_packet.payload.message_data.message);
            }

            pipe_close(pipe);
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
                // the pipe is opened and then closed to notify the client that
                // the box does not exist
                int pipe = pipe_open(pipeName, O_WRONLY);
                pipe_close(pipe);
                break;
            }

            INFO("Opening box in TFS");
            int box = tfs_open(formatBoxName(payload.box_name), 0);
            if (box == -1) {
                WARN("Failed to open box");
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
            if (tfs_close(box) == -1) {
                WARN("Failed to close box");
                pipe_close(pipe);
                break;
            }

            while (true) {
                // wait for publisher to write to box
                pthread_mutex_lock(&node->file.lock);
                pthread_cond_wait(&node->file.cond, &node->file.lock);
                pthread_mutex_unlock(&node->file.lock);

                INFO("Opening box in TFS");
                box = tfs_open(formatBoxName(payload.box_name), 0);
                if (box == -1) {
                    WARN("Failed to open box");
                    break;
                }

                INFO("Subscriber woken up");
                if (tfs_read(box, buffer, MESSAGE_SIZE) == -1) {
                    WARN("Failed to read from box");
                    break;
                }
                if (tfs_close(box) == -1) {
                    WARN("Failed to close box");
                    break;
                }

                INFO("Sending %s", message);
                memset(new_packet.payload.message_data.message, 0,
                       MESSAGE_SIZE);
                strcpy(new_packet.payload.message_data.message, message);
                if (write(pipe, &new_packet, sizeof(packet_t)) == -1) {
                    break;
                }
            }

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
            pthread_cond_init(&new_file.cond, NULL);
            pthread_mutex_init(&new_file.lock, NULL);

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

            // broadcasts to all subscribers that the box has been deleted
            ListNode *node = search_node(&list, payload.box_name);
            if (node != NULL) {
                pthread_cond_broadcast(&node->file.cond);
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
                while (node != NULL) {
                    strcpy(new_packet.payload.mailbox_data.box_name,
                           node->file.box_name);
                    new_packet.payload.mailbox_data.n_publishers =
                        node->file.n_publishers;
                    new_packet.payload.mailbox_data.n_subscribers =
                        node->file.n_subscribers;
                    new_packet.payload.mailbox_data.box_size =
                        node->file.box_size;

                    if (node->next == NULL) {
                        new_packet.payload.mailbox_data.last = 1;
                    } else {
                        new_packet.payload.mailbox_data.last = 0;
                    }
                    pipe_write(pipe, &new_packet);
                    node = node->next;
                }
            }

            pipe_close(pipe);
            break;
        }
        default: {
            WARN("Invalid opcode");
            break;
        }
        }

        INFO("Worker finished");
    }
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
    signal(SIGPIPE, SIG_IGN);

    registerPipeName = argv[1];
    maxSessions = strtoul(argv[2], NULL, 10);

    INFO("Starting server with pipe named %s", registerPipeName);

    list_init(&list);
    pcq_create(&queue, maxSessions);

    workers = malloc(sizeof(pthread_t) * maxSessions);
    for (int i = 0; i < maxSessions; ++i) {
        pthread_create(&workers[i], NULL, session_worker, NULL);
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
        }
    }

    return -1;
}
