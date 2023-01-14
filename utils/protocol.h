#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include <pthread.h>
#include <stdint.h>

#define PIPE_NAME_SIZE 256
#define BOX_NAME_SIZE 32
#define MESSAGE_SIZE 1024
#define MAX_FILES 2048

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

typedef struct registration_data_t {
    char client_pipe[PIPE_NAME_SIZE + 1];
    char box_name[BOX_NAME_SIZE + 1];
} registration_data_t;

typedef struct answer_data_t {
    int32_t return_code;
    char error_message[MESSAGE_SIZE + 1];
} answer_data_t;

typedef struct list_box_data_t {
    char client_pipe[PIPE_NAME_SIZE + 1];
} list_box_data_t;

typedef struct mailbox_data_t {
    uint8_t last;
    char box_name[BOX_NAME_SIZE + 1];
    uint64_t box_size;
    uint64_t n_subscribers;
    uint64_t n_publishers;
} mailbox_data_t;

typedef struct message_data_t {
    char message[MESSAGE_SIZE + 1];
} message_data_t;

typedef struct __attribute__((packed)) packet_t {
    uint8_t opcode;
    union {
        registration_data_t registration_data;
        answer_data_t answer_data;
        list_box_data_t list_box_data;
        mailbox_data_t mailbox_data;
        message_data_t message_data;
    } payload;
} packet_t;

typedef struct worker_t {
    packet_t packet;
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} worker_t;

typedef struct tfs_file {
    char box_name[BOX_NAME_SIZE + 1];
    uint64_t n_publishers;
    uint64_t n_subscribers;
} tfs_file;

typedef struct ListNode {
    tfs_file file;
    struct ListNode *next;
} ListNode;

typedef struct List {
	ListNode* head;
	ListNode* tail;
} List;




#endif