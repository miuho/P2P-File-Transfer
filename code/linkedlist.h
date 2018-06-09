/*!
  \file linkedlist.h
  \brief Header file for the linked list data structure

  Contains the type definition for the linked list node and the exposed
  functions for operating on the linked list.

  Each node in the linked list contains:
  	next -> points to the next node
    data -> the data stored in the node

  While most methods are intuitive, node_find and node_delete_all require
  function pointers that know what to do with the data contained inside the
  node.
*/

#ifndef LINKEDLIST_H
#define LINKEDLIST_H

#include <stdlib.h>

typedef struct node {
    struct node *next;
    void *data;
} Node;

/* Function definitions */

/*!
  \fn int count_node(Node **head)
  \brief Given the head of the list, count the number of nodes

  \return the number of nodes in the linked list
*/
int count_node(Node **head);

/*!
  \fn Node *node_insert_node(Node **head, Node *node)
  \brief Given a node, insert it into the list head

  \return the Node successfully inserted
*/
Node *node_insert_node(Node **head, Node *node);

/*!
  \fn Node *node_insert(Node **head, void *data)
  \brief insert the data into the linked list pointed to by head

  \return the inserted node if successfully inserted and NULL otherwise.
*/
Node *node_insert(Node **head, void *data);

/*!
  \fn int node_delete(Node **head, Node *node)
  \brief delete node node from the linked list starting at head

  The remove_data function is called in order to remove the data contained in
  the removed node.

  \return 0 if the node was found and deleted, 0 otherwise
*/
int node_delete(Node **head, Node *node, int(*remove_data)(void *data));

/*!
  \fn Node *find(Node *head,
  				int(*compare)(const void *, const void *),
                const void *key)
  \brief find the first node in the list head that contains the data where
  compare returns true for

  The compare function has the Node->data based in as the first argument and the
  key passed in as the second argument. (e.g. compare(node->data, key))

  \return a Node if the data is found, NULL otherwise
*/
Node *node_find(Node *head, int(*compare)(const void *, const void *),
                void *key);

/*!
  \fn size_t node_length(Node *head)
  \brief return the number of nodes in the list starting with head

  \return the number of nodes in the list
*/
size_t node_length(Node *head);

/*!
  \fn size_t node_delete_all(Node **head)
  \brief deallocate all nodes in the entire list

  The remove_data function is called in order to remove the data contained in
  each of the removed node.

  \return the number of nodes deleted
*/
size_t node_delete_all(Node **head, int(remove_data)(void *));

#endif
