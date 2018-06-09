#include "mytime.h"

mytime_t millitime(mytime_t *time) {
    struct timeval tv;
    mytime_t nCount;

    gettimeofday(&tv, NULL);
    nCount = (mytime_t) (tv.tv_usec/1000 + (tv.tv_sec & 0xfffff) * 1000);

    if (time != NULL) {
        *time = nCount;
    }

    return nCount;
}
