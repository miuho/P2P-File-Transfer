#ifndef BT_SERVER_H
#define BT_SERVER_H

/*
  Contains functions delaing with transmitting data from the current peer
*/
#include "bt_parse.h"
#include "chunk.h"
#include "packet.h"
#include "transfer.h"

void handle_server_timeout(bt_config_t *config);

/* Receiving functions */
void receive_GET(Packet* pack, bt_peer_t *peer, int max_conn);
void receive_WHOHAS(Packet* pack, bt_peer_t *peer);
void receive_ACK(Packet *pack, bt_peer_t *peer);

/* Sending functions */
void send_DATA(Transfer *transfer);
void send_DENIED(bt_peer_t *peer);
void send_IHAVE(bt_peer_t *peer, uint8_t **bin_hash_list,
                unsigned hash_list_len);

#endif
