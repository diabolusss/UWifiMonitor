#include "utils_time.h"

void timespec_diff(struct timespec *result, struct timespec *start, struct timespec *stop)	{
    if ((result->tv_nsec = stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec += 1000000000;

    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    return;
}

char timespec_gt(struct timespec *t1, struct timespec *t2){
	if (t1->tv_sec < t2->tv_sec)
        return (-1) ;				/* Less than. */

    else if (t1->tv_sec > t2->tv_sec)
        return (1) ;				/* Greater than. */

    else if (t1->tv_nsec < t2->tv_nsec)
        return (-1) ;				/* Less than. */

    else if (t1->tv_nsec > t2->tv_nsec)
        return (1) ;				/* Greater than. */

    else
        return (0) ;				/* Equal. */
}
