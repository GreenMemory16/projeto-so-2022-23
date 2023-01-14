#include "protocol.h"

typedef struct ListNode {
    tfs_file file;
    struct ListNode *next;
} ListNode;

typedef struct List {
    ListNode *head;
    ListNode *tail;
} List;

void list_init(List *list);
void list_add(List *list, tfs_file file);
void list_remove(List *list, ListNode *prev, ListNode *node);
void list_destroy(List *list);
void list_print(List *list);
void list_sort(List *list)