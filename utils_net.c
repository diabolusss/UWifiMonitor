#include <stdio.h>
#include <string.h>

#include "utils_net.h"

const char* ether_sprintf(const unsigned char *mac){
	static char etherbuf[18];
	snprintf(etherbuf, sizeof(etherbuf), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return etherbuf;
}

const char* ether_sprintf_short(const unsigned char *mac){
	static char etherbuf[5];
	snprintf(etherbuf, sizeof(etherbuf), "%02x%02x",
		mac[4], mac[5]);
	return etherbuf;
}

const char* ip_sprintf(const unsigned int ip){
	static char ipbuf[18];
	unsigned char* cip = (unsigned char*)&ip;
	snprintf(ipbuf, sizeof(ipbuf), "%d.%d.%d.%d",
		cip[0], cip[1], cip[2], cip[3]);
	return ipbuf;
}

const char* ip_sprintf_short(const unsigned int ip){
	static char ipbuf[5];
	unsigned char* cip = (unsigned char*)&ip;
	snprintf(ipbuf, sizeof(ipbuf), ".%d", cip[3]);
	return ipbuf;
}

void convert_string_to_mac(const char* string, unsigned char* mac){
	int c;
	for(c = 0; c < 6 && string; c++) {
		int x = 0;
		if (string)
			sscanf(string, "%x", &x);
		mac[c] = x;
		string = strchr(string, ':');
		if (string)
			string++;
	}
}

