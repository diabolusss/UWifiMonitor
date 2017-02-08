#include <stdarg.h> //va_list, va_start, ...
#include <stdio.h>
#include <net/if.h>//if_nametoindex

#include "io_log.h"
#include "utils_custom.h"
#include "util.h" //LOG_{} constants

void generate_mon_ifname(char *const buf, const size_t buf_size){
	unsigned int i;

	for (i=0;; ++i) {
		int len;

		len = snprintf(buf, buf_size, "horst%d", i);
		if (len < 0)
			printwrn("failed to generate monitor interface name");
		if ((unsigned int) len >= buf_size)
			printwrn("failed to generate a sufficiently short "
			     "monitor interface name");
		if (!if_nametoindex(buf))
			break;  /* interface does not exist yet, done */
	}
}
