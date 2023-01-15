#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

void list_init(List *list) {
    list->head = NULL;
    list->tail = NULL;
    pthread_mutex_init(&list->lock, NULL);
}

void list_add(List *list, tfs_file file) {
    pthread_mutex_lock(&list->lock);
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
    list->size++;
    pthread_mutex_unlock(&list->lock);
}

void list_remove(List *list, ListNode *prev, ListNode *node) {
    pthread_mutex_lock(&list->lock);
    if (node == NULL) {
        pthread_mutex_unlock(&list->lock);
        return;
    }
    if (prev == NULL) {
        list->head = node->next;
    } else {
        prev->next = node->next;
    }
    free(node);
    list->size--;
    pthread_mutex_unlock(&list->lock);
}

void list_destroy(List *list) {
    pthread_mutex_destroy(&list->lock);
    ListNode *node = list->head;
    ListNode *next;

    while (node != NULL) {
        next = node->next;
        free(node);
        node = next;
    }
}

void list_print(List *list) {
    pthread_mutex_lock(&list->lock);

    ListNode *node = list->head;

    while (node != NULL) {
        fprintf(stdout, "%s %zu %zu %zu\n", node->file.box_name,
                node->file.box_size, node->file.n_publishers,
                node->file.n_subscribers);
        node = node->next;
    }
    pthread_mutex_unlock(&list->lock);
}

void list_sort(List *list) {
    pthread_mutex_lock(&list->lock);
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
    pthread_mutex_unlock(&list->lock);
}

ListNode *search_prev_node(List *list, char *box_name) {
    pthread_mutex_lock(&list->lock);

    ListNode *node = list->head;

    // if we want to remove the head, there is no previous node
    if (strcmp(node->file.box_name, box_name) == 0) {
        pthread_mutex_unlock(&list->lock);
        return NULL;
    }

    while (node->next != NULL) {
        if (strcmp(node->next->file.box_name, box_name) == 0) {
            pthread_mutex_unlock(&list->lock);
            return node;
        }
        node = node->next;
    }
    pthread_mutex_unlock(&list->lock);
    return NULL;
}

ListNode *search_node(List *list, char *box_name) {
    pthread_mutex_lock(&list->lock);
    ListNode *node = list->head;

    while (node != NULL) {
        if (strcmp(node->file.box_name, box_name) == 0) {
            pthread_mutex_unlock(&list->lock);
            return node;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&list->lock);
    return NULL;
}

void increment_publishers(List *list, char *box_name) {
    pthread_mutex_lock(&list->lock);
    ListNode *node = list->head;

    while (node != NULL) {
        if (strcmp(node->file.box_name, box_name) == 0) {
            node->file.n_publishers++;
            pthread_mutex_unlock(&list->lock);
            return;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&list->lock);
}

void decrement_publishers(List *list, char *box_name) {
    pthread_mutex_lock(&list->lock);
    ListNode *node = list->head;

    while (node != NULL) {
        if (strcmp(node->file.box_name, box_name) == 0) {
            node->file.n_publishers--;
            pthread_mutex_unlock(&list->lock);
            return;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&list->lock);
}

void increment_subscribers(List *list, char *box_name) {
    pthread_mutex_lock(&list->lock);
    ListNode *node = list->head;

    while (node != NULL) {
        if (strcmp(node->file.box_name, box_name) == 0) {
            node->file.n_subscribers++;
            pthread_mutex_unlock(&list->lock);
            return;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&list->lock);
}

void decrement_subscribers(List *list, char *box_name) {
    pthread_mutex_lock(&list->lock);
    ListNode *node = list->head;

    while (node != NULL) {
        if (strcmp(node->file.box_name, box_name) == 0) {
            node->file.n_subscribers--;
            pthread_mutex_unlock(&list->lock);
            return;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&list->lock);
}
