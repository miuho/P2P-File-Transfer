#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "parse.h"
#include "stream.h"
#include "buffer.h"
#include "bitrate.h"

/*
Change HTTP/1.1 response to HTTP/1.0 response.
*/
static void http_1_to_0(char *buffer) {
        char *http_num;

        http_num = strstr(buffer->send_buf, "HTTP/1.");
        /* all response should have http number */
        assert(http_num != NULL);

        http_num[http_num + strlen("HTTP/1.")] = '0';
        /* confirms this response is http 1.0 */
        assert(strstr(buffer->send_buf, "HTTP/1.0") != NULL);
}

/*
Initialize the lists of storing server and browser socket fds.
*/
static void init_sock_lists(void) {
        int i;

        server_socks_count = 0;
        browser_socks_count = 0;
        for (i = 0; i < MAX_SOCK_NUM; i++) {
                server_socks[i] = 0;
                browser_socks[i] = 0;
        }
}

void add_browser_sock_to_list(int socket) {
        if ((!is_browser(socket)) && (!is_server(socket))) {
                browser_socks[browser_socks_count] = socket;
                browser_socks_count++;
        }
}

void add_server_sock_to_list(int socket) {
        if ((!is_server(socket))) {
                server_socks[server_socks_count] = socket;
                server_socks_count++;
        }
}

int is_browser(int socket) {
        int i;

        for (i = 0; i < browser_socks_count; i++) {
                if (browser_socks[i] == socket) {
                        return 1;
                }
        }

        return 0;
}

int is_server(int socket) {
        int i;

        for (i = 0; i < server_socks_count; i++) {
                if (server_socks[i] == socket) {
                        return 1;
                }
        }

        return 0;
}

/*
Erases the CRLF(s) in the beginning of the received data, and returns 1 if
a complete header is received, 0 otherwise.
*/
static int complete_header_received(Buffer *buffer) {
        char* first_CRLF;
        char* header_end;

        /* In the interest of robustness, servers SHOULD ignore any empty */
        /* line(s) received where a header is expected, so erase the */
        /* CRLF before the header */
        first_CRLF = strstr(buffer->recv_buf, "\r\n");
        while (buffer->recv_buf == first_CRLF && buffer->recv_len >= 2) {
                buffer->recv_len -= 2;
                memmove(buffer->recv_buf, buffer->recv_buf + 2,
                        buffer->recv_len);
                first_CRLF = strstr(buffer->recv_buf, "\r\n");
        }

        /* the data is full of "\r\n", ignore them, receive more bytes */
        if (buffer->recv_len < 2) {
                return 0;
        }

        /* now its valid to assume that the first 2 bytes is not "\r\n" */
        header_end = strstr(buffer->recv_buf, "\r\n\r\n");
        if (header_end == NULL) {
                /* the header hasnt been fully received yet, wait till next */
                /* recv call and check again. */
                return 0;
        }

        return 1;
}

/*
Extract the string from begin_loc (inclusive) to end_loc (exclusive)
*/
static char *extract_string(char *begin_loc, char *end_loc) {
        char *data;
        int h;
        int data_len;

        data = calloc(LINE_SIZE, sizeof(char));
        data_len = end_loc - begin_loc;
        assert(data != NULL);
        for (h = 0; h < data_len; h++) {
                data[h] = begin_loc[h];
        }
        data[h] = '\0';

        return data;
}

/*
Returns the character between field_name and data_end in the first header, or
NULL if field_name does not exists in the first header of the buffer.
*/
static char *extract_data_from_header(char *buffer, char *field_name,
                                      char *data_end) {
        char* field_name_begin;
        char* first_header_end;
        int field_name_len;
        char *field_data;

        /* with the assumption that the complete header has been receieved */
        field_name_begin = strstr(buffer, field_name);
        first_header_end = strstr(buffer, "\r\n\r\n");
        if (field_name_begin == NULL ||
            field_name_begin > first_header_end) {
                /* if content length field does not present, or the first */
                /* appearance of content length field is not in the first */
                /* header, both mean that no body is present in this first */
                /* request or response, so return NULL */
                return NULL;
        } else {
                /* the content length field presents, and it is in the first */
                /* header. Then check if the complete body is received */
                field_name_len = strlen(field_name);
                field_data = extract_string(field_name_begin + field_name_len,
                                            strstr(field_name_begin, data_end));
                assert(field_data != NULL);

                return field_data;
        }
}

/*
Returns the length of the first http request or response's body.
*/
static int first_body_length(char *buffer) {
        char *content_len_data;

        content_len_data = extract_data_from_header(buffer, "Content-Length: ",
                                                    "\r\n");

        /* if body doesnt present, body length is 0 */
        if (content_len_data == NULL) {
                return 0;
        } else {
                return atoi(content_len_data);
        }
}

/*
Returns the length of the first complete http request or response.
*/
static int first_message_length(char *buffer) {
        int first_header_len;
        char *first_header_end;

        first_header_end = strstr(buffer, "\r\n\r\n");
        first_header_len = first_header_end - buffer + strlen("\r\n\r\n");
        assert(first_header_len > 0);

        return first_header_len + first_body_length(buffer);
}

/*
Returns 0 if message body is incomplete, and returns 1 if a complete body is
received or if no body is embedded in the message.
*/
static int complete_body_received(Buffer *buffer) {
        int complete_message_len;

        complete_message_len = first_message_length(buffer->recv_buf);

        return (buffer->recv_len >= complete_message_len);
}

/*
moves the first complete request or response from recv_buf to send_buf.
*/
static void move_data_from_recv_to_send(Buffer *buffer) {
        int message_len;

        message_len = first_message_length(buffer->recv_buf);
        buffer->send_buf = calloc(message_len, sizeof(char));
        /* copies the first message from recv_buf to send_buf */
        memcpy(buffer->send_buf, buffer->recv_buf, message_len);

        buffer->recv_len -= message_len;
        assert(buffer->recv_len >= 0);
        buffer->send_len = message_len;

        /* cleanup the recv_buf, shift the remaining  data to the front */
        memmove(buffer->recv_buf, (buffer->recv_buf) + message_len,
                buffer->recv_len);
}

/*
Parse the http request.
*/
static void parse_request(Buffer *buffer, int socket) {
        char *seq_num;
        char *frag_num;
        int server_sock;

        time(current_steam->t_start);

        seq_num = extract_data_from_header(buffer->send_buf, "Seq", "-");
        frag_num = extract_data_from_header(buffer->send_buf, "Frag", " ");
        /* seq_num and frag_num should present together, not individually */
        assert((seq_num == NULL && frag_num == NULL) ||
               (seq_num != NULL && frag_num != NULL));

        if (strstr(buffer->send_buf, "f4m") != NULL) {
                /* if this a http GET request from browser for manifest file, */
                /* it should be modified from the normal version to the nolist */
                /* version, so browser always get the nolist manifest response */
                normal_to_nolist_manifest(buffer);
        } else if (seq_num != NULL && frag_num != NULL) {
                /* this is a http GET request for fragments of video chunk */
                /* modify the bitrate of the uri in the request */
                modfiy_bitrate(buffer);
        }
        /* if this is http GET request for HTML, SWF or f4m files */
        /* do nothing and simply forwards it to server */

        //dump_to_proxy(socket, buffer->send_buf, buffer->send_len);
        //server_sock = set_server();
        add_server_sock_to_list(server_sock);
        free(buffer->send_buf);
        buffer->send_len = 0;
}

/*
Parse the http response.
*/
static void parse_response(Buffer *buffer, proxy_config *config, int socket) {
        int frag_size;

        /* change HTTP/1.1 response to HTTP/1.0 response to avoid pipelining */
        http_1_to_0(buffer->send_buf);

        if (current_steam->throughput == 0) {
                /* set throughput to lowest bitrate in the beginning, and */
                /* extract bitrate from the normal manifest version */
                extract_bitrates_from_response(buffer);
                current_steam->throughput = lowest_bitrate();
                /* discard the normal manifest response */
                return;
        } else {
                /* stop the timestamp for the video fragment */
                time(current_steam->t_final);
                frag_size = first_body_length(buffer->send_buf);
                assert(frag_size > 0);

                /* calcualte the moveing average of the throughput */
                current_steam->throughput = calculate_throughput(frag_size,
                                                                 config);
        }

        //dump_to_proxy(socket, buffer->send_buf, buffer->send_len);
        free(buffer->send_buf);
        buffer->send_len = 0;
}

void parse_data(int socket, proxy_config *config) {
        Buffer *buffer;

        /* parse the request or response buffer */
        if (is_browser(socket)) {
                buffer = (current_steam->request_buffer);
        } else {
                assert(is_server(socket));
                buffer = (current_steam->response_buffer);
        }

        if (!complete_header_received(buffer)) {
                /* the received data is not ready to be parsed yet */
                return;
        } else if (!complete_body_received(buffer)) {
                /* the received data is not ready to be parsed yet */
                return;
        }

        /* moves the first complete request or response from recv_buf to */
        /* send_buf */
        move_data_from_recv_to_send(buffer);

        /* parse the received completed http request or response */
        if (is_browser(socket)) {
                /* received a http request */
                parse_request(buffer, socket);
                return;
        } else {
                assert(is_server(socket));
                /* received a http response */
                parse_response(buffer, config, socket);
                return;
        }
}