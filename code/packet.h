#ifndef PACKET_H
#define PACKET_H

#include <inttypes.h>
#include <stdlib.h>
#include "hash.h"
#include "linkedlist.h"
#include "bt_parse.h"
#include "chunk.h"

#define DATA_SIZE 1000
#define MAGIC_NUM 15441
#define VER_NUM 1
#define MAX_PACKET_SIZE 1500
#define STANDARD_HEADER_LEN 16
#define HEADER_PAD_LEN 4
#define MAX_PAYLOAD_SIZE (MAX_PACKET_SIZE - STANDARD_HEADER_LEN - HEADER_PAD_LEN)
#define MAX_HASH_IN_PACKET (MAX_PAYLOAD_SIZE / BIN_HASH_SIZE)

enum packet_type {
    WHOHAS,
    IHAVE,
    GET,
    DATA,
    ACK,
    DENIED
};

const char *packet_type_str(enum packet_type t);

typedef struct Packet {
    enum packet_type type;
    uint16_t header_len;
    uint16_t packet_len;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t hash_num;
    uint8_t hash_list[MAX_HASH_IN_PACKET][BIN_HASH_SIZE];
    uint8_t data[DATA_SIZE];
} Packet;

/*
  Send a packet to one peer
*/
void send_packet_to_peer(int sock, Packet* pack, bt_peer_t* p);

/*
  Send a packet to all peers
*/
void send_packet_to_all(int sock, Packet* pack, bt_config_t *config);

/*
  Convert from a binary array to a packet struct

  \returns
  if successful: a pointer to a dynamically allcoated Packet struct
  else: NULL
*/
Packet* deserialize_packet(uint8_t* buf);

/*
  Convert from the packet struct to binary array that can be sent over the
  network

  \return
  if successful: a dynamically allocated binary array
  else: NULL
*/
uint8_t* serialize_packet(Packet* p);

#endif
