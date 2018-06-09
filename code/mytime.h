#ifndef MYTIME_H
#define MYTIME_H

#include <sys/time.h>
#include <stdlib.h>

/* return the time since epoch in milliseconds:
   Code reference: https://code.google.com/p/lz4/issues/attachmentText?id=39&aid=390001000&name=gettimeofday.patch&token=NCIqhNChcHl0ht7Qtbu8reNw0jA%3A1383498967479
 */
typedef unsigned long mytime_t;

mytime_t millitime(mytime_t *);

#endif
