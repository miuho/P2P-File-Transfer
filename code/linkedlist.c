/*!
  \file linkedlist.c
  \brief The generic linked list data structure
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "linkedlist.h"

int count_node(Node **head) {
    Node* n;
    int count;

    if (head == NULL) {
        return 0;
    }

    count = 0;
    for (n = *head; n != NULL; n = n->next) {
	count++;
    }
    return count;
}

Node *node_insert_node(Node **head, Node *node) {
    /* Just a insert to head */
    if (head == NULL) {
        return NULL;
    }

    node->next = *head;
    *head = node;

    return node;
}

Node *node_insert(Node **head, void *data) {
    Node *node;

    if ((node = calloc(1, sizeof(Node))) == NULL) {
        return NULL;
    }

    /* insert to head */
    node->data = data;
    node->next = *head;
    *head = node;
    return node;
}

Node *node_find(Node *head, int(*compare)(const void *, const void *),
                void *key) {
    while(head != NULL) {
        if (compare(head->data, key)) {
            return head;
        }
        head = head->next;
    }

    return NULL;
}

int node_delete(Node **head, Node *node, int(*remove_data)(void *data)) {
    Node *temp;

    if (*head == NULL) {
        return EXIT_FAILURE;
    }

    if (*head == node) {
        *head = node->next;
        if (remove_data != NULL) {
            remove_data(node->data);
        }
        free(node);
        return EXIT_SUCCESS;
    }

    temp = *head;

    /* get the previous node that's before the node to be deleted */
    while (temp != NULL && temp->next != node) {
        temp = temp->next;
    }

    if (temp != NULL) {
        temp->next = node->next;

        if (remove_data != NULL) {
            remove_data(node->data);
        }

        free(node);
        return EXIT_SUCCESS;
    } else {
        return EXIT_FAILURE;
    }
}

size_t node_iterate_delete(Node **head,
                           int (*test)(void *data),
                           int(*remove_data)(void *data)) {
    Node *temp, *next;
    size_t rm_count = 0;

    if (head == NULL || test == NULL) {
        return -1;
    }

    /* iterate through the list and remove the ones that need to be removed */
    for(temp = *head; temp != NULL; temp = next) {
        next = temp->next;

        if(test(temp->data)) {
            node_delete(head, temp, remove_data);
            rm_count++;
        }
    }

    return rm_count;
}

size_t node_length(Node *head) {
    size_t length = 0;

    while(head != NULL) {
        length++;
        head = head->next;
    }

    return length;
}

size_t node_delete_all(Node **head, int(remove_data)(void *)) {
    Node *temp;
    size_t removed_count = 0;

    while (*head != NULL) {
        temp = (*head)->next;
        if (remove_data != NULL) {
            remove_data((*head)->data);
        }
        free(*head);
        *head = temp;
        removed_count++;
    }

    return removed_count;
}
