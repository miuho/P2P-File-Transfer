/*
  A representation of a data transfer in action
*/

#include <inttypes.h>

#include "transfer.h"
#include "chunk.h"
#include "congestion.h"
#include "mytime.h"
#include "log.h"

#define RTT 200 /* milliseconds */

int transfer_has_timeout(Transfer *transfer) {
    int timedout = (millitime(NULL) - transfer->timestamp) > TIMEOUT_THRESHOLD;

    /* check if the transfer has time-outed, and log the info for time-outs */
    if (timedout) {
        LOG("Transfer to peer (%u) for chunk (%u) has timedout.\n",
            transfer->peer->id, transfer->c->id);
    }

    return timedout;
}

int delete_transfer(void *ptr) {
    Transfer *transfer = (Transfer *) ptr;

    if (transfer == NULL) {
        return EXIT_FAILURE;
    }

    /* turns the peer available sign on, be ready for next data transfer */
    transfer->peer->available = 1;
    free(transfer);
    return EXIT_SUCCESS;
}

Transfer *create_transfer(bt_peer_t *peer, chunk *c) {
    Transfer *transfer = calloc(1, sizeof(Transfer));

    if (transfer == NULL) {
        return NULL;
    }

    transfer->peer = peer;
    transfer->c = c;
    transfer->timestamp = millitime(NULL);

    /* congestion control variables */
    transfer->cctrl.wind_size = DEFAULT_WIND_SIZE;
    transfer->cctrl.last_received = 0;
    transfer->cctrl.begin = 0;
    transfer->cctrl.index = 0;
    transfer->cctrl.dup_count = 0;
    transfer->cctrl.timeout_count = 0;
    transfer->cctrl.ssthresh = DEFAULT_SS_THRESH;
    transfer->cctrl.congest_state = SS;
    transfer->cctrl.rtt = RTT; /* a fixed RTT for CA window size calculation */

    /* the peer is no longer available for another new data transfer */
    peer->available = 0;

    return transfer;
}

int compare_transfer_by_chunk_hash(const void *transfer, const void *bin_hash) {
    Transfer *t = (Transfer *) transfer;
    uint8_t *h = (uint8_t *) bin_hash;

    if (t == NULL || h == NULL) {
        return 0;
    }

    return compare_chunk_by_bin_hash(t->c, h);
}

int compare_transfer_by_peer_id(const void *transfer, const void *peer) {
    Transfer *t = (Transfer *) transfer;
    bt_peer_t *p = (bt_peer_t *) peer;

    if (t == NULL || p == NULL) {
        return 0;
    }

    return t->peer->id == p->id;
}
