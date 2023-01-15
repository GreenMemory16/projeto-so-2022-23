#ifndef __LIST_H__
#define __LIST_H__

#include "protocol.h"

typedef struct ListNode {
    tfs_file file;
    struct ListNode *next;
} ListNode;

typedef struct List {
    ListNode *head;
    ListNode *tail;
    pthread_mutex_t lock;
    int size;
} List;

void list_init(List *list);
void list_add(List *list, tfs_file file);
void list_remove(List *list, ListNode *prev, ListNode *node);
void list_destroy(List *list);
void list_print(List *list);
void list_sort(List *list);
ListNode *search_prev_node(List *list, char *box_name);
ListNode *search_node(List *list, char *box_name);
void decrement_publishers(List *list, char *box_name);
void decrement_subscribers(List *list, char *box_name);
void increment_publishers(List *list, char *box_name);
void increment_subscribers(List *list, char *box_name);

#endif