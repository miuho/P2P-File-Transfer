/*
  Bitrate associates with the modifying the uri request for video fragments
*/

#define MAX_BITRATES_NUM 32 /* assume number of bitrates available is <= 32 */

/*
Extract the bitrates info from the http response of manifest file.
*/
void extract_bitrates_from_response(Buffer *buffer);

/*
Returns the throughput for the video fragment.
*/
int calculate_throughput(int frag_size, proxy_config *config);

/*
Returns the lowest bitrate available of all bitrates.
*/
int lowest_bitrate();

/*
Modify the bitrate of the http request in the send_buf to fittest bitrate.
*/
void modfiy_bitrate(Buffer *buffer);

/*
Modify the http request for normal version of manifest file to nolist version
of manifest file for the proxy to parse the available bitrates.
*/
void normal_to_nolist_manifest(Buffer *buffer);