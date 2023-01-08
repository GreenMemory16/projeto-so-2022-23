#include "logging.h"
#include "operations.h"
#include "utils.h"
#include "unistd.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <sys/wait.h>
#include <sys/types.h>

enum {
    TFS_1 = '1',
    TFS_2 = '2',
    TFS_3 = '3',
    TFS_4 = '4',
    TFS_5 = '5',
    TFS_6 = '6',
    TFS_7 = '7',
    TFS_8 = '8',
    TFS_9 = '9',
    TFS_10 = '10'
};

static int myPipe;
static char *registerPipe;
static int maxSessions;

void close_server(int status) {
    if (close(myPipe) < 0) {
        perror("Failed to close pipe");
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

int main(int argc, char **argv) {

    if (argc < 2) {
        printf("Failed: Couldn't start server");
        return EXIT_FAILURE;
    }

    maxSessions = atoi(argv[2]);
    registerPipe = argv[1];
    printf("Starting TecnicoFS with pipe named %s\n", registerPipe);

    if (unlink(registerPipe) != 0 && errno != ENOENT) {
        perror("Failed: Couldn't delete pipes");
        exit(EXIT_FAILURE);
    }
    
    if (mkfifo(registerPipe, 0666) != 0) {
        perror("Failed: Couldn't create pipes");
        exit(EXIT_FAILURE);
    }

    myPipe = open(registerPipe, O_RDONLY);
    if (myPipe < 0) {
        perror("Fail: Couldn't open server pipe");
        unlink(registerPipe);
        exit(EXIT_FAILURE);
    }
    
    while (true) {
        int tempPipe = open(registerPipe, O_RDONLY);

        if (tempPipe < 0) {
            if (errno == ENOENT) {
                return 0;
            }
            perror("Failed to open server pipe");
            close_server(EXIT_FAILURE);
        }
        if (close(tempPipe) < 0) {
            perror("Failed to close pipe");
            close_server(EXIT_FAILURE);
        }
        
        char opCode;
        ssize_t bytes_read = try_read(myPipe, &opCode, sizeof(char));

        while (bytes_read > 0) {
            switch (opCode) {
                case TFS_1: {
                    printf("Reading command 1\n");
                    break;
                }
                case TFS_2: {
                    printf("Reading command 2\n");
                    break;
                }
                case TFS_3: {
                    printf("Reading command 3\n");
                    break;
                }
                case TFS_4: {
                    printf("Reading command 4\n");
                    break;
                }
                case TFS_5: {
                    printf("Reading command 5\n");
                    break;
                }
                case TFS_6: {
                    printf("Reading command 6\n");
                    break;
                }
                case TFS_7: {
                    printf("Reading command 7\n");
                    break;
                }
                case TFS_8: {
                    printf("Reading command 8\n");
                    break;
                }
                case TFS_9: {
                    printf("Reading command 9\n");
                    break;
                }
                case TFS_10: {
                    printf("Reading command 10\n");
                    break;
                }
                default: {
                    break;
                }
            }
            bytes_read = try_read(myPipe, &opCode, sizeof(char));
        }
        if (bytes_read < 0) {
            perror("Failed to read from pipe");
            close_server(EXIT_FAILURE);
        }

    }
    
    return -1;
}
