#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

void list_init(List *list) {
    list->head = NULL;
    list->tail = NULL;
}

void list_add(List *list, tfs_file file) {
    ListNode *node = malloc(sizeof(ListNode));
    node->file = file;
    node->next = NULL;

    if (list->head == NULL) {
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
}

int list_find(List *list, char *box_name) {
    ListNode *node = list->head;

    while (node != NULL) {
        if (strcmp(node->file.box_name, box_name) == 0) {
            return 1;
        }
        node = node->next;
    }
    return 0;
}

void list_remove(List *list, ListNode *prev, ListNode *node) {
    if (node == NULL)
        return;

    if (prev == NULL) {
        list->head = node->next;
    } else {
        prev->next = node->next;
    }
    free(node);
}

void list_destroy(List *list) {
    ListNode *node = list->head;
    ListNode *next;

    while (node != NULL) {
        next = node->next;
        free(node);
        node = next;
    }
}

void list_print(List *list) {
    ListNode *node = list->head;

    while (node != NULL) {
        printf("box_name: %s, n_publishers: %lu, n_subscribers: %lu\n",
               node->file.box_name, node->file.n_publishers,
               node->file.n_subscribers);
        node = node->next;
    }
}

void list_sort(List *list) {
    ListNode *node, *next, *prev;
    int ended = false;

    while (!ended) {
        ended = true;
        for (prev = NULL, node = list->head; node != NULL;
             prev = node, node = next) {
            next = node->next;
            if (next != NULL &&
                strcmp(node->file.box_name, next->file.box_name) > 0) {
                ended = false;
                node->next = next->next;

                next->next = node;
                if (prev == NULL)
                    list->head = next;
                else
                    prev->next = next;
                if (next == list->tail)
                    list->tail = node;
            }
        }
    }
}

ListNode *search_prev_node(List *list, char *box_name) {
    ListNode *node = list->head;

    // if we want to remove the head, there is no previous node
    if (strcmp(node->file.box_name, box_name) == 0)
        return NULL;

    while (node->next != NULL) {
        if (strcmp(node->next->file.box_name, box_name) == 0) {
            return node;
        }
        node = node->next;
    }

    return NULL;
}

ListNode *search_node(List *list, char *box_name) {
    ListNode *node = list->head;

    while (node != NULL) {
        if (strcmp(node->file.box_name, box_name) == 0) {
            return node;
        }
        node = node->next;
    }

    return NULL;
}