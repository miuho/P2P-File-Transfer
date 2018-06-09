#include "hash.h"

int compare_bin_hash(uint8_t* a, uint8_t* b) {
    int i;
    /* check if every 8 bits is equal */
    for (i = 0; i < BIN_HASH_SIZE; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}

int compare_hex_hash(char* a, char* b) {
    int i;
    /* check if every character is equal */
    for (i = 0; i < HEX_HASH_SIZE; i++) {
        if (a[i] != b[i]) {
            return 0;
        }
    }
    return 1;
}
