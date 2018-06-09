#ifndef PEER_H
#define PEER_H

/*
  peer.h
*/
#include <sys/time.h>

#include "linkedlist.h"
#include "bt_parse.h"

mytime_t start_time;

Node *wanted; /* a linked list of chunk of user requested */
Node *have; /* a linked list of chunk locally owned */
Node *missing; /* wanted - have */
int sock; /* socket fd for local peer */
char *user_get_chunk_file; /* path of the output file to store data */
char *user_output_filename; /* path of the output file to store data */

/*
  Returns 1 if the chunk can be identified with the given hex hash, 0
  otherwise.
*/
int compare_chunk_by_hex_wrapper(const void *c, const void *hash);

/*
  Returns 1 if the chunk can be identified with the given bin hash, 0
  otherwise.
*/
int compare_chunk_by_bin_wrapper(const void *c, const void *hash);

/*
  Returns the peer with the given peer address, NULL if no match is found.
*/
bt_peer_t* find_peer(bt_config_t *config, struct sockaddr_in *p_addr);
#endif
