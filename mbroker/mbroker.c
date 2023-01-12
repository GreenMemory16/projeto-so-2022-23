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

ssize_t try_read(int fd, void *buf, size_t count) {
    ssize_t bytes_read;
    do {
        bytes_read = read(fd, buf, count);
    } while (bytes_read < 0 && errno == EINTR);
    return bytes_read;
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
                printf("Reading %s\n", new_packet.payload.message_data.message);
            }
            break;
        }
        case REGISTER_SUBSCRIBER: {
            printf("Thread Reading command 2\n");
            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            printf("%s\n", pipeName);

            // packet_t new_packet;
            // new_packet.opcode = SEND_MESSAGE;

            // open client pipe
            // int pipe = open(pipeName, O_WRONLY);

            // open TFS box
            printf("Opening box %s", payload.box_name);
            int box = tfs_open(payload.box_name, 0);
            printf("Box: %d", box);

            // get all messages from box
            char buffer[MESSAGE_SIZE];
            while (tfs_read(box, buffer, MESSAGE_SIZE) > 0) {
                printf("Reading %s\n", buffer);
                // if (write(pipe, &packet, sizeof(packet_t)) < 0) {
                //     perror("Failed to write to pipe");
                // }
            }

            // TODO: send every message written to box until now
            // packet_t new_packet;

            // listen for new messages by publisher

            break;
        }
        case CREATE_MAILBOX: {
            printf("Creating Mailbox\n");

            registration_data_t payload = packet.payload.registration_data;
            char *pipeName = payload.client_pipe;

            int pipe = open(pipeName, O_WRONLY);

            // int box = tfs_open(payload.box_name, TFS_O_EXISTS);

            // char *message;
            packet_t new_packet;
            new_packet.opcode = CREATE_MAILBOX_ANSWER;

            // if (box == -1) {
            //     // Box already exists
            //     // Sends "ERROR" message to manager
            //     message = "ERROR: box already exists\n";
            //     new_packet.payload.answer_data.return_code = -1;
            //     memcpy(new_packet.payload.answer_data.error_message,
            //         message, strlen(message)+1);

            //     write(pipe, &new_packet, sizeof(packet_t));
            // }

            // Sends "OK" message to manager
            new_packet.payload.answer_data.return_code = 0;
            write(pipe, &new_packet, sizeof(packet_t));

            // Creates Mailbox
            int box = tfs_open(payload.box_name, TFS_O_CREAT);
            if (box == -1) {
                fprintf(stderr, "Failed to create box\n");
            }

            break;
        }
        case REMOVE_MAILBOX: {
            printf("Thread Reading command 5\n");
            break;
        }
        case LIST_MAILBOXES: {
            printf("Thread Reading command 7\n");
            break;
        }
        default: {
            break;
        }
        }

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
    if (tfs_init(NULL) != 0) {
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
