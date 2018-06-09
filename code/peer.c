/*
  peer.c

  A peer is a both client and server node in a distributed system.  Each
  peer has data chunks that they own, and they can be queried by other peers.

  Authors: Xing Zhou <xingz@andrew>
  HingOn Miu <hmiu@andrew>
*/

#include <sys/time.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "linkedlist.h"
#include "chunk.h"
#include "bt_io.h"
#include "log.h"
#include "packet.h"
#include "hash.h"
#include "transfer.h"
#include "bt_server.h"
#include "bt_client.h"
#include "peer.h"

/* Static Functions */
static void peer_run(bt_config_t *config);
static void peer_setup(bt_config_t *config);
static void load_own_chunk_data(bt_config_t *config);
static void handle_user_input(bt_config_t *config, char *line, void *cbdata);
static void handle_timeout(bt_config_t *config);

int main(int argc, char **argv) {
    bt_config_t config;

    bt_init(&config, argc, argv);

    LOG("peer.c main beginning.\n");

#ifdef TESTING
    DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

    config.identity = 0; // your group number here
    strcpy(config.chunk_file, "chunkfile");
    strcpy(config.has_chunk_file, "haschunks");
#endif

    bt_parse_command_line(&config);

    /* set up log file, open file descripter */
    setup_logging(&config);

#ifdef DEBUG
    if (debug & DEBUG_INIT) {
        bt_dump_config(&config);
    }
#endif

    peer_setup(&config);
    peer_run(&config);

    return 0;
}

/**
 * Returns 1 if two peer addresses are equal, 0 otherwise.
 */
int compare_peer_addr(struct sockaddr_in *p1_addr, struct sockaddr_in *p2_addr){
    return ((ntohs(p1_addr->sin_port) == ntohs(p2_addr->sin_port)) &&
            (strcmp(inet_ntoa(p1_addr->sin_addr),
                    inet_ntoa(p2_addr->sin_addr)) == 0));
}

/**
 * Find the peer of all peers with matching address.
 */
bt_peer_t* find_peer(bt_config_t *config, struct sockaddr_in *p_addr) {
    bt_peer_t* p;

    for (p = config->peers; p != NULL; p = p->next) {
        if (compare_peer_addr(&(p->addr), p_addr) == 1) {
            return p;
        }
    }

    return NULL;
}

void process_inbound_udp(int sock, bt_config_t *config) {
#define BUFLEN 1500
    struct sockaddr_in from;
    socklen_t fromlen;
    char buf[BUFLEN];
    Packet* packet;
    bt_peer_t* peer;

    fromlen = sizeof(from);
    spiffy_recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &from, &fromlen);

    peer = find_peer(config, &from);

    if (peer == NULL) {
        LOG("Invalid Peer - NULL.\n");
        return;
    }

    /* initialize the peer timer */
    peer->timer = millitime(NULL);
    peer->timeout_count = 0;

    /* parse the buf and store info to Packet struct */
    packet = deserialize_packet((uint8_t *)buf);

    if (packet == NULL) {
        LOG("Invalid Packet - NULL.\n");
        return;
    }

    if (packet != NULL && peer != NULL) {
        switch(packet->type) {
            /* Server Side */
            case WHOHAS:
                receive_WHOHAS(packet, peer);
                break;

            case GET:
                receive_GET(packet, peer, config->max_conn);
                break;

            case ACK:
                receive_ACK(packet, peer);
                break;

                /* Client Side */
            case IHAVE:
                receive_IHAVE(packet, config, &from);
                break;

            case DENIED:
                receive_DENIED(packet, config, &from);
                break;

            case DATA:
                receive_DATA(packet, peer, config);
                break;

            default:
                LOG("INVALID packet type received\n");
                assert(0);
                break;
        }
    }

    return;
}

/*
  Create the wanted list and missing list.

  The wanted list is all the chunks requested by the user.  The missing list is
  the list of chunks fo wanted - have.

  This reads in the chunks file specified by the user.
*/
void create_wanted_missing_list(char *chunkfile) {
    FILE *f;
    Node *node;
    chunk* c;
    int id;
    char line[FILE_LEN], hash[HEX_HASH_SIZE];

    /* open the getchunkfile */
    f = fopen(chunkfile, "r");
    assert(f != NULL);

    /* parse the "id chunk_hash\n" and store into chunks */
    while (fgets(line, FILE_LEN, f) != NULL) {
        sscanf(line, "%d %s\n", &id, hash);
        LOG("Want chunk (%u:%s)\n", id, (char *) hash);

        node = node_find(have, compare_chunk_by_hex_wrapper, hash);
        if (node == NULL) {
            c = create_chunk(id, (uint8_t *) hash);
            node = node_insert(&(missing), c);

            LOG("inserted chunk with hash "
                       "(%s) in missing\n",
                       (char *) c->hex_hash);

            LOG("missing now has %d chunks\n",
                       count_node(&missing));
        } else {
            LOG("Do have chunk id %u\n",
                       ((chunk*) (node->data))->id);
        }

        c = node->data;
        c->id = id;

        node_insert(&(wanted), node->data);
        LOG("inserted chunk (%u) with hash (%s)"
                   " in wanted\n",
                   ((chunk*) (node->data))->id,
                   (char *) ((chunk*)(node->data))->hex_hash);
        LOG("wanted now has %d chunks\n",
                   count_node(&wanted));
    }

    check_all_received();
}

void process_get(char *chunkfile, char *outputfile, bt_config_t *config) {
    LOG("haschunkfilename: (%s) outputfilename: (%s)\n",
               chunkfile, outputfile);
    user_output_filename = outputfile;
    user_get_chunk_file = chunkfile;

    /* check to see what chunks the user wants for output file */
    create_wanted_missing_list(chunkfile);

    /* send WHOHAS packets to all peers of the missing chunks */
    send_WHOHAS(config);
}

/*
  Setup self by parsing the .haschunkfile to create the chunks and then load
  the actual data chunks that map to chunks indicated in .haschunkfile
*/
static void peer_setup(bt_config_t *config) {
    LOG("peer_setup begin.\n");

    /* create the have chunk list with the given haschunk file */
    parse_chunkfile(&(have), config->has_chunk_file);

    /* load the have chunk data from masterchunk file */
    load_own_chunk_data(config);

    /* records the start time of user GET request */
    millitime(&start_time);
}

/*
  Read the master chunk file and extract the data chunk owned by self
*/
static void load_own_chunk_data(bt_config_t *config) {
    FILE* masterchunkfile_f;
    char line[FILE_LEN], master_chunk_filename[PATH_LEN];
    Node* node;
    chunk *c;

    LOG("get own chunk data from master.\n");

    masterchunkfile_f = fopen(config->chunk_file, "r");

    assert(masterchunkfile_f != NULL);

    if (fgets(line, FILE_LEN, masterchunkfile_f) != NULL) {
        /* parse the data file location from the master chunk file */
        sscanf(line, "File: %s\n", master_chunk_filename);

        /* read the have list and load each chunk data */
        for (node = have; node != NULL; node = node->next) {
            c = node->data;
            read_chunk_data_file_by_id(master_chunk_filename,
                                       c->id, c->chunk_data);

            LOG("Loaded chunk %d with hash %s\n",
                       c->id, (char *)c->hex_hash);
            c->bits_count = MAX_SEQ_NUM;
        }
    }
}

static void peer_run(bt_config_t *config) {
    struct sockaddr_in myaddr;
    fd_set readfds;
    struct user_iobuf *userbuf;
    struct timeval tv; /* timeouts for select */

    if ((userbuf = create_userbuf()) == NULL) {
        perror("peer_run could not allocate userbuf");
        exit(-1);
    }

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) {
        perror("peer_run could not create socket");
        exit(-1);
    }

    bzero(&myaddr, sizeof(myaddr));
    myaddr.sin_family = AF_INET;
    inet_aton("127.0.0.1", (struct in_addr *) &myaddr.sin_addr.s_addr);
    myaddr.sin_port = htons(config->myport);

    if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
        perror("peer_run could not bind socket");
        exit(-1);
    }

    spiffy_init(config->identity, (struct sockaddr *)&myaddr, sizeof(myaddr));

    while (1) {
        int nfds;
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);

        /* timeout for select */
        tv.tv_sec = 5; //TIMEOUT_THRESHOLD;
        tv.tv_usec = 0;

        nfds = select(sock+1, &readfds, NULL, NULL, &tv);

        if (nfds > 0) {
            if (FD_ISSET(sock, &readfds)) {
                process_inbound_udp(sock, config);
            }

            if (FD_ISSET(STDIN_FILENO, &readfds)) {
                process_user_input(config, STDIN_FILENO, userbuf,
                                   handle_user_input, "Currently unused");
            }
        } else if (nfds == 0) {
            /* if select time-outed, handle timeout of client and server side */
            handle_timeout(config);
        }
    }
}

/*********** Helper Functions ***********/
static void handle_timeout(bt_config_t *config) {
    handle_server_timeout(config);
    handle_client_timeout(config);
}

static void handle_user_input(bt_config_t *config, char *line, void *cbdata) {
    char *chunkf = (char *) calloc(128, sizeof(char));
    char *outf = (char *) calloc(128, sizeof(char));

    /* extracts the path of getchunk file and output file from user input */
    if (sscanf(line, "GET %120s %120s", chunkf, outf)) {
        if (strlen(outf) > 0) {
            process_get(chunkf, outf, config);
        }
    }
}

int compare_chunk_by_hex_wrapper(const void *c, const void *hash) {
    return compare_chunk_by_hex_hash((chunk *) c, (char *) hash);
}

int compare_chunk_by_bin_wrapper(const void *c, const void *hash) {
    return compare_chunk_by_bin_hash((chunk *) c, (uint8_t *) hash);
}
