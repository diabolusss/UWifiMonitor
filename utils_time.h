#ifndef __UTILS_TIME_H__
#define __UTILS_TIME_H__
	#include <time.h>

	void timespec_diff(struct timespec *result, struct timespec *start, struct timespec *stop);

	char timespec_gt(struct timespec *t1, struct timespec *t2);

#endif
