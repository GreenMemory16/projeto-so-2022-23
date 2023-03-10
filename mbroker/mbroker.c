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
        LOG("Worker waiting for new message");
        packet_t packet = *(packet_t *)pcq_dequeue(&queue);
        LOG("Worker dequeued message");

        switch (packet.opcode) {
        case REGISTER_PUBLISHER: {
            // Register a publisher to a given box

            LOG("Registering Publisher");
            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            LOG("Verifying box exists");

            // Looks for the box in the list
            ListNode *node = search_node(&list, payload.box_name);

            // If the box does not exist, create it
            if (node == NULL) {
                WARN("Box does not exist");
                int pipe = pipe_open(pipeName, O_RDONLY);
                pipe_close(pipe);
                break;
            }

            // If the box already has a publisher, reject the new publisher
            if (node->file.n_publishers > 0) {
                WARN("Too many publishers");
                int pipe = pipe_open(pipeName, O_RDONLY);
                pipe_close(pipe);
                break;
            }

            // Increment number of publishers of the box
            increment_publishers(&list, payload.box_name);
            DEBUG("Publishers: %ld", node->file.n_publishers);

            LOG("Waiting to receive messages in %s", pipeName);
            int pipe = pipe_open(pipeName, O_RDONLY);

            packet_t new_packet;
            char *message;
            while (true) {
                // Reads packet from publisher
                if (try_read(pipe, &new_packet, sizeof(packet_t)) <= 0)
                    break;

                message = new_packet.payload.message_data.message;
                LOG("Opening box in TFS");

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

                node->file.box_size += strlen(message) + 1;
                if (tfs_close(box) == -1) {
                    WARN("Failed to close box");
                    break;
                }
                LOG("Writing %s", new_packet.payload.message_data.message);

                // signals condvar to wake up subscribers reading
                LOG("Waking up subscribers");
                pthread_cond_broadcast(&node->file.cond);
            }

            pipe_close(pipe);
            decrement_publishers(&list, payload.box_name);

            break;
        }
        case REGISTER_SUBSCRIBER: {
            // Register a subscriber to a given mailbox

            LOG("Registering subscriber");
            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            LOG("Verifying box exists");

            // Looks for the box in the list
            ListNode *node = search_node(&list, payload.box_name);

            // If box does not exist, sends error message
            if (node == NULL) {
                WARN("Box does not exist");
                // The pipe is opened and then closed to notify the client that
                // the box does not exist
                int pipe = pipe_open(pipeName, O_WRONLY);
                pipe_close(pipe);
                break;
            }

            LOG("Opening box in TFS");

            // Open box in TFS
            int box = tfs_open(formatBoxName(payload.box_name), 0);
            // If box creation fails, sends error message
            if (box == -1) {
                WARN("Failed to open box");
                int pipe = pipe_open(pipeName, O_WRONLY);
                pipe_close(pipe);
                break;
            }

            // Increment number of subscribers of the box
            increment_subscribers(&list, payload.box_name);

            LOG("Waiting to write messages");
            int pipe = pipe_open(pipeName, O_WRONLY);

            // Send messages to subscriber
            char buffer[MESSAGE_SIZE];
            memset(buffer, 0, MESSAGE_SIZE);
            packet_t new_packet;
            new_packet.opcode = SEND_MESSAGE;
            tfs_read(box, buffer, MESSAGE_SIZE);
            char *message = buffer;
            while (strlen(message) > 0) {
                LOG("Sending %s", message);
                memset(new_packet.payload.message_data.message, 0,
                       MESSAGE_SIZE);
                strcpy(new_packet.payload.message_data.message, message);
                pipe_write(pipe, &new_packet);
                message += strlen(message) + 1;
            }

            while (true) {
                // Wait for publisher to write to box
                pthread_mutex_lock(&node->file.lock);
                pthread_cond_wait(&node->file.cond, &node->file.lock);
                pthread_mutex_unlock(&node->file.lock);
                LOG("Subscriber woken up");
                if (tfs_read(box, buffer, MESSAGE_SIZE) == -1) {
                    WARN("Failed to read from box");
                    break;
                }

                LOG("Sending %s", message);
                memset(new_packet.payload.message_data.message, 0,
                       MESSAGE_SIZE);
                strcpy(new_packet.payload.message_data.message, buffer);
                if (write(pipe, &new_packet, sizeof(packet_t)) == -1) {
                    break;
                }
            }
            decrement_subscribers(&list, payload.box_name);

            if (tfs_close(box) == -1) {
                WARN("Failed to close box");
                pipe_close(pipe);
                break;
            }
            pipe_close(pipe);

            break;
        }
        case CREATE_MAILBOX: {
            // Creates mailbox in TFS

            LOG("Creating Mailbox");

            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            int pipe = pipe_open(pipeName, O_WRONLY);

            // Creates packet to send to client
            packet_t new_packet;
            new_packet.opcode = CREATE_MAILBOX_ANSWER;

            LOG("Checking if box already exists");

            // Checks if box already exists
            if (search_node(&list, payload.box_name) != NULL) {
                WARN("Box already exists");
                new_packet.payload.answer_data.return_code = -1;
                strcpy(new_packet.payload.answer_data.error_message,
                       "Box already exists");
                pipe_write(pipe, &new_packet);
                pipe_close(pipe);
                break;
            }

            LOG("Creating box in TFS");

            // Creates Mailbox
            int box = tfs_open(formatBoxName(payload.box_name), TFS_O_CREAT);
            // If box creation fails, sends error message
            if (box == -1) {
                WARN("Failed to create box");
                new_packet.payload.answer_data.return_code = -1;
                strcpy(new_packet.payload.answer_data.error_message,
                       "Failed to create box");
                pipe_write(pipe, &new_packet);
                pipe_close(pipe);
                break;
            }

            LOG("Adding box to list");

            // Initializes new file and adds it to list
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
            // Removes mailbox from tfs

            LOG("Removing Mailbox");

            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            int pipe = pipe_open(pipeName, O_WRONLY);

            // Creates packet to send to client
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

            // Removes tfs_file from tfs_list
            ListNode *prev = search_prev_node(&list, payload.box_name);
            // If the node is not the head
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
            // Sends all existing mailboxes to manager

            LOG("Listing Mailboxes");
            list_box_data_t payload = packet.payload.list_box_data;
            char *pipeName = payload.client_pipe;

            int pipe = pipe_open(pipeName, O_WRONLY);

            // Creates packet to send to manager
            packet_t new_packet;
            new_packet.opcode = LIST_MAILBOXES_ANSWER;

            // If the list is empty, send a packet with last = 1
            if (list.size == 0) {
                new_packet.payload.mailbox_data.last = 1;
                memset(new_packet.payload.mailbox_data.box_name, 0,
                       sizeof(new_packet.payload.mailbox_data.box_name));
                pipe_write(pipe, &new_packet);
            } else {
                // Send packet for each mailbox
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

                    // if it is the last message, set last = 1
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

        LOG("Worker finished");
    }
}

void close_server(int status) {
    list_destroy(&list);

    pipe_close(registerPipe);
    pipe_destroy(registerPipeName);

    free(workers);

    LOG("Successfully ended the server.");
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

    LOG("Starting server with pipe named %s", registerPipeName);

    // Initialize file list and queue
    list_init(&list);
    pcq_create(&queue, maxSessions);

    // Initialize workers
    workers = malloc(sizeof(pthread_t) * maxSessions);
    for (int i = 0; i < maxSessions; ++i) {
        pthread_create(&workers[i], NULL, session_worker, NULL);
    }

    // Start TFS filesystem
    if (tfs_init(&params) != 0) {
        WARN("Failed to init tfs");
        return EXIT_FAILURE;
    }

    // Creates and open registration server pipe
    pipe_create(registerPipeName);
    registerPipe = pipe_open(registerPipeName, O_RDONLY);

    // Main loop
    // Waits for new packets and adds them to the queue
    while (true) {
        int tempPipe = pipe_open(registerPipeName, O_RDONLY);
        pipe_close(tempPipe);

        packet_t packet;
        while (try_read(registerPipe, &packet, sizeof(packet_t)) > 0) {
            LOG("Received packet with opcode %d", packet.opcode);
            pcq_enqueue(&queue, &packet);
        }
    }

    return -1;
}
