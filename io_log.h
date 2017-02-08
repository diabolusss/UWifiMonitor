#ifndef __IO_LOG_H__
#define __IO_LOG_H__
/*
#ifdef LOG_LEVEL
	#undef LOG_LEVEL
	#define LOG_LEVEL LOG_INFO
	#warning "LOG_LEVEL was decreased to LOG_INFO from IO_LOG_H"
#else
	#define LOG_LEVEL LOG_INFO
	#warning "LOG_LEVEL was set to LOG_INFO from IO_LOG_H"
#endif*/	
	#include "wlan_parser.h"//struct uwifi_packet

	void __attribute__ ((format (printf, 2, 3))) printlog_(const char *appender, const char *fmt, ...);
	void __attribute__ ((format (printf, 2, 3))) printlog(int level, const char *fmt, ...);
	void dumpfile_open(const char* name);
	void dumpfile_write(struct uwifi_packet* p);
	void dumpfile_close();
#endif
