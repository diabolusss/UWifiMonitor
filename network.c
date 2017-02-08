/* horst - Highly Optimized Radio Scanning Tool
 *
 * Copyright (C) 2005-2016 Bruno Randolf (br1@einfach.org)
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
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include <err.h>

#include "network.h"
#include "main.h"//defines
#include "wlan_parser.h" //struct uwifi_packet
#include "io_log.h"//for print{dbg}

#define PROTO_VERSION	3

/**
	//BEGIN////////////////////////////////////
	// LOCAL NETWORK TYPES
	///////////////////////////////////////////
**/
	enum pkt_type {
		PROTO_PKT_INFO		= 0,
		PROTO_CHAN_LIST		= 1,
		PROTO_CONF_CHAN		= 2,
		PROTO_CONF_FILTER	= 3,
	};

	struct net_header {
		unsigned char version;
		unsigned char type;
	} __attribute__ ((packed));

	struct net_conf_chan {
		struct net_header	proto;

		unsigned char do_change;
		unsigned char upper;
		char channel;

	#define NET_WIDTH_HT40PLUS	0x80
		unsigned char width_ht40p;	// use upper bit for HT40+-

		int dwell_time;

	} __attribute__ ((packed));

	struct net_band {
		unsigned char num_chans;
		unsigned char max_width;
		unsigned char streams_rx;
		unsigned char streams_tx;
	} __attribute__ ((packed));

	struct net_chan_list {
		struct net_header	proto;

		unsigned char num_bands;
		struct net_band band[2];	// always send both
		unsigned int freq[1];
	} __attribute__ ((packed));

	#define PKT_INFO_VERSION	2

	struct net_packet_info {
		struct net_header	proto;
		unsigned char		version;		
		unsigned char		wlan_moni[MAC_LEN];

		/* general */
		unsigned int		pkt_types;	/* bitmask of packet types */

		/* wlan phy (from radiotap) */
		int			phy_signal;	/* signal strength (usually dBm) */
		unsigned int		phy_rate;	/* physical rate * 10 (= in 100kbps) */
		unsigned char		phy_rate_idx;
		unsigned char		phy_rate_flags;
		unsigned int		phy_freq;	/* frequency */
		unsigned int		phy_flags;	/* A, B, G, shortpre */

		/* wlan mac */
		unsigned int		wlan_len;	/* packet length */
		unsigned int		wlan_type;	/* frame control field */
		unsigned char		wlan_src[MAC_LEN];
		unsigned char		wlan_dst[MAC_LEN];
		unsigned char		wlan_bssid[MAC_LEN];
		char			wlan_essid[WLAN_MAX_SSID_LEN];
		uint64_t		wlan_tsf;	/* timestamp from beacon */
		unsigned int		wlan_bintval;	/* beacon interval */
		unsigned int		wlan_mode;	/* AP, STA or IBSS */
		unsigned char		wlan_channel;	/* channel from beacon, probe */
		unsigned char		wlan_chan_width;
		unsigned char		wlan_tx_streams;
		unsigned char		wlan_rx_streams;
		unsigned char		wlan_qos_class;	/* for QDATA frames */
		unsigned int		wlan_nav;	/* frame NAV duration */
		unsigned int		wlan_seqno;	/* sequence number */

	#define PKT_WLAN_FLAG_WEP	0x01
	#define PKT_WLAN_FLAG_RETRY	0x02
	#define PKT_WLAN_FLAG_WPA	0x04
	#define PKT_WLAN_FLAG_RSN	0x08
	#define PKT_WLAN_FLAG_HT40PLUS	0x10

		/* bitfields are not portable - endianness is not guaranteed */
		unsigned int		wlan_flags;

		/* IP */
		unsigned int		ip_src;
		unsigned int		ip_dst;
		unsigned int		tcpudp_port;
		unsigned int		olsr_type;
		unsigned int		olsr_neigh;
		unsigned int		olsr_tc;

	#define PKT_BAT_FLAG_GW		0x1
		unsigned char		bat_flags;
		unsigned char		bat_pkt_type;
	} __attribute__ ((packed));
/**
	//////////////////////////////////////////////
	// LOCAL NETWORK TYPES
	//END/////////////////////////////////////////
**/

/**
	//BEGIN////////////////////////////////////
	// FUNCTION PROTOS
	///////////////////////////////////////////
**/
	struct net_packet_info packet_to_net(struct uwifi_packet *p);

	//local
	static int net_write(int *fd, unsigned char* buf, size_t len);
	static int net_send_packet(int* sock, struct uwifi_packet *p);

	static int net_open_client_socket(char* serveraddr, char* rport);
	static int net_close_socket(int *fd);

	static int net_is_socket_open(int fd);
/**
	//////////////////////////////////////////////
	// FUNCTION PROTOS
	//END/////////////////////////////////////////
**/

/**
	//BEGIN////////////////////////////////////
	// VARIABLES
	///////////////////////////////////////////
**/
	int cli_fd;
/**
	//////////////////////////////////////////////
	// FUNCTION PROTOS
	//END/////////////////////////////////////////
**/
int network_send_packet(struct uwifi_packet *p){
	int status = net_send_packet(&cli_fd, p);

	if(status < 0){
		printwrn("<network> network_send_packet(): send failed to socketfd %d. {ERRNO#%d, ERR#%s}\n", cli_fd, errno, strerror(errno));
		net_close_socket(&cli_fd);
	}

	return status;
}

int network_open_client_socket(char* serveraddr, char* rport){
	if(cli_fd > 0){
		//printwrn("<network> network_open_client_socket(): Client socket already open - fd:%d!", cli_fd);
		return -1;		
	}

	cli_fd = net_open_client_socket(serveraddr, rport);
	if(cli_fd > 0) return 1;

	return -1;
}

int network_is_client_open(void){
	return net_is_socket_open(cli_fd);
}

void network_close_client(void){
	printinf("<network> network_close_client(): Closing client file descriptor %d...", cli_fd);	
	if(net_close_socket(&cli_fd) > 0)
		printinf_("[DONE]\n");	
	else
		printinf_("[FAIL]\n");	
}

void network_finish(void){
	network_close_client();
}

struct net_packet_info packet_to_net(struct uwifi_packet *p){
	struct net_packet_info np;

	memcpy(np.wlan_moni, conf.my_mac_addr, MAC_LEN);

	np.proto.version = PROTO_VERSION;
	np.proto.type	= PROTO_PKT_INFO;

	np.version	= PKT_INFO_VERSION;
	np.pkt_types	= htole32(p->pkt_types);

	np.phy_signal	= htole32(p->phy_signal);
	//np.phy_noise	= htole32(0);
	//np.phy_snr	= htole32(0);
	np.phy_rate	= htole32(p->phy_rate);
	np.phy_rate_idx	= p->phy_rate_idx;
	np.phy_rate_flags = p->phy_rate_flags;
	np.phy_freq	= htole32(p->phy_freq);
	np.phy_flags	= htole32(p->phy_flags);

	np.wlan_len	= htole32(p->wlan_len);
	np.wlan_type	= htole32(p->wlan_type);
	memcpy(np.wlan_src, p->wlan_src, MAC_LEN);
	memcpy(np.wlan_dst, p->wlan_dst, MAC_LEN);
	memcpy(np.wlan_bssid, p->wlan_bssid, MAC_LEN);
	memcpy(np.wlan_essid, p->wlan_essid, WLAN_MAX_SSID_LEN);
	np.wlan_tsf	= htole64(p->wlan_tsf);
	np.wlan_bintval	= htole32(p->wlan_bintval);
	np.wlan_mode	= htole32(p->wlan_mode);
	np.wlan_channel = p->wlan_channel;
	np.wlan_chan_width = p->wlan_chan_width;
	np.wlan_tx_streams = p->wlan_tx_streams;
	np.wlan_rx_streams = p->wlan_rx_streams;
	np.wlan_qos_class = p->wlan_qos_class;
	np.wlan_nav	= htole32(p->wlan_nav);
	np.wlan_seqno	= htole32(p->wlan_seqno);
	np.wlan_flags = 0;
	if (p->wlan_wep)
		np.wlan_flags |= PKT_WLAN_FLAG_WEP;
	if (p->wlan_retry)
		np.wlan_flags |= PKT_WLAN_FLAG_RETRY;
	if (p->wlan_wpa)
		np.wlan_flags |= PKT_WLAN_FLAG_WPA;
	if (p->wlan_rsn)
		np.wlan_flags |= PKT_WLAN_FLAG_RSN;
	if (p->wlan_ht40plus)
		np.wlan_flags |= PKT_WLAN_FLAG_HT40PLUS;
	np.wlan_flags	= htole32(np.wlan_flags);
	np.ip_src	= p->ip_src;
	np.ip_dst	= p->ip_dst;
	np.tcpudp_port	= htole32(p->tcpudp_port);
	np.olsr_type	= htole32(p->olsr_type);
	np.olsr_neigh	= htole32(p->olsr_neigh);
	np.olsr_tc	= htole32(p->olsr_tc);
	np.bat_flags = 0;
	if (p->bat_gw)
		np.bat_flags |= PKT_BAT_FLAG_GW;
	np.bat_pkt_type = p->bat_packet_type;

	return np;
}

/**
	/////////////////////////////////////////
	// LOCAL FUNCTIONS
	/////////////////////////////////////////
**/

static int net_open_client_socket(char* serveraddr, char* rport){
	struct addrinfo saddr;
	struct addrinfo *result, *rp;
	//char rport_str[20];
	int ret;

	//snprintf(rport_str, 20, "%d", rport);

	printdbg("<network> net_open_client_socket(): Connecting to server %s port %s", serveraddr, rport);

	/* Obtain address(es) matching host/port */
	memset(&saddr, 0, sizeof(struct addrinfo));
	saddr.ai_family = AF_INET;
	saddr.ai_socktype = SOCK_STREAM;
	saddr.ai_flags = 0;
	saddr.ai_protocol = 0;

	ret = getaddrinfo(serveraddr, rport, &saddr, &result);
	if (ret != 0) {
		printerr("<network> net_open_client_socket(): Couldn't resolve - %s\n", gai_strerror(ret));
		//exit(EXIT_FAILURE);
		return -1;
	}
	printdbg_("\n");
	/* getaddrinfo() returns a list of address structures.
	 * Try each address until we successfully connect. */
	int netmon_fd;
	for (rp = result; rp != NULL; rp = rp->ai_next) {
		netmon_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (netmon_fd == -1)
			continue;

		if (connect(netmon_fd, rp->ai_addr, rp->ai_addrlen) != -1)
			break; /* Success */

		close(netmon_fd);
	}

	if (rp == NULL) {
		/* No address succeeded */
		freeaddrinfo(result);
		//err(1, "Could not connect");
		return -1;
	}

	freeaddrinfo(result);

	return netmon_fd;
}

static int net_write(int *fd, unsigned char* buf, size_t len){
	int ret;
	ret = write(*fd, buf, len);
	// On success, the number of bytes written is returned (zero indicates
    //   nothing was written).  It is not an error if this number is smaller
    //   than the number of bytes requested; this may happen for example
    //   because the disk device was filled.  See also NOTES.
    // On error, -1 is returned, and errno is set appropriately.
	return ret;	
}

static int net_send_packet(int* sock, struct uwifi_packet *p){
	//if(p->phy_signal == 0) return -1;
	struct net_packet_info np = packet_to_net(p);

	return net_write(sock, (unsigned char *)&np, sizeof(np));
}


static int getSO_ERROR(int fd) {
	int err = 1;
	socklen_t len = sizeof err;
	//if (
	// On success, zero is returned for the standard options.  On error, -1
    //   is returned, and errno is set appropriately.
	getsockopt(fd, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
	// == -1)
	//   printdbg_("<network> getSO_ERROR(): getsockopt\n");
	if (err)
	  errno = err;              // set errno to the socket SO_ERROR
	return err;
}

static int net_close_socket(int *fd) {      // *not* the Windows closesocket()
	if(*fd < 0) return 1;

	getSO_ERROR(*fd); // first clear any errors, which can cause close to fail
	
	int status = shutdown(*fd, SHUT_RDWR);// secondly, terminate the 'reliable' delivery

	#if(LOG_LEVEL >= LOG_DEBUG)
	if (status < 0){ 
		//EBADF 
		//	The socket argument is not a valid file descriptor. 
		//EINVAL 
		//	The how argument is invalid. 
		//ENOTCONN 
		//	The socket is not connected. 
		//ENOTSOCK 
		//	The socket argument does not refer to a socket.
		//if (errno != ENOTCONN && errno != EINVAL) // SGI causes EINVAL
		printdbg_("<network> net_close_socket(): error on shutdown. {ERRNO#%d, ERR#%s}", errno, strerror(errno));
	}
	#endif

	getSO_ERROR(*fd); // first clear any errors, which can cause close to fail

	status = close(*fd); // finally call close()
	
	if(status == 0){
		status = 1;
		*fd = -1;

	}
	#if(LOG_LEVEL > LOG_DEBUG)
	else
		printdbg_("<network> net_close_socket(): close failed. {ERRNO#%d. ERR#%s}\n", errno, strerror(errno));	
	#endif
   
	return status;
}

static int net_is_socket_open(int fd){
	if(fd > 0) return 1;
	else return -1;
}
