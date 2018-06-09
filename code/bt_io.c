#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>
#include <string.h>

#include "chunk.h"
#include "bt_io.h"
#include "debug.h"
#include "hash.h"
#include "log.h"

size_t read_chunk_data_file_by_id(char *filename, unsigned id, uint8_t *buf) {
    FILE *file;
    unsigned long offset;
    size_t bytes_read;

    LOG("reading id (%u) file: %s\n", id,
               filename);

    if (filename == NULL) {
        return -1;
    }

    file = fopen(filename, "rb");
    if (file == NULL) {
        LOG("Failed to open chunk data file"
                   "%s\n", filename);
        return -1;
    }

    /* calculate data offset by id */
    offset = id * BT_CHUNK_SIZE;
    fseek(file, offset, SEEK_SET);

    /* read in a chunk file */
    bytes_read = fread(buf, sizeof(uint8_t), BT_CHUNK_SIZE, file);

    LOG("read %lu bytes from file %s.\n",
               bytes_read, filename);

    if (bytes_read < BT_CHUNK_SIZE) {
        if (feof(file)) {
            fclose(file);
            return bytes_read;
        } else {
            LOG("File read error.");
            free(buf);
            fclose(file);
            return -1;
        }
    }

    fclose(file);
    return bytes_read;
}

Node *parse_chunkfile(Node **list, char *chunkfile) {
    FILE *f;
    int id;
    char line[FILE_LEN], hash[HEX_HASH_SIZE];
    chunk *c;

    /* open the getchunkfile */
    f = fopen(chunkfile, "r");
    assert(f != NULL);

    /* parse the "id chunk_hash\n" and store into chunks */
    while (fgets(line, FILE_LEN, f) != NULL) {
        if (sscanf(line, "%d %s\n", &id, hash) < 2) {
            fclose(f);
            return *list;
        }

        c = create_chunk(id, (uint8_t *) hash);
        assert(c != NULL);
        node_insert(list, c);
    }

    fclose(f);
    return *list;
}
