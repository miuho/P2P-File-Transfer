#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

#include "log.h"
#include "packet.h"
#include "spiffy.h"
#include "debug.h"

#define BYTE_SIZE_1 1
#define BYTE_SIZE_2 2
#define BYTE_SIZE_4 4

/* Static Function Declarations */
static void serialize_payload(Packet *p, uint8_t *buffer);
static void deserialize_payload(Packet *p, uint8_t *buf);

/*
  return string presentation of the packet type.  Used mostly for debug purposes
*/
const char *packet_type_str(enum packet_type t) {
    switch (t) {
        case WHOHAS:
            return "WHOHAS";
        case IHAVE:
            return "IHAVE";
        case GET:
            return "GET";
        case DATA:
            return "DATA";
        case ACK:
            return "ACK";
        case DENIED:
            return "DENIED";
        default:
            return NULL;
    }
}

void send_packet_to_peer(int sock, Packet* pack, bt_peer_t* p) {
    uint8_t* packet = serialize_packet(pack);
    if (packet == NULL) {
        return;
    }

    spiffy_sendto(sock, packet, pack->packet_len, 0,
                   (struct sockaddr *) &(p->addr), sizeof(p->addr));

    free(packet);
    return;
}

void send_packet_to_all(int sock, Packet* pack, bt_config_t *config) {
    uint8_t* packet = serialize_packet(pack);

    /* send the packet to all peers */
    bt_peer_t* p;
    for (p = config->peers; p != NULL; p = p->next) {

        /* should exclude myself in sending WHOHAS */
        if (config->identity != p->id) {
            /* start the timer for the peer */
            millitime(&(p->timer));
            spiffy_sendto(sock, packet, pack->packet_len, 0,
                       (struct sockaddr *) &(p->addr), sizeof(p->addr));
        }
    }

    free(packet);
    return;
}

Packet* deserialize_packet(uint8_t* buf) {
    uint8_t version;
    uint16_t magic, header_len, packet_len;
    uint32_t seq_num, ack_num;
    uint8_t* start_buf;
    Packet *p;

    start_buf = buf;

    memcpy(&magic, buf, BYTE_SIZE_2);
    buf += BYTE_SIZE_2;

    /* the first two bytes could be the extension ID field */
    if (ntohs(magic) != MAGIC_NUM) {

        /* if there is extension, check if the next magic number matches */
        memcpy(&magic, buf, BYTE_SIZE_2);
        buf += BYTE_SIZE_2;

        /* makes sure the packet has the correct magic number, if */
        /* it doesn't drop the packet */
        if (ntohs(magic) != MAGIC_NUM) {
            return NULL;
        }
    }

    memcpy(&version, buf, BYTE_SIZE_1);
    buf += BYTE_SIZE_1;

    /* makes sure the packet has the correct version number, if */
    /* it doesn't drop the packet */
    if (version != VER_NUM) {
        return NULL;
    }

    /* parse the header and stores info into the packet structure */
    p = (Packet*) calloc(1, sizeof(Packet));

    if (p == NULL) {
        app_errno("Failed to calloc Packet for deserialize.\n");
        return NULL;
    }

    /* type of packet: DATA, GET, WHOHAS, etc.*/
    p->type = *buf;
    buf += BYTE_SIZE_1;

    memcpy(&header_len, buf, BYTE_SIZE_2);
    buf += BYTE_SIZE_2;
    p->header_len = ntohs(header_len);

    memcpy(&packet_len, buf, BYTE_SIZE_2);
    buf += BYTE_SIZE_2;
    p->packet_len = ntohs(packet_len);

    memcpy(&seq_num, buf, BYTE_SIZE_4);
    buf += BYTE_SIZE_4;
    p->seq_num = ntohl(seq_num);

    memcpy(&ack_num, buf, BYTE_SIZE_4);
    buf += BYTE_SIZE_4;
    p->ack_num = ntohl(ack_num);

    deserialize_payload(p, start_buf + (p->header_len));

    return p;
}

/*
  parse the payload and stores info into the packet structure
*/
static void deserialize_payload(Packet *p, uint8_t *buf) {
    int i;

    if (p->type == WHOHAS || p->type == IHAVE || p->type == GET ||
        p->type == DENIED) {
        /* WHOHAS and IHAVE contains no data */
        p->hash_num = *buf;
        buf += BYTE_SIZE_4; /* padding of 3 */

        /* load the hashes from the payload */
        for (i = 0; i < p->hash_num; i++) {
            memcpy(p->hash_list[i], buf, BIN_HASH_SIZE * BYTE_SIZE_1);
            buf += (BIN_HASH_SIZE * BYTE_SIZE_1);
        }
    }
    else if (p->type == ACK) {
        return;
    }
    else {
        /* DATA packets' payload just contain the data */
        p->hash_num = 0;
        memcpy(p->data, buf, (p->packet_len - p->header_len));
    }
}

uint8_t* serialize_packet(Packet* p) {
    uint8_t *packet, *location;
    uint16_t magic, header_len, packet_len;
    uint32_t seq_num, ack_num;

    packet = calloc(p->packet_len, BYTE_SIZE_1);

    if (packet == NULL) {
        LOG("Failed to calloc Packet to serialize.\n");
        return NULL;
    }

    location = packet;
    /******************* below is the header section ********************/
    /* put in the magic number in network byte order */
    magic = htons(MAGIC_NUM);
    memcpy(location, &magic, BYTE_SIZE_2);
    location += BYTE_SIZE_2;

    /* put in the version number */
    *location = (uint8_t)VER_NUM;
    location += BYTE_SIZE_1;

    /* put in the packet type */
    *location = (uint8_t)(p->type);
    location += BYTE_SIZE_1;

    /* put in the header length in network byte order */
    header_len = htons(p->header_len);
    memcpy(location, &header_len, BYTE_SIZE_2);
    location += BYTE_SIZE_2;

    /* put in the packet length in network byte order */
    packet_len = htons(p->packet_len);
    memcpy(packet + 6, &packet_len, BYTE_SIZE_2);
    location += BYTE_SIZE_2;

    /* put in the sequence number in network byte order */
    seq_num = htonl(p->seq_num);
    memcpy(packet + 8, &seq_num, BYTE_SIZE_4);
    location += BYTE_SIZE_4;

    /* put in the acknowledgment number in network byte order */
    ack_num = htonl(p->ack_num);
    memcpy(packet + 12, &ack_num, BYTE_SIZE_4);
    location += BYTE_SIZE_4;

    serialize_payload(p, location);

    return packet;
}

/*
  parse payload section of the Packet p and serialize into the buffer

*/
static void serialize_payload(Packet *p, uint8_t *buffer) {
    uint8_t *hash;
    int i;

    /******************* below is the payload section ********************/
    if (p->type == WHOHAS || p->type == IHAVE || p->type == GET ||
        p->type == DENIED) {
        /* WHOHAS, IHAVE, GET, and DENIED packets contain hash numbers and */
        /* 3 bytes of padding. The default hash number for GET is 1 and for */
        /* DENIED is 0 */
        *buffer = p->hash_num;
        buffer++;
        *buffer = (uint8_t)0;
        buffer++;
        *buffer = (uint8_t)0;
        buffer++;
        *buffer = (uint8_t)0;
        buffer++;

        /* load the hashes into the payload */
        for (i = 0; i < p->hash_num; i++) {
            hash = p->hash_list[i];
            memcpy(buffer, hash, BIN_HASH_SIZE * BYTE_SIZE_1);
            buffer += (BIN_HASH_SIZE * BYTE_SIZE_1);
        }
    }
    else if (p->type == ACK) {
        return;
    }
    else {
        /* DATA packets' payload just contain the data */
        /* load the data into the payload */
        memcpy(buffer, p->data, (p->packet_len - p->header_len));
    }
}
