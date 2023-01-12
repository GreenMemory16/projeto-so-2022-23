#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stdint.h>

#define PIPE_NAME_SIZE 256
#define BOX_NAME_SIZE 32
#define MESSAGE_SIZE 1024

enum packet_opcode_t {
    REGISTER_PUBLISHER = 1,
    REGISTER_SUBSCRIBER = 2,
    CREATE_MAILBOX = 3,
    CREATE_MAILBOX_ANSWER = 4,
    REMOVE_MAILBOX = 5,
    REMOVE_MAILBOX_ANSWER = 6,
    LIST_MAILBOXES = 7,
    LIST_MAILBOXES_ANSWER = 8,
    PUBLISH_MESSAGE = 9,
    SEND_MESSAGE = 10
};

typedef struct packet_t {
    int opcode;
    char client_pipe[PIPE_NAME_SIZE + 1];
    char box_name[BOX_NAME_SIZE + 1];
    char message[MESSAGE_SIZE + 1];
} packet_t;

typedef struct worker_t {
    packet_t packet;
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} worker_t;

/* I don't know how to organize the structs for them to be parsed correctly in*/
/* the mbroker, so I'm just gonna use the dumb_struct for now.                */

/* TODO: add the packed. It wasn't working.*/

typedef struct dumb_struct {
    uint8_t code;
    char client_named_pipe_path[256];
    char box_name[32];
    int return_code;
    char error_message[1024];
    uint8_t last;
    uint64_t box_size;
    uint64_t n_subscribers;
    uint64_t n_publishers;
    char *message[1024];
} dumb_struct;

typedef struct register_handler {
    uint8_t code;
    char client_named_pipe_path[256];
    char box_name[32];
} register_handler;

typedef struct mailbox_answer {
    uint8_t code;
    int return_code;
    char error_message[1024];
} mailbox_answer;

typedef struct remover {
    uint8_t code;
    char client_named_pipe_path[256];
    char box_name[32];
} remover;

typedef struct mailbox_list {
    uint8_t code;
    char client_named_pipe_path[256];
} mailbox_list;

typedef struct list_answer {
    uint8_t code;
    char client_named_pipe_path[256];
    char box_name[32];
} list_answer;

typedef struct publisher_register {
    uint8_t code;
    uint8_t last;
    char box_name[32];
    uint64_t box_size;
    uint64_t n_subscribers;
    uint64_t n_publishers;

} publisher_register;

typedef struct message_sender {
    int code;
    char *message[1024];
} message_sender;

#endif