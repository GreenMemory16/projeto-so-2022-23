#include "../producer-consumer/producer-consumer.h"
#include "logging.h"
#include "operations.h"
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
static int tfs_index = 0;
static tfs_file file_list[MAX_FILES];

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

int search_array(char *box_name) {
    for (int i = 0; i <= tfs_index; i++) {
        if (strcmp(box_name, file_list->box_name) == 0) {
            return i;
        }
    }

    return 0;
}

void *session_worker(void *args) {
    worker_t *worker = (worker_t *)args;
    while (true) {
        sem_wait(&hasNewMessage);
        packet_t packet = *(packet_t *)pcq_dequeue(&queue);
        // pthread_mutex_lock(&worker->lock);
        // while (worker->packet.opcode == 0) {
        //     pthread_cond_wait(&worker->cond, &worker->lock);
        // }

        switch (packet.opcode) {
        case REGISTER_PUBLISHER: {
            printf("Registering Publisher\n");
            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            // open client pipe
            int pipe = open(pipeName, O_RDONLY);

            // increment box publisher
            int index = search_array(payload.box_name);
            file_list[index].n_publishers++;

            // open TFS box
            int box = tfs_open(payload.box_name, TFS_O_EXISTS);
            if (box == -1) {
                fprintf(stderr, "Failed to open box\n");
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
            file_list[index].n_publishers--;

            break;
        }
        case REGISTER_SUBSCRIBER: {
            printf("Registering subscriber\n");
            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            printf("%s\n", pipeName);

            // packet_t new_packet;
            // new_packet.opcode = SEND_MESSAGE;

            // open client pipe
            // int pipe = open(pipeName, O_WRONLY);

            // increment box subscribers
            // int index = search_array(payload.box_name);
            // file_list[index].n_subscribers++;

            // open TFS box
            printf("Opening box %s\n", payload.box_name);
            int box = tfs_open(payload.box_name, TFS_O_EXISTS);
            printf("Box: %d\n", box);

            // open pipe
            int pipe = open(pipeName, O_WRONLY);

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

            // listen for new messages by publisher

            close(pipe);
            // decrement box subscribers
            // file_list[index].n_subscribers--;

            break;
        }
        case CREATE_MAILBOX: {
            printf("Creating Mailbox\n");

            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            int pipe = open(pipeName, O_WRONLY);

            packet_t new_packet;
            new_packet.opcode = CREATE_MAILBOX_ANSWER;

            // Sends "OK" message to manager
            new_packet.payload.answer_data.return_code = 0;
            write(pipe, &new_packet, sizeof(packet_t));

            // Creates the file entry in array
            tfs_file file_entry;
            strcpy(file_entry.box_name, payload.box_name);
            file_entry.n_publishers = 0;
            file_entry.n_subscribers = 0;

            file_list[tfs_index] = file_entry;
            tfs_index++;

            // Creates Mailbox
            int box = tfs_open(payload.box_name, TFS_O_CREAT);
            if (box == -1) {
                fprintf(stderr, "Failed to create box\n");
            }

            tfs_close(box);

            close(pipe);

            break;
        }
        case REMOVE_MAILBOX: {
            printf("Removing Mailbox\n");

            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            int pipe = open(pipeName, O_WRONLY);

            packet_t new_packet;
            new_packet.opcode = CREATE_MAILBOX_ANSWER;

            // Sends "OK" message to manager
            new_packet.payload.answer_data.return_code = 0;
            write(pipe, &new_packet, sizeof(packet_t));

            // Deletes Mailbox
            if (tfs_unlink(payload.box_name) == -1) {
                fprintf(stderr, "Failed to delete box\n");
            }

            // removes tfs_file from tfs_list
            

            close(pipe);

            break;
        }
        case LIST_MAILBOXES: {
            printf("Listing Mailboxes\n");
            list_box_data_t payload = packet.payload.list_box_data;
            char *pipeName = payload.client_pipe;

            int pipe = open(pipeName, O_WRONLY);

            packet_t new_packet;
            new_packet.opcode = LIST_MAILBOXES_ANSWER;

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

            close(pipe);
            break;
        }
        default: {
            break;
        }
        }

        printf("Exiting thread\n");

        pthread_mutex_unlock(&worker->lock);
    }
}

int start_server() {
    workers = malloc(sizeof(worker_t) * maxSessions);
    for (int i = 0; i < maxSessions; ++i) {
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
    if (close(registerPipe) < 0) {
        perror("Failed to close pipe\n");
        exit(EXIT_FAILURE);
    }

    if (unlink(registerPipeName) != 0 && errno != ENOENT) {
        perror("Failed to delete pipe");
        exit(EXIT_FAILURE);
    }

    free(workers);

    printf("\nSuccessfully ended the server.\n");
    exit(status);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Failed: Couldn't start server\n");
        return EXIT_FAILURE;
    }

    signal(SIGINT, close_server);

    registerPipeName = argv[1];
    maxSessions = strtoul(argv[2], NULL, 10);
    printf("Starting server with pipe named %s\n", registerPipeName);

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

    if (unlink(registerPipeName) != 0 && errno != ENOENT) {
        perror("Failed: Couldn't delete pipes\n");
        exit(EXIT_FAILURE);
    }

    if (mkfifo(registerPipeName, 0640) != 0) {
        perror("Failed: Couldn't create pipes\n");
        exit(EXIT_FAILURE);
    }

    registerPipe = open(registerPipeName, O_RDONLY);
    if (registerPipe < 0) {
        perror("Fail: Couldn't open server pipe\n");
        unlink(registerPipeName);
        exit(EXIT_FAILURE);
    }

    while (true) {
        int tempPipe = open(registerPipeName, O_RDONLY);

        if (tempPipe < 0) {
            if (errno == ENOENT) {
                return 0;
            }
            perror("Failed to open server pipe\n");
            close_server(EXIT_FAILURE);
        }
        if (close(tempPipe) < 0) {
            perror("Failed to close pipe\n");
            close_server(EXIT_FAILURE);
        }

        packet_t packet;
        while (try_read(registerPipe, &packet, sizeof(packet_t)) > 0) {
            pcq_enqueue(&queue, &packet);
            sem_post(&hasNewMessage);
        }
    }

    return -1;
}

// void parse_register_publisher(int op_code) {
//     char pipePath[PIPE_NAME_SIZE];
//     char boxName[BOX_NAME_SIZE];

//     // read pipe path
//     ssize_t bytes_read = try_read(myPipe, pipePath, PIPE_NAME_SIZE);
//     if (bytes_read < 0) {
//         perror("Failed to read from pipe\n");
//         close_server(EXIT_FAILURE);
//     }

//     // read box name
//     bytes_read = try_read(myPipe, boxName, BOX_NAME_SIZE);
//     if (bytes_read < 0) {
//         perror("Failed to read from pipe\n");
//         close_server(EXIT_FAILURE);
//     }
// }

// void parse_list_mailboxes(int op_code) {
//     char clientNamePipePath[PIPE_NAME_SIZE];

//     // read pipe path
//     ssize_t bytes_read = try_read(myPipe, clientNamePipePath,
//     PIPE_NAME_SIZE); if (bytes_read < 0) {
//         perror("Failed to read from pipe\n");
//         close_server(EXIT_FAILURE);
//     }
// }
