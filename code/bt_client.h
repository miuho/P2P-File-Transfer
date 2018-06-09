#ifndef BT_CLIENT_H
#define BT_CLIENT_H

#include "packet.h"
#include "bt_parse.h"

void handle_client_timeout(bt_config_t *config);
void check_all_received(void);

/* Sending functions */
void send_WHOHAS(bt_config_t *config);
void send_GET(bt_peer_t *peer, uint8_t *bin_hash, uint32_t count);
void send_ACK(bt_peer_t *peer, unsigned ack_num);

/* Receiving functions */
void receive_DATA(Packet *pack, bt_peer_t *peer, bt_config_t *config);
void receive_DENIED(Packet* pack, bt_config_t *config,
                    struct sockaddr_in *p_addr);
void receive_IHAVE(Packet* pack, bt_config_t *config,
                   struct sockaddr_in *p_addr);

#endif
