#include <assert.h>

#include "congestion.h"
#include "log.h"
#include "peer.h"

#define MAX(a,b) (a < b ? b : a)

/*
  SS mode increments the window size per ACK packet received and switches
  to CA mode if the window size reaches ssthresh.
*/
static void slow_start(Transfer *transfer);
/*
  CA mode increments the window size per round-trip time.
*/
static void congestion_avoidance(Transfer *transfer);

void congestion_control(Transfer *transfer) {
    switch((transfer->cctrl).congest_state) {
        case SS:
            slow_start(transfer);
            break;
        case CA:
            congestion_avoidance(transfer);
            break;
        default:
            break;
    }
}

static void slow_start(Transfer *transfer) {
    /* initialize the round trip time for later in congestion avoidance mode */
    if ((transfer->cctrl).rtt == 0) {
        (transfer->cctrl).rtt = millitime(NULL) - (transfer->timestamp);
    }

    /* increment the window size if the window size is below ssthresh */
    if ((transfer->cctrl).wind_size < (transfer->cctrl).ssthresh) {
        (transfer->cctrl).wind_size *= 2;
        graph_wind_size(transfer);
    }

    /* switch to congestion avoidance mode if window size reaches ssthresh */
    else {
        LOG("SSThresh reached, changed to CA.\n");
        (transfer->cctrl).congest_state = CA;
        millitime(&((transfer->cctrl).rtt_timer));
        (transfer->cctrl).start_wind_size = (transfer->cctrl).wind_size;
    }
}

void detected_first_loss(Transfer *transfer) {
    /* set ssthresh to max(windowsize/2, 2) before enter congestion */
    /* avoidance mode */

    (transfer->cctrl).ssthresh = MAX((transfer->cctrl).wind_size / 2, 2);

    if ((transfer->cctrl).congest_state == SS) {
        LOG("Switch to CA.\n");
        (transfer->cctrl).congest_state = CA;
        millitime(&((transfer->cctrl).rtt_timer));
        (transfer->cctrl).start_wind_size = (transfer->cctrl).wind_size;
    }
    else {
        (transfer->cctrl).wind_size = 1;
        (transfer->cctrl).congest_state = SS;

        graph_wind_size(transfer);
        LOG("Switch to SS, ssthresh %d.\n", (transfer->cctrl).ssthresh);
    }
}

static void congestion_avoidance(Transfer *transfer) {
    /* increments the window size at most by one packet per RRT */
    mytime_t new_wind_size;

    if (((transfer->cctrl).rtt)) {
        new_wind_size = ((millitime(NULL) - ((transfer->cctrl).rtt_timer))
                         / ((transfer->cctrl).rtt))
                        + (transfer->cctrl).start_wind_size;

        if ((transfer->cctrl).wind_size != new_wind_size) {
            (transfer->cctrl).wind_size = new_wind_size;
            graph_wind_size(transfer);
        }
    }
}

void graph_wind_size(Transfer *transfer) {
    GRAPH("T%u\t%lu\t%u\n", transfer->peer->id,
          millitime(NULL) - start_time, transfer->cctrl.wind_size);
}
