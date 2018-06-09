/*
  Contains functions delaing with transmitting data from the current peer
*/

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>

#include "peer.h"
#include "bt_server.h"
#include "log.h"
#include "hash.h"
#include "chunk.h"
#include "transfer.h"
#include "congestion.h"

#define MAX_DUP_ACKS 3

static Node *transfers; /* a linked list of current data transfers (outgoing) */
static void adjust_window(Transfer *transfer, uint32_t new_index);

/*
  Handles when the select call timeout:
  1) loop through transfers and handle any time'd out Transfers
*/
void handle_server_timeout(bt_config_t *config) {
    Node *node, *next;
    Transfer *transfer;

    config = config; /* quiet GCC compilation */

    if (!node_length(transfers)) {
        return;
    }

    /* check the transfers and handle any timeout transfers */
    for (node = transfers; node != NULL; node = next) {

        /* Server only cares about sending requested data,
           just delete the transfer
        */
        next = node->next;
        transfer = node->data;

        if (transfer_has_timeout(transfer)) {
            if (++(transfer->cctrl.timeout_count) >= MAX_TO_COUNTS) {
                LOG("transfer have (%u) died!!!\n",
                           ((Transfer *) node->data)->peer->id);
                node_delete(&(transfers), node, delete_transfer);
            }
            else {
                detected_first_loss(transfer);
            }
        }
    }
}

/**
 * Reply the WHOHAS packet with either IHAVE packet.
 */
void send_IHAVE(bt_peer_t *peer, uint8_t **bin_hash_list,
                unsigned hash_list_len) {

    Packet pack;
    unsigned i;

    pack.type = IHAVE;
    pack.header_len = STANDARD_HEADER_LEN;
    pack.packet_len = (STANDARD_HEADER_LEN +
                       HEADER_PAD_LEN + hash_list_len * BIN_HASH_SIZE);
    pack.seq_num = 0;
    pack.ack_num = 0;
    pack.hash_num = hash_list_len;

    /* copy hashes from hash list into packet's hash list */
    for(i = 0; i < hash_list_len; i++) {
        memcpy(pack.hash_list[i], bin_hash_list[i], BIN_HASH_SIZE);
    }

    LOG("Send to peer (%u).\n", peer->id);

    /* send a reply for the WHOHAS packet to that peer */
    send_packet_to_peer(sock, &pack, peer);
    return;
}

/*
  Send the data chunk in chunk c to the peer.
*/
void send_DATA(Transfer *transfer) {
    Packet packet;
    unsigned index; /* index into the data */
    chunk *c = transfer->c;

    transfer->cctrl.dup_count = 0;

    /* continue to send data chunks until sliding window is complete */
    while (transfer->cctrl.index < MAX_SEQ_NUM && /* end of the data chunk */
           transfer->cctrl.index < transfer->cctrl.begin +
           transfer->cctrl.wind_size) { /* sliding window */

        memset(&packet, 0, sizeof(Packet));

        packet.type = DATA;
        packet.header_len = STANDARD_HEADER_LEN;

        /* Load data from chunk into packet */
        index = transfer->cctrl.index * DATA_SIZE;

        if (BT_CHUNK_SIZE - index < DATA_SIZE) {
            /* last bit of data */
            memcpy(packet.data, c->chunk_data + index, BT_CHUNK_SIZE - index);
            packet.packet_len = STANDARD_HEADER_LEN + (BT_CHUNK_SIZE - index);
        } else {
            memcpy(packet.data, c->chunk_data + index, DATA_SIZE);
            packet.packet_len = STANDARD_HEADER_LEN + DATA_SIZE;
        }

        transfer->cctrl.index++;
        packet.seq_num = transfer->cctrl.index;

        LOG("Send DATA (%d)\n", packet.seq_num);

        send_packet_to_peer(sock, &packet, transfer->peer);
    }
}

/*
  Send a DENIED packet
*/
void send_DENIED(bt_peer_t *peer) {
    Packet packet;

    packet.type = DENIED;
    packet.header_len = STANDARD_HEADER_LEN;
    packet.packet_len = packet.header_len;

    send_packet_to_peer(sock, &packet, peer);
}

/*
  Process an inbound GET packet from peer

  Read the hash contained in the GET packet, check that the hash matches, and
  construct DATA packet from the actual data chunk, and send the packet.
*/
void receive_GET(Packet* pack, bt_peer_t *peer, int max_conn) {
    uint8_t *bin_hash = pack->hash_list[0];
    Node *chunk_node;
    chunk *c;
    Transfer *transfer;

    chunk_node = node_find(have, compare_chunk_by_bin_wrapper, bin_hash);

    if (chunk_node == NULL) {
        /* seq_num of 0 tells GET sender requested chunk is not found */
        LOG("Failed to find chunk that should be owned by self"
                   " in receive GET.\n");
        send_DENIED(peer);
        return;
    }

    c = (chunk *) (chunk_node->data);
    LOG("Peer (%u) for chunk (%d)\n", peer->id, c->id);

    if (count_node(&(transfers)) >= max_conn) {
        /* seq_num >= 1 tells GET sender max number of transfers is reached */
        LOG("Reached max number of transfers.\n");
        send_DENIED(peer);
        return;
    }

    if (!node_find(transfers, compare_transfer_by_chunk_hash, bin_hash)) {
        /* only create transfer if chunk is not in flight */

        if ((transfer = create_transfer(peer, c)) != NULL) {
            /* create the transfer node in the outgoing transfer list */
            node_insert(&(transfers), transfer);
            send_DATA(transfer);
        }
    }
}

/**
 * Process the WHOHAS packets recieved, prepare to send back a reply to the
 * peer.
 */
void receive_WHOHAS(Packet *pack, bt_peer_t *peer) {
    uint8_t *bin_hash_list[MAX_HASH_IN_PACKET];
    chunk *c;
    int i, found = 0;
    Node* node;

    LOG("Receive WHOHAS from peer (%u).\n", peer->id);

    /*
      loops through every hash in the WHOHAS packet and tries to find a
      match in hahes already have

      if so, the hash is added to a IHAVE list
    */
    for (i = 0; i < pack->hash_num; i++) {
        for (node = have; node != NULL; node = node->next) {
            c = node->data;

            /* add to the hash list of the IHAVE packet if it is owned */
            if (compare_bin_hash(c->bin_hash, pack->hash_list[i]) == 1) {
                bin_hash_list[found++] = c->bin_hash;
            }
        }
    }

    LOG("Find (%d) matches\n", found);

    send_IHAVE(peer, bin_hash_list, found);
}

/*
  Handles receiving an ACK.

  This function affects the slow start congestion control algorithm
*/
void receive_ACK(Packet *pack, bt_peer_t *peer) {
    Transfer *transfer;
    Node *transfer_node;

    transfer_node = node_find(transfers, compare_transfer_by_peer_id, peer);

    if (transfer_node == NULL) {
        LOG("Failed to find transfer for ACK'd peer (%u)."
                   "\n", peer->id);
        return;
    }

    LOG("Receive ACK (%d) from peer (%u).\n", pack->ack_num, peer->id);

    transfer = transfer_node->data;

    /* The last ACK number is received */
    if (pack->ack_num == MAX_SEQ_NUM) {
        LOG("Transfer complete chunk (%u).\n", transfer->c->id);
        node_delete(&transfers, transfer_node, delete_transfer);
        return;
    }

    /* Non-duplicated ACK */
    if (pack->ack_num > transfer->cctrl.begin) {
        adjust_window(transfer, pack->ack_num);
        congestion_control(transfer);
        send_DATA(transfer);
    }

    /* A duplicate ACK number is received */
    else if (pack->ack_num == transfer->cctrl.begin) {
        transfer->cctrl.dup_count++;

        if (transfer->cctrl.dup_count >= MAX_DUP_ACKS) {
            /* Resend the data of SEQ number from next expected ACK number */
            LOG("Got 3 duplicates of ACK (%d).\n", pack->ack_num);

            transfer->cctrl.index = transfer->cctrl.begin;
            detected_first_loss(transfer);
            send_DATA(transfer);
        }
    }

    transfer->cctrl.timeout_count = 0;
    millitime(&(transfer->timestamp));
}

/*
  Shifts the congestion window to the most recent ACKed packet.
*/
static void adjust_window(Transfer *transfer, uint32_t new_index) {
    transfer->cctrl.begin = new_index;

    /* update next packet to be sent if it is outside of the new window */
    if (transfer->cctrl.begin > transfer->cctrl.index) {
        transfer->cctrl.index = transfer->cctrl.begin;
    }
}
