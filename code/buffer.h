/*
  A buffer associates a video data transfer
*/

typedef struct {
        char* recv_buf; /* buffer of receiving request or response */
        char* send_buf; /* buffer of sending request or response */
        int recv_len; /* read_buf length have received so far */
        int send_len; /* total length of write_buf to be written */
} Buffer;
