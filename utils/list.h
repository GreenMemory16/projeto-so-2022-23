#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"

void list_init(List* list) {
	list->head = NULL;
	list->tail = NULL;
}

void list_add(List* list, tfs_file file) {
	ListNode* node = malloc(sizeof(ListNode));
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

void list_remove(List* list, ListNode* prev, ListNode* node) {
	if (node == NULL) return;

	if (prev == NULL) {
		list->head = node->next;
	}
	else {
		prev->next = node->next;
	}
	free(node);
}

void list_destroy(List* list) {
	ListNode* node = list->head;
	ListNode* next;

	while (node != NULL) {
		next = node->next;
		free(node);
		node = next;
	}
}

void list_print(List* list) {
	ListNode* node = list->head;

	while (node != NULL) {
		printf("box_name: %s, n_publishers: %lu, n_subscribers: %lu\n", node->file.box_name, node->file.n_publishers, node->file.n_subscribers);
		node = node->next;
	}
}
