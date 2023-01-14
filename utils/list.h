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
} List;

void list_init(List *list);
void list_add(List *list, tfs_file file);
int list_find(List *list, char *box_name);
void list_remove(List *list, ListNode *prev, ListNode *node);
void list_destroy(List *list);
void list_print(List *list);
void list_sort(List *list);
ListNode *search_prev_node(List *list, char *box_name);
ListNode *search_node(List *list, char *box_name);

#endif