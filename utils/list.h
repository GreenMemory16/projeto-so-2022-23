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

/**
 * Initializes the list.
 */
void list_init(List *list);

/**
 * Adds a file to the list.
 */
void list_add(List *list, tfs_file file);

/**
 * Removes a file from the list.
 */
void list_remove(List *list, ListNode *prev, ListNode *node);

/**
 * Destroys the list.
 */
void list_destroy(List *list);

/**
 * Prints all the files in the list.
 */
void list_print(List *list);

/**
 * Sorts the list alphabetically by the files box names.
 */
void list_sort(List *list);

/**
 * Searches for the previous node of the node with a given box name.
 */
ListNode *search_prev_node(List *list, char *box_name);

/**
 * Searches for the node with a given box name.
 */
ListNode *search_node(List *list, char *box_name);

/**
 * Increments the number of publishers of a given file
 */
void decrement_publishers(List *list, char *box_name);

/**
 * Decrements the number of subscribers of a given file
 */
void decrement_subscribers(List *list, char *box_name);

/**
 * Increments the number of publishers of a given file
 */
void increment_publishers(List *list, char *box_name);

/**
 * Increments the number of subscribers of a given file
 */
void increment_subscribers(List *list, char *box_name);

#endif