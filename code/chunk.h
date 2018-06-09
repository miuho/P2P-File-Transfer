#ifndef _CHUNK_H_
#define _CHUNK_H_
#include <stdio.h>
#include <inttypes.h>

#include "linkedlist.h"
#include "hash.h"
#include "bt_parse.h"
#include "packet.h"

#define BT_CHUNK_SIZE (512 * 1024)
#define DATA_SIZE 1000
#define MAX_SEQ_NUM (BT_CHUNK_SIZE / DATA_SIZE) + 1

#define ascii2hex(ascii,len,buf) hex2binary((ascii),(len),(buf))
#define hex2ascii(buf,len,ascii) binary2hex((buf),(len),(ascii))

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct chunk {
        unsigned int id; /* records the position of the chunk in file */
        char hex_hash[HEX_HASH_SIZE + 1]; /* records the hex hash of the chunk
                                           * (40 bytes + 1 for \0) */
        uint8_t bin_hash[BIN_HASH_SIZE]; /* bin hash of the chunk (20 bytes) */
        uint8_t chunk_data[BT_CHUNK_SIZE]; /* the actual data of the chunk */

        /* An array where the (i-1)th index represent whether or not the ith sequence
         * number has been received.

         example: if the seq_num 5 has been received, then chunk_bits[5-1] == 1.
         If it has not been received, then it should be 0.
        */
        uint8_t bits[MAX_SEQ_NUM];
        int bits_count;

        Node *peer;
    } chunk;

	/**
       Given a chunk id and hash, create a chunk structure to hold the data
       @return a dynamically allocated chunk struct if successful.  NULL otherwise.
    */
    chunk *create_chunk(uint8_t id, uint8_t *hash);

    /* Find the first available peer from peer linked list in a chunk. */
    bt_peer_t* find_first_available_peer(chunk* c);

    /* return 1 if the hex_hash matches the hex in chunk c; 0 otherwise */
    int compare_chunk_by_hex_hash(chunk *c, char *hex_hash);

    /* return 1 if the bin_hash matches the hex in chunk c; 0 otherwise */
    int compare_chunk_by_bin_hash(chunk *c, uint8_t *bin_hash);

    /* Returns the number of chunks created, return -1 on error */
    int make_chunks(FILE *fp, uint8_t **chunk_hashes);

    /* returns the sha hash of the string */
    void shahash(uint8_t *chr, int len, uint8_t *target);

    /* converts a hex string to ascii */
    void binary2hex(uint8_t *buf, int len, char *ascii);

    /* converts an ascii to hex */
    void hex2binary(char *hex, int len, uint8_t*buf);
#ifdef __cplusplus
}
#endif

#endif /* _CHUNK_H_ */
