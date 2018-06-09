#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "bt_parse.h"

#define USERBUF_SIZE 8191

struct user_iobuf {
  char *buf;
  unsigned int cur;
};

struct user_iobuf *create_userbuf();

void process_user_input(bt_config_t *config, int fd, struct user_iobuf *userbuf,
			void (*handle_line)(bt_config_t *, char *, void *),
			void *cbdata);
