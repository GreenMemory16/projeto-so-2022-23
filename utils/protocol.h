#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <stdint.h>

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