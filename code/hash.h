#ifndef HASH_H
#define HASH_H

#include <inttypes.h>

#define HEX_HASH_SIZE 40
#define BIN_HASH_SIZE 20

/**
  Compare two binary hashes and two hex hashes, respectively.
  @return 1 if equal, 0 otherwise.
*/
int compare_bin_hash(uint8_t* a, uint8_t* b);
int compare_hex_hash(char* a, char* b);

#endif
