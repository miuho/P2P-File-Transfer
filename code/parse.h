/*
  Parsing functions associates with input and output of stream
*/

#define LINE_SIZE 128
#define MAX_SOCK_NUM 64 /* the max number of sockets in browser and server list */

/* global variables */
Stream *current_steam = NULL; /* the current stream that receives and sends data */
int server_socks[MAX_SOCK_NUM]; /* a list of server socks */
int browser_socks[MAX_SOCK_NUM]; /* a list of browser socks */
int server_socks_count; /* records the number of server socks */
int browser_socks_count; /* records the number of browser socks */

/*
Add the socket to browser socket list if unseen before, else do nothing.
*/
void add_browser_sock_to_list(int socket);

/*
Add the socket to server socket list if unseen before, else do nothing.
*/
void add_server_sock_to_list(int socket);

/*
Returns 1 if the socket is browser socket, 0 otherwise.
*/
int is_browser(int socket);

/*
Returns 1 if the socket is server socket, 0 otherwise.
*/
int is_server(int socket);

/*
Parse the received data.
*/
void parse_data(int socket, proxy_config *config);