#ifndef __UTILS_CUSTOM_H__
#define __UTILS_CUSTOM_H__

	#define max(_x, _y) ((_x) > (_y) ? (_x) : (_y))
	#define min(_x, _y) ((_x) < (_y) ? (_x) : (_y))

	void generate_mon_ifname(char *const buf, const size_t buf_size);
#endif
