#include "../producer-consumer/producer-consumer.h"
#include "logging.h"
#include "operations.h"
#include "protocol.h"
#include "pthread.h"
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

#define PIPE_PATH_SIZE 256
#define BOX_NAME_SIZE 32
#define ERROR_MESSAGE_SIZE 1024

typedef struct packet_t {
    int opcode;
    char client_pipe[PIPE_PATH_SIZE + 1];
    char box_name[BOX_NAME_SIZE + 1];
} packet_t;

typedef struct worker_t {
    // what else
    packet_t packet;
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} worker_t;

enum {
    REGISTER_PUBLISHER = 1,
    REGISTER_SUBSCRIBER = 2,
    CREATE_MAILBOX = 3,
    CREATE_MAILBOX_ANSWER = 4,
    REMOVE_MAILBOX = 5,
    REMOVE_MAILBOX_ANSWER = 6,
    LIST_MAILBOXES = 7,
    LIST_MAILBOXES_ANSWER = 8,
    PUBLISHER_MESSAGES = 9,
    MESSAGE_SENDER = 10
};

static int myPipe;
static char *registerPipe;
static size_t maxSessions;
static worker_t *workers;

void close_server(int status) {
    if (close(myPipe) < 0) {
        perror("Failed to close pipe\n");
        exit(EXIT_FAILURE);
    }

    if (unlink(registerPipe) != 0 && errno != ENOENT) {
        perror("Failed to delete pipe");
        exit(EXIT_FAILURE);
    }

    printf("\nSuccessfully ended the server.\n");
    exit(status);
}

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
        printf("Waiting for command\n");
        pthread_mutex_lock(&worker->lock);

        switch (worker->packet.opcode) {
        case REGISTER_PUBLISHER: {
            // parse_register_publisher(op_code);
            printf("Reading command 1\n");
            break;
        }
        case REGISTER_SUBSCRIBER: {
            printf("Reading command 2\n");
            break;
        }
        case CREATE_MAILBOX: {
            printf("Reading command 3\n");
            break;
        }
        case REMOVE_MAILBOX: {
            printf("Reading command 5\n");
            break;
        }
        case LIST_MAILBOXES: {
            printf("Reading command 7\n");
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
        printf("Creating thread\n");
        pthread_mutex_init(&workers[i].lock, NULL);
        pthread_cond_init(&workers[i].cond, NULL);
        pthread_create(&workers[i].thread, NULL, session_worker, &workers[i]);
    }
    return 0;
}

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Failed: Couldn't start server\n");
        return EXIT_FAILURE;
    }


    maxSessions = strtoul(argv[2], NULL, 10);
    registerPipe = argv[1];
    printf("Starting server with pipe named %s\n", registerPipe);

    if (start_server() != 0) {
        printf("Failed: Couldn't start server\n");
        return EXIT_FAILURE;
    }

    pc_queue_t queue;
    pcq_create(&queue, maxSessions);

    // start TFS filesystem
    if (tfs_init(NULL) != 0) {
        printf("Failed to init tfs\n");
        return EXIT_FAILURE;
    }

    if (unlink(registerPipe) != 0 && errno != ENOENT) {
        perror("Failed: Couldn't delete pipes\n");
        exit(EXIT_FAILURE);
    }

    if (mkfifo(registerPipe, 0640) != 0) {
        perror("Failed: Couldn't create pipes\n");
        exit(EXIT_FAILURE);
    }

    myPipe = open(registerPipe, O_RDONLY);
    if (myPipe < 0) {
        perror("Fail: Couldn't open server pipe\n");
        unlink(registerPipe);
        exit(EXIT_FAILURE);
    }

    while (true) {
        int tempPipe = open(registerPipe, O_RDONLY);

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

        int op_code;

        while (try_read(myPipe, &op_code, sizeof(int)) > 0) {
            switch (op_code) {
            case REGISTER_PUBLISHER: {
                // parse_register_publisher(op_code);
                printf("Reading command 1\n");
                break;
            }
            case REGISTER_SUBSCRIBER: {
                printf("Reading command 2\n");
                break;
            }
            case CREATE_MAILBOX: {
                printf("Reading command 3\n");
                break;
            }
            case REMOVE_MAILBOX: {
                printf("Reading command 5\n");
                break;
            }
            case LIST_MAILBOXES: {
                printf("Reading command 7\n");
                break;
            }
            default: {
                break;
            }
            }
        }
        // if (bytes_read < 0) {
        //     perror("Failed to read from pipe\n");
        //     close_server(EXIT_FAILURE);
        // }
    }

    return -1;
}

// void parse_register_publisher(int op_code) {
//     char pipePath[PIPE_PATH_SIZE];
//     char boxName[BOX_NAME_SIZE];

//     // read pipe path
//     ssize_t bytes_read = try_read(myPipe, pipePath, PIPE_PATH_SIZE);
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
//     char clientNamePipePath[PIPE_PATH_SIZE];

//     // read pipe path
//     ssize_t bytes_read = try_read(myPipe, clientNamePipePath,
//     PIPE_PATH_SIZE); if (bytes_read < 0) {
//         perror("Failed to read from pipe\n");
//         close_server(EXIT_FAILURE);
//     }
// }
