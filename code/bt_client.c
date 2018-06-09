/*
  Functions and states for the client-side code of a peer
*/

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "bt_client.h"
#include "transfer.h"
#include "peer.h"
#include "log.h"
#include "sha.h"

#define MAX_GET_FOR_DENIED 1000

static Node *transfers; /* a linked list of current data transfers (incoming) */
static unsigned get_chunk_data(void);
static int validate_chunk(Transfer *transfer);
static int chunk_bits_all_received(Transfer *transfer);
static uint32_t data_find_largest_consec_seq(Transfer *transfer);
static void data_store(Packet *pack, Transfer *transfer);
static int data_already_stored(Packet *pack, Transfer *transfer);

/**
 * Returns 1 if the peer exists in the linked list, 0 otherwise.
 */
static int peer_exist_in_list(Node **head, bt_peer_t* target_p) {
    Node* p_node;
    Node* next;
    bt_peer_t* p;

    /* returns 1 if the peer list contains target peer already */
    for(p_node = *head; p_node != NULL; p_node = next) {
        next = p_node->next;
        p = p_node->data;
        if (p->id == target_p->id) {
            return 1;
        }
    }

    return 0;
}

/**
 * Initiate the data transfer, returns 0 if unsuccessful.
 */
static int init_data_transfer(bt_config_t *config) {
    get_chunk_data();
    return EXIT_FAILURE;
}

/**
 * Contruct the output file with wanted chunks data.
 */
static void construct_data(void) {
    FILE* pFile;
    Node* node;
    chunk* c;

    pFile = fopen(user_output_filename, "w+b");

    /* moves the wanted chunks data to output file */
    for (node = wanted; node != NULL; node = node->next) {
        c = node->data;
        LOG("Write chunk (%d) with hash (%s) of file.\n",
                   c->id, c->hex_hash);
        fseek(pFile , c->id * BT_CHUNK_SIZE, SEEK_SET);
        fwrite(c->chunk_data, sizeof(char), BT_CHUNK_SIZE, pFile);
    }
    fclose(pFile);

    /* tell the user the data transfer is completed */
    printf("GOT %s\n", user_get_chunk_file);

    free(user_output_filename);
    free(user_get_chunk_file);
    return;
}

/**
 * Check if all chunks are received.
 */
void check_all_received(void) {
    Node *node, *next;

    /* checks if there is anything chunks left in the missing chunks list */
    if (count_node(&missing)) {
        LOG("(%d) chunks are not received.\n",
                    count_node(&missing));
        return;
    }
    else {
        /* all chunks have received */
        construct_data();

        /* clear wanted list */
        for(node = wanted; node != NULL; node = next) {
            next = node->next;
            node_delete(&(wanted), node, NULL);
        }

        assert(wanted == NULL);
        LOG("Cleared wanted list.\n");
    }
}

/**
 * Begin gather data from other peers by sending GET packages
 */
static unsigned get_chunk_data(void) {
    Node* node;
    Node* transfer_node;
    chunk* c;
    bt_peer_t* peer;
    Transfer *transfer;
    unsigned count = 0;

    /* loops through each missing chunk to see if they have a available peer to
     * send GET packet */
    for (node = missing; node != NULL; node = node->next) {
        c = node->data;

        /* make sures a transfer has not created with this chunk yet */
        transfer_node = node_find(transfers, compare_transfer_by_chunk_hash,
                                  c->bin_hash);

        peer = find_first_available_peer(c);

        /* make sure the chunk has a available peer */
        if (peer != NULL && transfer_node == NULL) {
            transfer = create_transfer(peer, c);

            if (transfer != NULL) {
                node_insert(&transfers, transfer);
                send_GET(peer, c->bin_hash, 1);

                LOG("New transfer for chunk (%u) to peer (%u).\n", c->id,
                    peer->id);

                count++;
            }
        }

        else if (peer == NULL && transfer_node == NULL) {
            LOG("No available peer for chunk (%d).\n" , c->id);
        }
    }

    return count;
}

/**
 * Remove the dead peer from each missing chunks' peers list, and remove the
 * dead transfer node.
 */
static void handle_transfer_dead(Node *transfer_node, bt_config_t *config) {
    Node *node;
    Node *p_node, *next;
    bt_peer_t *p;
    bt_peer_t *peer;
    chunk *missing_c;
    Transfer *transfer;

    transfer = transfer_node->data;
    peer = transfer->peer;

    LOG("Transfer with peer (%d) is dead.\n", peer->id);

    /* remove the dead peer from each peers list in missing chunks */
    for(node = missing; node != NULL; node = node->next) {
        missing_c = node->data;

        /* check matching peer id of each peer in missing chunks list */
        for(p_node = missing_c->peer; p_node != NULL; p_node = next) {
            next = p_node->next;
            p = p_node->data;

            if (p->id == peer->id) {
                node_delete(&(missing_c->peer), p_node, NULL);
            }
        }
    }

    /* remove the transfer node from the transfer list */
    node_delete(&(transfers), transfer_node, delete_transfer);
}

/*
  Handles when the select call timeout:
  1) loop through outstanding list and handle any time'd out GET requests
  2) loop through transfers and handle any time'd out Transfers
*/
void handle_client_timeout(bt_config_t *config) {
    Node *node, *next;
    Transfer *transfer;
    chunk *c;
    uint32_t ack_num;

    if (!count_node(&missing)) {
        return;
    }

    /* check the transfers and handle any timeout transfers */
    for (node = transfers; node != NULL; node = next) {
        next = node->next;
        transfer = node->data;
        c = transfer->c;

        if (transfer_has_timeout(transfer)) {
            if (++(transfer->cctrl.timeout_count) >= MAX_TO_COUNTS) {
                handle_transfer_dead(node, config);
            }
            else {
                if (transfer->c->bits_count  == 0) {
                    /* should resend GET if timeout happens for first DATA */
                    send_GET(transfer->peer, c->bin_hash, 1);
                }
                else {
                    /* should resend ACK if timeout happens for next DATA */
                    ack_num = data_find_largest_consec_seq(transfer);
                    send_ACK(transfer->peer, ack_num);
                }
            }
        }
    }

    /* check if any new data transfer can be initiated */
    init_data_transfer(config);

    /* flood the network with WHOHAS if there is unclaimed chunk in missing */
    if (node_length(transfers) < node_length(missing)) {
        send_WHOHAS(config);
    }
}

/*
  Send a GET packet to the peer with given count number of binary hashes.
*/
void send_GET(bt_peer_t *peer, uint8_t *bin_hash, uint32_t count) {
    Packet pack;
    Node *transfer_node;
    Transfer *transfer;

    transfer_node = node_find(transfers, compare_transfer_by_peer_id, peer);

    if (transfer_node == NULL) {
        LOG("Failed to find match in transfers.");
        return;
    }

    /* create a GET paccket */
    pack.type = (uint8_t)GET;
    pack.header_len = (uint16_t)STANDARD_HEADER_LEN;
    pack.packet_len = (uint16_t)(STANDARD_HEADER_LEN +
                                 HEADER_PAD_LEN + BIN_HASH_SIZE);
    pack.seq_num = (uint32_t)count;
    pack.ack_num = (uint32_t)0;
    pack.hash_num = (uint8_t)1;
    memcpy(pack.hash_list[0], bin_hash, BIN_HASH_SIZE);

    transfer = transfer_node->data;

    /* measure the time between this GET and first DATA */
    millitime(&(transfer->timestamp));

    send_packet_to_peer(sock, &pack, peer);
}

/*
 * Send WHOHAS packets to all peers.
 *
 */
void send_WHOHAS(bt_config_t *config) {
    Node* n;
    chunk* c;
    Packet* pack = (Packet*) calloc(1, sizeof(Packet));
    /* counts the number of hash appended to list so far */
    int count = 0;

    /* creates a list of hashes */
    for (n = missing; n != NULL; n = n->next) {
        c = n->data;
        /* generate a WHOHAS packet */
        memcpy(pack->hash_list[count], c->bin_hash, BIN_HASH_SIZE);
        count++;

        /* prepare and send the hash list in a WHOHAS packet whenever */
        /* 74 hashes are appended to the list or that there is no more */
        /* chunk in the chunks linked list */
        if ((count == MAX_HASH_IN_PACKET) || (n->next == NULL)) {
            /* fill in the WHOHAS packet */
            pack->type = WHOHAS;
            pack->header_len = STANDARD_HEADER_LEN;
            pack->packet_len = (STANDARD_HEADER_LEN +
                                HEADER_PAD_LEN + count * BIN_HASH_SIZE);
            pack->seq_num = 0;
            pack->ack_num = 0;
            pack->hash_num = count;

            LOG("creates a packet of type (%s) header_len"
                       " (%d)\n", packet_type_str(pack->type),pack->header_len);
            LOG("packet_len (%d) seq_num (%d) ack_num(%d)"
                       "hash_num (%d)\n",
                       pack->packet_len,
                       pack->seq_num, pack->ack_num, pack->hash_num);

            /* send the WHOHAS packet to all peers */
            send_packet_to_all(sock, pack, config);

            /* clear data of the WHOHAS packet for sending a new packet */
            memset(pack, 0, sizeof(Packet));
            /* reinitialize the hashes count for next packet */
            count = 0;
        }
    }

    LOG("all WHOHAS packets sent\n");
    return;
}

/*
  Send a ACK packet to the peer with given acknowledgement number.
*/
void send_ACK(bt_peer_t *peer, unsigned ack_num) {
    Packet packet;
    Node *transfer_node;
    Transfer *transfer;

    packet.type = ACK;
    packet.header_len = STANDARD_HEADER_LEN;
    packet.packet_len = packet.header_len;
    packet.seq_num = 0;
    packet.ack_num = ack_num;

    transfer_node = node_find(transfers, compare_transfer_by_peer_id, peer);

    if (transfer_node == NULL) {
        LOG("Failed to find match in transfers.");
        return;
    }

    transfer = transfer_node->data;

    /* measure the time between this ACK and next DATA */
    millitime(&(transfer->timestamp));

    send_packet_to_peer(sock, &packet, peer);
    LOG("Sent ACK (%u) to peer (%u).\n", ack_num, peer->id);
}

/**
 * Process the DENIED packets recieved and remove the failed transfer node.
 */
void receive_DENIED(Packet* pack, bt_config_t *config,
                    struct sockaddr_in *p_addr) {
    Node *transfer_node;
    bt_peer_t *peer;

    LOG("received DENIED from address (%s) port (%d)\n",
               inet_ntoa(p_addr->sin_addr), ntohs(p_addr->sin_port));

    /* find the peer structure from peers by a given peer address */
    peer = find_peer(config, p_addr);

    if (peer == NULL) {
        LOG("Failed to find a matching peer address in peers.");
        return;
    }

    LOG("DENIED from peer id (%d)\n", peer->id);

    transfer_node = node_find(transfers, compare_transfer_by_peer_id, peer);

    if (transfer_node == NULL) {
        LOG("Failed to find match in transfers.");
        return;
    }

    node_delete(&(transfers), transfer_node, delete_transfer);
}

/**
 * Process the IHAVE packets recieved, insert the peer to the missing chunk as
 * one of the peers that owns it.
 */
void receive_IHAVE(Packet* pack, bt_config_t *config,
                   struct sockaddr_in *p_addr) {
    int i;
    uint8_t* bin_hash;
    chunk* c;
    Node* node;
    bt_peer_t* peer;

    /* find the peer structure from peers by a given peer address */
    peer = find_peer(config, p_addr);

    if (peer == NULL) {
        LOG("Failed to find a matching peer address in peers.");
        return;
    }

    LOG("Received IHAVE from peer (%u).\n", peer->id);
    LOG("Peer (%d) has (%d) hashes\n", peer->id, pack->hash_num);

    /* insert the peer structure into missing chunks, so that in the */
    /* end, each chunk will have a linked list of peers that owns that */
    /* chunk */
    for (i = 0; i < pack->hash_num; i++) {
        bin_hash = pack->hash_list[i];
        for (node = missing; node != NULL; node = node->next) {
            c = node->data;
            /* check if the missing chunk hash matches the hash from */
            /* the IHAVE packet, and insert the peer to the chunk */
            if (compare_bin_hash(c->bin_hash, bin_hash) == 1) {
                /* add the peer to peer list if not added before */
                if (peer_exist_in_list(&(c->peer), peer) == 0) {
                    node_insert(&(c->peer), peer);
                    LOG("Insert peer (%d) as available peer for chunk (%d).\n",
                        peer->id, c->id);
                }
            }
        }
    }

    /* reset the timer for peer for recording next IHAVE reply */
    millitime(&(peer->timer));

    /* check if any new data transfer can be initiated */
    init_data_transfer(config);
}

/*
  Receive a DATA packet

  Read the data in the packet, make sure that its hash value is correct,
  and then store the data inside the request chunk
*/
void receive_DATA(Packet *pack, bt_peer_t *peer, bt_config_t *config) {
    uint32_t ack_num;
    Node *transfer_node;
    Transfer *transfer;

    transfer_node = node_find(transfers, compare_transfer_by_peer_id, peer);

    if (transfer_node == NULL) {
        LOG("Invalid tranfser node - NULL.\n");
        return;
    }

    transfer = transfer_node->data;

    /* records this data packet is stored if it haven't been stored yet*/
    if (!data_already_stored(pack, transfer)) {
        data_store(pack, transfer);
    }

    ack_num = data_find_largest_consec_seq(transfer);
    send_ACK(peer, ack_num);

    if (chunk_bits_all_received(transfer)) {
        if (validate_chunk(transfer)) {
            check_all_received();
        }

        /* can safely delete transfer:
           if data is valid, start new transfer
           else, need a new transfer to get the data anyway
        */
        node_delete(&transfers, transfer_node, delete_transfer);
        init_data_transfer(config);
    }
}

/*
  Test whether or not the data packet pack has already been received
  and stored in the transfer's chunk storage.

  Returns 1 if the packet has already been received and stored, 0 otherwise.
*/
static int data_already_stored(Packet *pack, Transfer *transfer) {
    return transfer->c->bits[pack->seq_num - 1];
}

/*
  Store the chunk in the packet into the transfer's chunk storage
*/
static void data_store(Packet *pack, Transfer *transfer) {
    chunk *c = transfer->c;
    uint8_t *data = pack->data;

    memcpy(c->chunk_data + ((pack->seq_num - 1) * DATA_SIZE), data,
           pack->packet_len - pack->header_len);

    transfer->c->bits[pack->seq_num - 1] = (char) 1;
    transfer->c->bits_count++;

    LOG("Stored data (%u) for chunk (%u).\n", pack->seq_num, c->id);
}

/*
  Loop through the transfer's chunk storage and return the
  sequence number presenting the last of a consecutive chunk.

  uint32_t should be [1, CHUNK_BITS_COUNT]
*/
static uint32_t data_find_largest_consec_seq(Transfer *transfer) {
    uint32_t i = 0;

    while(i < MAX_SEQ_NUM && transfer->c->bits[i]) {
        i++;
    }

    return i;
}

/*
  Return whether or not all chunk bits have been received.

  return 1 if all have received, 0 otherwise
 */
static int chunk_bits_all_received(Transfer *transfer) {
    return transfer->c->bits_count == MAX_SEQ_NUM;
}

/*
  Check if the data chunk received is correct or not. Then adjust the missing
  and have list accordingly.

  Data is validated by check it's hash to the expect chunk hash.  If the data is
  valid, then we remove the chunk from missing and into have.  Otherwise, just
  discard the data.

  Return 1 if chunk was valid, 0 otherwise.
 */
static int validate_chunk(Transfer *transfer) {
    uint8_t hash[SHA1_HASH_SIZE];
    chunk *c = transfer->c;
    Node *missing_node;
    int result;

    shahash(c->chunk_data, BT_CHUNK_SIZE, hash);

    if ((result = compare_bin_hash(hash, c->bin_hash))) {
        LOG("Valid chunk data.\n");

        missing_node = node_find(missing, compare_chunk_by_bin_wrapper,
                                 c->bin_hash);

        if (missing_node == NULL) {
            LOG("Failed to find missing node to store data.\n");
        } else {
            node_delete(&(missing), missing_node, NULL);
            node_insert(&(have), c);

            LOG("Now have chunk (%d).\n", c->id);
        }
    }

    else {
        LOG("Invalid chunk data.\n");
        memset(c->chunk_data, 0, BT_CHUNK_SIZE);
        memset(c->bits, 0, sizeof(c->bits) * sizeof(c->bits[0]));
        c->bits_count = 0;
    }

    return result;
}
