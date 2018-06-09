/*
  A stream associates a video data transfer
*/

#include <time.h>

#include "buffer.h"
#include "bitrate.h"

typedef struct {
        time_t t_start, t_final; /* the start time and end time for a chunk */
        int bitrates[MAX_BITRATES_NUM]; /* bitrates available for this video */
        int bitrates_count; /* the number of bitrates read from manifest */
        int throughput; /* the current throughput for the chunk */
        Buffer *request_buffer; /* buffer to write and read http requests */
        Buffer *response_buffer; /* buffer to write and read http requests */
} Stream;

/*
  Create a new Stream
*/
Stream *create_stream();

/*
Prepare the forwarding data for proxy.
*/
void dump_to_stream(int socket, char *buffer, int bytes_received,
                       proxy_config *config);

/*
Generate a normal version manifest file request to server.
*/
void init_stream(proxy_config *config, int socket);