#ifndef BT_IO_H
#define BT_IO_H

#include <inttypes.h>
#include <stdlib.h>

#include "linkedlist.h"
#include "chunk.h"

#define FILE_LEN 1024
#define PATH_LEN 128

/*
  \fn read_master_file_by_id(filename, id)
  \brief Given the master filename, find the chunk that's associated with the id.

  \return
  	if successful: the number of bytes read into the buffer
    otherwise: -1 otherwise
 */
size_t read_chunk_data_file_by_id(char *filename, unsigned id, uint8_t *buf);

/**
 * Parse the getchunkfile and store the ids and hashes of the chunks need
 * to be fetched from other peers.

 @return if successful, a linked list of chunks struct; NULL otherwise.
 */
Node *parse_chunkfile(Node **list, char *chunkfile);
#endif
