/*
  congestion.h

  Handles congestion control:
  -- Slow Start
  -- Congestion Control
  -- Fast Retransmit
*/

#ifndef CONGESTION_H
#define CONGESTION_H

#include "transfer.h"

#define DEFAULT_WIND_SIZE 1
#define DEFAULT_SS_THRESH 64

/*
  Check the transfer's congestion mode and switch between SS or CA mode.
*/
void congestion_control(Transfer *transfer);

/*
  Reset the sstthresh for the transfer after detecting a loss, and switch from
  SS to CA mode or CA to SS mode.
*/
void detected_first_loss(Transfer *transfer);

/*
  Records the peer id, time elapsed, and current congestion window size
  to log file.
*/
void graph_wind_size(Transfer *transfer);

#endif
