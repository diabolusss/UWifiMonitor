/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2014-2016 Bruno Randolf (br1@einfach.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <err.h>

#include "main.h"
#include "io_log.h"
#include "util.h"
#include "wlan_util.h"
#include "conf_options.h"

struct conf_option {
	int		option;
	const char*	name;
	int		value_required;
	const char*	default_value;
	bool		(*func)(const char* value);
};

#if DEBUG
static bool conf_debug(__attribute__((unused)) const char* value) {
	conf.debug = 1;
	return true;
}
#endif

static bool conf_interface(const char* value) {
	strncpy(conf.ifname, value, IF_NAMESIZE);
	conf.ifname[IF_NAMESIZE] = '\0';
	return true;
}

static bool conf_outfile(const char* value) {
	dumpfile_open(value);
	return true;
}

static bool conf_client(const char* value) {
	strncpy(conf.serveraddr, value, MAX_CONF_VALUE_STRLEN);
	conf.serveraddr[MAX_CONF_VALUE_STRLEN] = '\0';
	return true;
}

static bool conf_port(const char* value) {
	conf.port = atoi(value);
	return true;
}

static struct conf_option conf_options[] = {
	/* C , NAME        VALUE REQUIRED, DEFAULT	CALLBACK */
#if DEBUG
	{ 'D', "debug", 		0, NULL,	conf_debug },		// NOT dynamic
#endif
	{ 'i', "interface", 		1, "wlan0",	conf_interface },	// NOT dynamic
	{ 'o', "outfile", 		1, NULL,	conf_outfile },
	{ 'n', "client",		1, NULL,	conf_client },		// NOT dynamic
	{ 'p', "port",			1, "4444",	conf_port },		// NOT dynamic
};

/*
 * More possible config options:
 *
 * main view:
 *	sort nodes by: signal, time, bssid, channel
 * spec view:
 *	show nodes or bars
 */


/*
 * This handles command line options from getopt as well as options from the config file
 * In the first case 'c' is non-zero and name is NULL
 * In the second case 'c' is 0 and name is set
 * Value may be null in all cases
 */
bool config_handle_option(int c, const char* name, const char* value)
{
	unsigned int i;
	char* end;

	for (i=0; i < sizeof(conf_options)/sizeof(struct conf_option); i++) {
		if (((c != 0 && conf_options[i].option == c) ||
		    (name != NULL && strcmp(conf_options[i].name, name) == 0)) &&
		     conf_options[i].func != NULL) {
			if (!conf.quiet) {
				if (value != NULL)
					printlog("Set '%s' = '%s'", conf_options[i].name, value);
				else
					printlog("Set '%s'", conf_options[i].name);
			}
			if (value != NULL) {
				/* split list values and call function multiple times */
				while ((end = strchr(value, ',')) != NULL) {
					*end = '\0';
					conf_options[i].func(value);
					value = end + 1;
				}
			}
			/* call function */
			return conf_options[i].func(value);
		}
	}
	if (name != NULL)
		printlog("Ignoring unknown config option '%s' = '%s'", name, value);
	return false;
}

static void config_read_file(const char* filename)
{
	FILE* fp ;
	char line[255];
	char name[MAX_CONF_NAME_STRLEN + 1];
	char value[MAX_CONF_VALUE_STRLEN + 1];
	int n;
	int linenum = 0;

	if ((fp = fopen(filename, "r")) == NULL) {
		printerr("Could not open config file '%s'\n", filename);
		return;
	}

	printinf("Reading %s config file.\n", filename);

	while (fgets(line, sizeof(line), fp) != NULL) {
		++linenum;

		if (line[0] == '#' ) // comment
			continue;

		// Note: 200 below has to match MAX_CONF_VALUE_STRLEN
		// Note: 32 below has to match MAX_CONF_NAME_STRLEN
		n = sscanf(line, " %32[^= \n] = %200[^ \n]", name, value);
		if (n < 0) { // empty line
			continue;
		} else if (n == 0) {
			printwrn("Config file has garbage on line %d, "
				 "ignoring the line.", linenum);
			continue;
		} else if (n == 1) { // no value
			printdbg("config_handle_option{%s}\n", name);
			config_handle_option(0, name, NULL);
		} else {
			printdbg("config_handle_option{%s,%s}\n", name, value);
			config_handle_option(0, name, value);			
		}
	}

	fclose(fp);
}

static void config_apply_defaults(void)
{
	unsigned int i;
	for (i=0; i < sizeof(conf_options)/sizeof(struct conf_option); i++) {
		if (conf_options[i].default_value != NULL) {
			conf_options[i].func(conf_options[i].default_value);
		}
	}
}

static char* config_get_getopt_string(char* buf, size_t maxlen, const char* add)
{
	unsigned int pos = 0;
	unsigned int i;
	maxlen = maxlen - 1; // we use it as string index

	for (i=0; i < sizeof(conf_options)/sizeof(struct conf_option) && pos < maxlen; i++) {
		if (conf_options[i].option != 0 && pos < maxlen) {
			buf[pos++] = conf_options[i].option;
			if (conf_options[i].value_required && pos < maxlen) {
				buf[pos++] = ':';
			}
			if (conf_options[i].value_required == 2 && pos < maxlen) {
				buf[pos++] = ':';
			}
		}
	}
	buf[pos] = '\0';

	if (add != NULL) {
		if (pos < maxlen && (maxlen - pos) >= strlen(add))
			strncat(buf, add, (maxlen - pos));
		else {
			printinf("Not enough space for getopt string!");
			exit(1);
		}
	}

	return buf;
}

static void print_usage(const char* name)
{
	printf("\nUsage: %s [-v] [-h] [-q] [-D] [-a] [-c file] [-i interface] [-t sec] [-d ms] [-V view] [-b bytes]\n"
		"\t\t[-s] [-u] [-N] [-n IP] [-p port] [-o file] [-X[name]] [-x command]\n"
		"\t\t[][-e MAC] [-f PKT_NAME] [-m MODE] [-B BSSID]\n\n"

		"General Options: Description (default value)\n"
		"  -v\t\tshow version\n"
		"  -h\t\tHelp\n"
		"  -q\t\tQuiet, no output\n"
#if DO_DEBUG
		"  -D\t\tShow lots of debug output, no UI\n"
#endif
		"  -a\t\tAlways add virtual monitor interface\n"
		"  -c <file>\tConfig file (" CONFIG_FILE ")\n"
		"  -C <chan>\tSet initial channel\n"
		"  -i <intf>\tInterface name (wlan0)\n"
		"  -t <sec>\tNode timeout in seconds (60)\n"
		"  -d <ms>\tDisplay update interval in ms (100)\n"
		"  -V view\tDisplay view: history|essid|statistics|spectrum\n"
		"  -b <bytes>\tReceive buffer size in bytes (not set)\n"
		"  -M[filename]\tMAC address to host name mapping (/tmp/dhcp.leases)\n"

		"\nFeature Options:\n"
		"  -s\t\t(Poor mans) Spectrum analyzer mode\n"
		"  -u\t\tUpper channel limit\n\n"

		"  -N\t\tAllow network connection, server mode (off)\n"
		"  -n <IP>\tConnect to server with <IP>, client mode (off)\n"
		"  -p <port>\tPort number of server (4444)\n\n"

		"  -o <filename>\tWrite packet info into 'filename'\n\n"

		"  -X[filename]\tAllow control socket on 'filename' (/tmp/horst)\n"
		"  -x <command>\tSend control command\n"

		"\nFilter Options:\n"
		" Filters are generally 'positive' or 'inclusive' which means you define\n"
		" what you want to see, and everything else is getting filtered out.\n"
		" If a filter is not set it is inactive and nothing is filtered.\n"
		" Most filter options can be specified multiple times and will be combined\n"
		"  -e <MAC>\tSource MAC addresses (xx:xx:xx:xx:xx:xx), up to 9 times\n"
		"  -f <PKT_NAME>\tFilter packet types, multiple\n"
		"  -m <MODE>\tOperating mode: AP|STA|ADH|PRB|WDS|UNKNOWN, multiple\n"
		"  -B <MAC>\tBSSID (xx:xx:xx:xx:xx:xx), only one\n"
		"\n",
		name);
}

void config_parse_file_and_cmdline(int argc, char** argv)
{
	char getopt_str[(sizeof(conf_options)/sizeof(struct conf_option))*2 + 10];
	char* conf_filename = CONFIG_FILE;
	int c;

	config_get_getopt_string(getopt_str, sizeof(getopt_str), "hvc:x:");

	/* first: apply default values */
	config_apply_defaults();

	/*
	 * then: handle command line options which are not
	 * configuration options ("hc:")
	 */
	while ((c = getopt(argc, argv, getopt_str)) > 0) {
		switch (c) {
		case 'c':
			printlog("Using config file '%s'", optarg);
			conf_filename = optarg;
			break;
		case 'v':
			printf("Version %s (build date: %s %s)\n", VERSION, __DATE__, __TIME__);
			exit(0);
		case 'h':
		case '?':
			print_usage(argv[0]);
			exit(0);
		}
	}

	/* read config file */
	config_read_file(conf_filename);

	/*
	 * get command line options which are configuration, to let them
	 * override or add to the config file options
	 */
	optind = 1;
	while ((c = getopt(argc, argv, getopt_str)) > 0) {
		config_handle_option(c, NULL, optarg);
	}
}
