#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include "input_buffer.h"

void printline(bt_config_t *config, char *line, void *cbdata) {
    config = config;
    printf("LINE:  %s\n", line);
    printf("CBDATA:  %s\n", (char *)cbdata);
}


int main() {


    struct user_iobuf *u;

    u = create_userbuf();
    assert(u != NULL);

    while (1) {
        process_user_input(NULL, STDIN_FILENO, u, printline, "Cows moo!");
    }

    return 0;
}
