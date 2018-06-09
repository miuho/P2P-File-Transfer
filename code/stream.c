#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "parse.h"
#include "stream.h"
#include "buffer.h"
#include "bitrate.h"

Stream *create_stream() {
        Stream *stream = calloc(1, sizeof(Stream));
        assert(stream != NULL);

        /* initiate buffer for http request */
        Buffer *request_buffer = calloc(1, sizeof(Buffer));
        assert(request_buffer != NULL);
        stream->request_buffer = request_buffer;

        /* initiate buffer for http response */
        Buffer *response_buffer = calloc(1, sizeof(Buffer));
        assert(response_buffer != NULL);
        stream->response_buffer = response_buffer;

        return stream;
}

void dump_to_stream(int socket, char *buffer, int bytes_received,
                       proxy_config *config){
        Buffer *buffer;
        char *new_recv_buf;
        int buf_size;

        /* If socket is unseen before, add it to the browser_socks */
        /* all unseen sockets here are treated as new browser sockets */
        add_browser_sock_to_list(socket);

        /* differentiate the received data is http request or response */
        if (is_browser(socket)) {
                buffer = (new_stream->request_buffer);
        } else {
                assert(is_server(socket));
                buffer = (new_stream->response_buffer);
        }

        /* dynamically allocate memory for recv_buf for every recv calls */
        /* avoid if recv_len + bytes_received > previously defined buf_size */
        new_recv_buf = calloc(buffer->recv_len + bytes_received, sizeof(char));
        assert(new_recv_buf != NULL);
        if (buffer->recv_len != 0) {
                /* move previous data in old recv buffer to new recv buffer*/
                memcpy(new_recv_buf, buffer->recv_buf, buffer->recv_len);
        }
        /* move the new received data to the new recv buffer */
        memcpy(new_recv_buf + buffer->recv_len, buffer, bytes_received);
        buffer->recv_len += bytes_received;
        buffer->recv_buf = new_recv_buf;

        /* parse the received data */
        parse_data(socket, config);

        return;
}

void init_stream(proxy_config *config, int socket) {
        int server_sock;
        char *normal_manifest;

        /* initialize the current_steam and socket lists */
        if (current_steam == NULL) {
                current_steam = create_stream();
                init_sock_lists();
        }

        /* send the normal manifest version request to server, so that proxy */
        /* can parse the bitrates */
        normal_manifest = "GET /vod/big_buck_bunny.f4m  HTTP/1.0\r\n\r\n";
        //dump_to_proxy(socket, normal_manifest, strlen(normal_manifest));
        //server_sock = set_server();
        assert(socket == server_sock);
        add_server_sock_to_list(server_sock);
}