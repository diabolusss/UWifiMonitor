#include <stdarg.h> //va_list, va_start, ...
#include <stdio.h>
#include <string.h>

#include "io_log.h"
#include "utils_net.h"
#include "util.h" //LOG_{} constants
#include "main.h"//conf.debug application specific

static FILE* DF = NULL;

void __attribute__ ((format (printf, 2, 3))) printlog_(const char *appender, const char *fmt, ...){
	char buf[128];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, 128, fmt, args);
	va_end(args);

	if(strcmp(appender, "")==0){
		fprintf(stderr, "%s", buf);

	}else if( strcmp(appender, "[DBG]")==0 ){
		if(conf.debug)
			fprintf(stderr, "%s", buf);

	}else if ( strcmp(appender, "DBG")==0 ){
		if(conf.debug)
			fprintf(stderr, "[%s]:%s",appender, buf);

	}else	
		fprintf(stderr, "[%s]:%s",appender, buf);
}

void __attribute__ ((format (printf, 2, 3))) printlog(int level, const char *fmt, ...){
	char buf[128];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, 128, fmt, args);
	va_end(args);

	char appender[4];
	switch(level){
		case LOG_DEBUG:		strncpy(appender,"DBG",4); break;
		case LOG_INFO:		strncpy(appender,"INF",4); break;
		case LOG_WARNING:	strncpy(appender,"WRN",4); break;
		case LOG_ERR:		strncpy(appender,"ERR",4); break;
		case LOG_CRIT:		strncpy(appender,"CRT",4); break;
	}

	if(level <= LOG_LEVEL)
		fprintf(stderr, "[%s]:%s",appender, buf);
}

void dumpfile_open(const char* name){
	if (DF != NULL) {
		fclose(DF);
		DF = NULL;
	}

	if (name == NULL || strlen(name) == 0) {
		printinf("- Not writing outfile");
		conf.dumpfile[0] = '\0';
		return;
	}

	strncpy(conf.dumpfile, name, MAX_CONF_VALUE_STRLEN);
	conf.dumpfile[MAX_CONF_VALUE_STRLEN] = '\0';
	DF = fopen(conf.dumpfile, "w");
	if (DF == NULL)
		printwrn("Couldn't open dump file %s", name);

	fprintf(DF, "TIME, WLAN TYPE, MAC SRC, MAC DST, BSSID, PACKET TYPES, SIGNAL, ");
	fprintf(DF, "LENGTH, PHY RATE, FREQUENCY, TSF, ESSID, MODE, CHANNEL, ");
	fprintf(DF, "WEP, WPA1, RSN (WPA2), IP SRC, IP DST\n");

	printinf("- Writing to outfile %s", conf.dumpfile);
}

void dumpfile_close(){
	if (DF != NULL) {
		fclose(DF);
		DF = NULL;
	}
}

void dumpfile_write(struct uwifi_packet* p){
	char buf[40];
	int i;
	struct tm* ltm = localtime(&the_time.tv_sec);

	//timestamp, e.g. 2015-05-16 15:05:44.338806 +0300
	i = strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", ltm);
	i += snprintf(buf + i, sizeof(buf) - i, ".%06ld", (long)(the_time.tv_nsec / 1000));
	i += strftime(buf + i, sizeof(buf) - i, " %z", ltm);
	fprintf(DF, "%s, ", buf);

	fprintf(DF, "%s, %s, ",
		get_packet_type_name(p->wlan_type), ether_sprintf(p->wlan_src));
	fprintf(DF, "%s, ", ether_sprintf(p->wlan_dst));
	fprintf(DF, "%s, ", ether_sprintf(p->wlan_bssid));
	fprintf(DF, "%x, %d, %d, %d, %d, ",
		p->pkt_types, p->phy_signal, p->wlan_len, p->phy_rate, p->phy_freq);
	fprintf(DF, "%016llx, ", (unsigned long long)p->wlan_tsf);
	fprintf(DF, "%s, %d, %d, %d, %d, %d, ",
		p->wlan_essid, p->wlan_mode, p->wlan_channel,
		p->wlan_wep, p->wlan_wpa, p->wlan_rsn);
	fprintf(DF, "%s, %s\n", ip_sprintf(p->ip_src), ip_sprintf(p->ip_dst));
	fflush(DF);
}
