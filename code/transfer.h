/*
  A transfer associates a chunk with a specific peer
  For every data chunk that need to be transfered, a transfer struct would be
  created.
*/

#ifndef TRANSFER_H
#define TRANSFER_H

#include "bt_parse.h"
#include "chunk.h"
#include "linkedlist.h"

#define TIMEOUT_THRESHOLD 3000 /* milliseconds */
#define MAX_TO_COUNTS 5 /* max timeouts before assuming peer is dead */

enum CongestType {SS, CA};

typedef struct {
    enum CongestType congest_state;

    /* congestion control variable */
    unsigned timeout_count,
        dup_count, /* record duplicate ACKs for flow control */
        last_received, /* last sequence received; uased for cumulative ACK */

    /* windows size variables */
        begin, /* beginning of the sliding window = LAST_ACK received */
        index, /* index into the sliding window = the LAST PACKET SENT; max is 7*/
        wind_size, /* congestion control window size */
        start_wind_size, /* records the window size when entering CA mode */
        rtt, /* records the round trip time of packet send and receive */
        ssthresh; /* slow start threshold */

    mytime_t rtt_timer; /* time since entering CA mode */
} CongestCtrl;

/* A connection that maintains the state of one data transfer
 */
typedef struct _transfer {
    bt_peer_t *peer;
    chunk *c;

    CongestCtrl cctrl;
    mytime_t timestamp, /* records the time spent of each data transfer */
        rtt; /* records the round trip time for CA mode */
} Transfer;

/*
  Check the timestamp field of the transfer and return 1 if it has timeout,
  0 otherwise
*/
int transfer_has_timeout(Transfer *transfer);

/*
  Return 1 if the transfer's chunk has the same hash as has, 0 otherwise
*/
int compare_transfer_by_chunk_hash(const void *transfer, const void *hash);

/*
  Return 1 if the transfer's peer has the same id as peer does, 0 otherwise */
int compare_transfer_by_peer_id(const void *transfer, const void *peer);

/*
  Create a new transfer struct and associate it with the peer and chunk passed
  in*/
Transfer *create_transfer(bt_peer_t *peer, chunk *c);

/*
  Delete a transfer, and set the peer to available. Returns 1 if successful,
  0 otherwise.
*/
int delete_transfer(void *ptr);

#endif
