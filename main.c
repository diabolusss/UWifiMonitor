#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <net/if.h>//if_nametoindex

#include <signal.h>//interrupt
#include <time.h>//In C, time.h declares time(), clock(), timespec_get(), difftime(), asctime(), ctime(), strftime() wcsftime(), gmtime(), localtime(), mktime() and related structs and macros: http://en.cppreference.com/w/c/chrono

//uwifi includes
#include "radiotap.h"
#include "radiotap_iter.h"
#include "ifctrl.h"
#include "conf.h"
#include "util.h"
#include "netdev.h"
#include "capture.h"//open_packet_socket, device_get_hwinfo
#include "channel.h"//channel_get_remaining_dwell_time

//application includes
#include "main.h"
#include "io_log.h"//LOG_{constants}, print{debug}
#include "utils_custom.h"//generate mon ifname
#include "wlan_parser.h"//struct uwifi_packet
#include "raw_parser.h" //uwifi_parse_raw
#include "wlan_util.h" //get_packet_type_name
#include "ieee80211_util.h" //ieee80211_frame_duration
//#include "node.h"//uwifi_node_update
#include "network.h"//socket connection to remote server
#include "utils_time.h"
#include "conf_options.h" // parse cmdline arguments and configuration file settings

/**
	//BEGIN////////////////////////
	// INTERRUPTS
	//////////////////////////
**/
	// Handlers	
	static void exit_handler(void);
	static void sigint_handler(__attribute__((unused)) int sig);
	static void sigpipe_handler(__attribute__((unused)) int sig);

	//Variables
	static volatile sig_atomic_t is_sigint_caught;
/**
	//////////////////////////
	// INTERRUPTS
	//END/////////////////////
**/

/**
	//BEGIN////////////////////////////////////
	// RECEIVE HANDLERS
	//////////////////////////////////////
**/
	//Handlers
	static void handle_fd(const sigset_t *const waitmask);
	static void receive_packet_mon(int fd, unsigned char* buffer, size_t bufsize);
	static void handle_packet(struct uwifi_packet* p);
	static void send_packet_to_db(struct uwifi_packet* p);

	//Variables
	/* receive packet buffer
	 *
	 * due to the way we receive packets the network (TCP connection) we have to
	 * expect the reception of partial packet as well as the reception of several
	 * packets at one. thus we implement a buffered receive where partially received
	 * data stays in the buffer.
	 *
	 * we need two buffers: one for packet capture or receiving from the server and
	 * another one for data the clients sends to the server.
	 *
	 * not sure if this is also an issue with local packet capture, but it is not
	 * implemented there.
	 *
	 * size: max 80211 frame (2312) + space for prism2 header (144)
	 * or radiotap header (usually only 26) + some extra */
	static unsigned char buffer[2312 + 200];
	static size_t buflen;

	/* for select */
	static fd_set read_fds;
	static fd_set write_fds;
	static fd_set excpt_fds;
/**
	//////////////////////////////////////
	// RECEIVE HANDLERS
	//END////////////////////////////////////
**/

struct uwifi_interface* intf;
int mon;

struct config conf;
struct timespec the_time;

int main (int argc, char *argv[]){
	/**
		//BEGIN////////////////////////////
		// SETUP INTERRUPT HANDLERS
		///////////////////////////////////
	**/
	sigset_t workmask;
	sigset_t waitmask;
	struct sigaction sigint_action;
	struct sigaction sigpipe_action;
	
	sigint_action.sa_handler = sigint_handler;
	sigemptyset(&sigint_action.sa_mask);
	sigint_action.sa_flags = 0;
	sigaction(SIGINT, &sigint_action, NULL);
	sigaction(SIGTERM, &sigint_action, NULL);
	sigaction(SIGHUP, &sigint_action, NULL);

	sigpipe_action.sa_handler = sigpipe_handler;
	sigaction(SIGPIPE, &sigpipe_action, NULL);

	atexit(exit_handler);		

	/**
		//BEGIN////////////////////////////
		// SETUP INTERFACE
		///////////////////////////////////
	**/

	config_parse_file_and_cmdline(argc, argv);
	
	//set initial interface name
	//strncpy(
	//	conf.ifname, 
		//"wlan0"
		//"horst1"
		//"mon0"
		//, IF_NAMESIZE
	//);

	intf = malloc(sizeof(struct uwifi_interface));
	strncpy(intf->ifname, conf.ifname, IF_NAMESIZE);
	
	clock_gettime(CLOCK_MONOTONIC, &the_time);		

	printdbg("Config:\n");
	printdbg_("\tintf->ifname: %s;\n",intf->ifname);

	printinf("Initiating netlink device...");//[independent of interface]
	if(!ifctrl_init()){
		printcri("[FAILED]\n");
		return -1;
	}else printinf_("[DONE]\n");

	printinf("Checking provided wlan interface...");	
	if(!if_nametoindex(intf->ifname)){
		printinf_("[FAILED]\n");
		if(errno == ENODEV)
			printcri("No such interface[%s] found. ERRNO#%d\n",intf->ifname, errno);
		return -1;
	}else printinf_("[DONE]\n");
		
	printinf("Initiating uwifi_interface struct...");
	if(uwifi_init(intf)){
		printinf_("[DONE]\n");
	}else printinf_("[FAILED]\n");

	printinf("Getting interface information...");
	if(ifctrl_iwget_interface_info(intf)){
		printinf_("[DONE]\n");
	}else printinf_("[FAILED]\n");		

	printinf_("\tInterface %s %s monitor\n", intf->ifname, ((ifctrl_is_monitor(intf))?("is"):("isn't")) );

	printinf("Trying to put interface into monitor state...");
	if(ifctrl_iwset_monitor(intf->ifname)){
		printinf_("[DONE]\n");
	}else printinf_("[FAILED]\n");
	
	//if is not monitor and couldnt be set into monitor state create new interface
	if ((!ifctrl_is_monitor(intf) && !ifctrl_iwset_monitor(intf->ifname))) {
		char mon_ifname[IF_NAMESIZE];
		generate_mon_ifname(mon_ifname, IF_NAMESIZE);
		printinf("Trying to add new virtual monitor interface %s...", mon_ifname);		
		if( !ifctrl_iwadd_monitor(intf->ifname, mon_ifname) ){
			printcri("[FAILED]\n");
			//printcri( "Failed to add virtual monitor interface\n");
			return -1;
		}else printinf_("[DONE]\n");

		strncpy(intf->ifname, mon_ifname, IF_NAMESIZE);
		strncpy(conf.ifname, mon_ifname, IF_NAMESIZE);
		/* Now we have a new monitor interface, proceed
		 * normally. The interface will be deleted at exit[check exit handler]. */
	}

	printinf("Trying to bring interface %s up...", intf->ifname);		
	if( !ifctrl_flags(intf->ifname, true, true) ){
		printcri("[FAILED]\n");
		//printcri( "Failed to bring interface '%s' up", intf->ifname);
		return -1;
	}else printinf_("[DONE]\n");

	/* get info again, as chan width is only available on UP interfaces */
	printinf("Updating interface information...");
	if(ifctrl_iwget_interface_info(intf)){
		printinf_("[DONE]\n");
	}else printinf_("[FAILED]\n");

	printinf("Opening packet socket...");
	if( (mon = open_packet_socket(intf->ifname, 0)) <= 0 ){
		printcri("[FAILED]\n");
		return -1;
	}else printinf_("[DONE]\n");
	
	printinf("Getting hardware info...");
	if( (conf.arphdr = device_get_hwinfo(mon, intf->ifname,(unsigned char *) &conf.my_mac_addr)) > 0 ){
		printinf_("[DONE]\n");
		printdbg_("\tARPTYPE %d[%s], MAC %s\n", conf.arphdr, ((conf.arphdr == ARPHDR_IEEE80211_PRISM)?("arphdr_IEEE80211_PRISM"):(((conf.arphdr == ARPHDR_IEEE80211_RADIOTAP)?("arphdr_IEEE80211_RADIOTAP"):("UNKNOWN")))), ether_sprintf(conf.my_mac_addr));
		if (
			conf.arphdr != ARPHDR_IEEE80211_PRISM &&
			conf.arphdr != ARPHDR_IEEE80211_RADIOTAP
		){
			printcri( "Interface '%s' is not in monitor mode\n",   conf.ifname);
			return -1;
		}
		intf->arphdr = conf.arphdr;
		
	}else{
		printcri("[FAILED]\n");
		return -1;
	}	

	printinf("Trying to change the initial channel number...");
	if( !uwifi_channel_init(intf) ){
		printinf_("[FAILED]\n");
	}else printinf_("[DONE]\n");
	uwifi_channel_printall(intf);//print received channels
	printinf_("Max PHY rate: %d Mbps\n", intf->max_phy_rate/10);
	
	/**
		//////////////////////////////
		// SETUP
		//END/////////////////////////
	**/
	while (!conf.do_change_channel || conf.channel_scan_rounds != 0){
		handle_fd(&waitmask);

		if (is_sigint_caught) exit(1);

		clock_gettime(CLOCK_MONOTONIC, &the_time);
	}

	return 0;
}

/**
	//////////////////////////////////////
	// RECEIVE HANDLERS
	//////////////////////////////////////
**/

/**
	Check for input from multiple file descriptors
**/

struct timespec tstart={0,0};
//get 1 packet in 1\100 sec for specific mac
struct timespec ttimeout={0,1000000};

struct timespec truntime={0,0};
static void handle_fd(const sigset_t *const waitmask){
	int ret, mfd;
	uint32_t usecs = UINT32_MAX;
	struct timespec ts;

	FD_ZERO(&read_fds);
	FD_ZERO(&write_fds);
	FD_ZERO(&excpt_fds);

	//if (!conf.quiet && !conf.debug)
	//	FD_SET(0, &read_fds);

	FD_SET(mon, &read_fds);

	//if (srv_fd != -1)
	//	FD_SET(srv_fd, &read_fds);
	//if (cli_fd != -1)
	//	FD_SET(cli_fd, &read_fds);
	//if (ctlpipe != -1)
	//	FD_SET(ctlpipe, &read_fds);

	usecs = min(uwifi_channel_get_remaining_dwell_time(intf), 1000000);
	ts.tv_sec = usecs / 1000000;
	ts.tv_nsec = usecs % 1000000 * 1000;
	mfd = max(mon, 1)+1;
	//mfd = max(mfd, ctlpipe);
	//mfd = max(mfd, cli_fd) + 1;

	ret = pselect(mfd, &read_fds, &write_fds, &excpt_fds, &ts, waitmask);
	if ( (ret == 0) || (ret == -1 && errno == EINTR) ) /* interrupted */
		return;
	//if (ret == 0) {
	//	return;
	//}
	//else if (ret < 0) /* error */
	//	printerr( "select()");
	static int count=0;

	/* local monitor packet received */
	if (FD_ISSET(mon, &read_fds)) {
		if(count == 0)
			clock_gettime(CLOCK_MONOTONIC, &tstart);

		receive_packet_mon(mon, buffer, sizeof(buffer));
		clock_gettime(CLOCK_MONOTONIC, &the_time);

		timespec_diff(&truntime, &tstart, &the_time);
		count++;
		if(timespec_gt(&truntime, &ttimeout) == 1){
			printinf("Handled %d packet(s) in %f second(s).\n", count, ((double)truntime.tv_sec + 1.0e-9*truntime.tv_nsec));
			count = 0;

		}

		//printinf("Handled packet in %f nsecond(s).\n", ((double)truntime.tv_sec + 1.0e-9*truntime.tv_nsec));
		//printinf("Handled %d packet(s) in %f second(s).\n", count, ((double)truntime.tv_sec + 1.0e-9*truntime.tv_nsec));
		
	}
}

/**
	Extract received packet from file descriptor
**/
static void receive_packet_mon(int fd, unsigned char* buffer, size_t bufsize){
	int len;
	struct uwifi_packet p;

	len = recv_packet(fd, buffer, bufsize);

	if (conf.debug) {
			printdbg_( "===============================================================================\n");
			printdbg_( "receive_packet_mon: dump_packet START\n");
			printdbg_( "receive_packet_mon: dump_packet(buffer, len);\n");
			printdbg_( "receive_packet_mon: dump_packet END\n");
			printdbg_( "===============================================================================\n");
	}

	memset(&p, 0, sizeof(p));

	int res = uwifi_parse_raw(buffer, len, &p, intf->arphdr);
	if (res < 0) {
		printdbg("parsing failed\n");
		return;

	}else if(res == 0){
		printdbg("Bad FCS, allow packet but stop parsing\n");
		
	}
	//wlan_util

	//printinf("<main> receive_packet_mon: %s\n", get_packet_type_name(p.wlan_type));
	
	handle_packet(&p);

	//exit(1);
}

static void handle_packet(struct uwifi_packet* p){
	//struct node_info* n = NULL;

	/* filter on server side only */
	uwifi_fixup_packet_channel(p, intf);	
	
	/* we can't trust any fields except phy_* of packets with bad FCS */
	if ((p->phy_flags & PHY_FLAG_BADFCS)) return;
	if(conf.debug){
		printinf("<main> handle_packet: %s", get_packet_type_name(p->wlan_type));
		printinf_("{src:%s, ", ether_sprintf(p->wlan_src));
		printinf_(" dst:%s, ", ether_sprintf(p->wlan_dst));
		printinf_(" bssid:%s,", ether_sprintf(p->wlan_bssid));
		printinf_(" my_mac:%s}\n", ether_sprintf(conf.my_mac_addr));
	}
	//n = uwifi_node_update(p);

	//if (n)
	//	p->wlan_retries = n->wlan_retries_last;

	p->pkt_duration = ieee80211_frame_duration(
				p->phy_flags & PHY_FLAG_MODE_MASK,
				p->wlan_len, p->phy_rate,
				p->phy_flags & PHY_FLAG_SHORTPRE,
				0 /*shortslot*/, p->wlan_type,
				p->wlan_qos_class,
				p->wlan_retries);

	send_packet_to_db(p);
}

static void send_packet_to_db(struct uwifi_packet* p){
	char port[5];
	sprintf(port, "%d", conf.port);

	network_open_client_socket(
				conf.serveraddr, 
				port
	);

	if(network_is_client_open() <= 0) return;
	
	if(network_send_packet(p) > 0){	
		dumpfile_write	(p);
		//printpac(p);		
		printdbg(" packet sent\n");
	}
	dumpfile_write	(p);
	//network_close_client();	
}

/**
	//////////////////////////////////////
	// APPLICATION INTERRUPT HANDLERS
	//////////////////////////////////////
**/

static void exit_handler(void){
	if(mon>0) close_packet_socket(mon);

	ifctrl_flags(intf->ifname, false, false);
	ifctrl_iwdel(intf->ifname);
	ifctrl_finish();

	dumpfile_close();
	
	printinf("<main.c> exit_handler(): Bye-bye!\n");
}

static void sigint_handler(__attribute__((unused)) int sig){
	/* Only set an atomic flag here to keep processing in the interrupt
	 * context as minimal as possible (at least all unsafe functions are
	 * prohibited, see signal(7)). The flag is handled in the mainloop. */
	is_sigint_caught = 1;
}

static void sigpipe_handler(__attribute__((unused)) int sig){
	/* ignore signal here - we will handle it after write failed */
}
