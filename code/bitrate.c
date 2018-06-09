#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#include "parse.h"
#include "stream.h"
#include "buffer.h"
#include "bitrate.h"

void extract_bitrates_from_response(Buffer *buffer) {
        char buf_copy[buffer->send_len];
        char *first_bitrate_loc;
        char * field_end_loc;

        current_steam->bitrates_count = 0;
        /* creates a local copy of the response to free to play with it */
        memcpy(buf_copy, buffer->send_buf, buffer->send_len);

        first_bitrate_loc = strstr(buf_copy, "bitrate=");
        while (first_bitrate_loc != NULL) {
                field_end_loc = strstr(first_bitrate_loc, "\"");
                current_steam->bitrates[bitrates_count] =
                        atoi(extract_string(first_bitrate_loc +
                                            strlen("bitrate=\""),
                                            field_end_loc));
                /* change "bitrate=" to "xitrate=" so that next call of */
                /* strstr for "bitrate=" will find the next bitrate */
                first_bitrate_loc[0] = 'x';
                first_bitrate_loc = strstr(buf_copy, "bitrate=");
                current_steam->bitrates_count++;
                assert(current_steam->bitrates_count <= MAX_BITRATES_NUM);
        }

        /* there should be at least one bitrate available for the video */
        assert(current_steam->bitrates_count > 0);
}

int calculate_throughput(int frag_size, proxy_config *config) {
        int time_spent;
        double new_throughput;
        double cur_throughput;

        time_spent = current_steam->t_final - current_steam->t_start;
        assert(time_spent > 0);

        /* multiply 8 to change from bytes to bits */
        new_throughput = (double)((frag_size * 8) / time_spent);

        cur_throughput = (config->alpha)*new_throughput +
                (1 - (config->alpha))*(current_steam->throughput);

        return (int)floor(cur_throughput);
}

int lowest_bitrate() {
        int lowest_bitrate;
        int i;

        lowest_bitrate = current_steam->bitrates[0];
        for (i = 0; i < current_steam->bitrates_count; i++) {
                if (current_steam->bitrates[i] < lowest_bitrate) {
                        lowest_bitrate = current_steam->bitrates[i];
                }
        }

        return lowest_bitrate;
}

/*
Returns the highest bitrate available under the calculated bitrate.
*/
static int highest_bitrate_under(int bitrate) {
        int highest_bitrate;
        int i;

        highest_bitrate = lowest_bitrate();
        for (i = 0; i < current_steam->bitrates_count; i++) {
                if (current_steam->bitrates[i] <= bitrate &&
                    current_steam->bitrates[i] > highest_bitrate) {
                        highest_bitrate = current_steam->bitrates[i];
                }
        }

        return highest_bitrate;
}

/*
Choose the bitrate corresponding to the current throughput calculation.
*/
static char *choose_bitrate() {
        int bitrate;
        int highest_matched_bitrate;
        char *tmp;

        /* the average throughout should be at least 1.5 times of the bitrate */
        /* so bitrate should be 2/3 of the throughput */
        bitrate = 2 * ((current_steam->throughput) / 3);

        /* bitrate is read from manifest file in the unit of Kbps, and it is */
        /* now in bps, so it should be divided by 1000 */
        highest_matched_bitrate = highest_bitrate_under(bitrate / 1000);

        tmp = calloc(LINE_SIZE, sizeof(char));
        assert(tmp != NULL);
        sprintf(tmp, "%d", highest_matched_bitrate);

        return tmp;
}

void modfiy_bitrate(Buffer *buffer) {
        char *bitrate;
        char *end_loc;
        char *begin_loc;
        char *new_send_buf;
        int first_part_len;
        int last_part_len;

        /* find the end location (exclusive) of the bitrate field in uri */
        end_loc = strstr(buffer->send_buf, "Seq");
        assert(end_loc != NULL);

        /* find the begin location (inclusive) of the bitrate field in uri */
        begin_loc = end_loc;
        while (begin_loc[0] != '/' && begin_loc >= (buffer->send_buf)) {
                begin_loc--;
        }
        assert(begin_loc[0] == '/');
        begin_loc++;

        /* this is the bitrate selected to replace the bitrate in the uri */
        bitrate = choose_bitrate();

        /* calculate the new send_len */
        first_part_len = begin_loc - (buffer->send_buf);
        last_part_len = buffer->send_len - (end_loc - buffer->send_buf);
        buffer->send_len = first_part_len + strlen(bitrate) + last_part_len;

        /* creates the new send_buf */
        new_send_buf = calloc(buffer->send_len, sizeof(char));
        /* copies over first part of the http request */
        memcpy(new_send_buf, buffer->send_buf, first_part_len);
        /* replace the part of the bitrate */
        memcpy(new_send_buf + first_part_len, bitrate, strlen(bitrate));
        /* copes over the last part of the http request */
        memcpy(new_send_buf + first_part_len + strlen(bitrate),
               end_loc, last_part_len);
        free(buffer->send_buf);
        buffer->send_buf = new_send_buf;
}

void normal_to_nolist_manifest(Buffer *buffer) {
        char *insert_loc;
        char *new_send_buf;
        int first_part_len;
        int last_part_len;

        /* find the insert location of "_nolist" in uri */
        insert_loc = strstr(buffer->send_buf, ".f4m");
        assert(insert_loc != NULL);

        /* calculate the new send_len */
        buffer->send_len += strlen("_nolist");

        /* creates the new send_buf */
        new_send_buf = calloc(buffer->send_len, sizeof(char));
        /* copies over first part of the http request */
        first_part_len = insert_loc - (buffer->send_buf);
        memcpy(new_send_buf, buffer->send_buf, first_part_len);
        /* insert the part of "_nolist" */
        memcpy(new_send_buf + first_part_len, "_nolist", strlen("_nolist"));
        /* copes over the last part of the http request */
        last_part_len = buffer->send_len - strlen("_nolist") -
                        (insert_loc - buffer->send_buf);
        memcpy(new_send_buf + first_part_len + strlen("_nolist"),
               insert_loc, last_part_len);
        assert(buffer->send_len == first_part_len + strlen("_nolist") +
                                   last_part_len);
        free(buffer->send_buf);
        buffer->send_buf = new_send_buf;
}