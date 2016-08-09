#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ifaddrs.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#define _GNU_SOURCE
#include <net/if.h>

// From linux/if_link.h. No portable solution found at this time.
typedef unsigned int __u32;
struct rtnl_link_stats {
	__u32   rx_packets;             /* total packets received       */
	__u32   tx_packets;             /* total packets transmitted    */
	__u32   rx_bytes;               /* total bytes received         */
	__u32   tx_bytes;               /* total bytes transmitted      */
	__u32   rx_errors;              /* bad packets received         */
	__u32   tx_errors;              /* packet transmit problems     */
	__u32   rx_dropped;             /* no space in linux buffers    */
	__u32   tx_dropped;             /* no space available in linux  */
	__u32   multicast;              /* multicast packets received   */
	__u32   collisions;
	/* detailed rx_errors: */
	__u32   rx_length_errors;
	__u32   rx_over_errors;         /* receiver ring buff overflow  */
	__u32   rx_crc_errors;          /* recved pkt with crc error    */
	__u32   rx_frame_errors;        /* recv'd frame alignment error */
	__u32   rx_fifo_errors;         /* recv'r fifo overrun          */
	__u32   rx_missed_errors;       /* receiver missed packet       */
	/* detailed tx_errors */
	__u32   tx_aborted_errors;
	__u32   tx_carrier_errors;
	__u32   tx_fifo_errors;
	__u32   tx_heartbeat_errors;
	__u32   tx_window_errors;
	/* for cslip etc */
	__u32   rx_compressed;
	__u32   tx_compressed;
	__u32   rx_nohandler;           /* dropped, no handler found    */
};

char* ip_to_string(struct sockaddr *addr)
{
	static char buf[INET6_ADDRSTRLEN] = {0};
	switch(addr->sa_family) {
	case AF_INET:
		{ struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
		inet_ntop(AF_INET, &(addr_in->sin_addr), buf, sizeof(buf));
		break; }
	case AF_INET6:
		{ struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
		inet_ntop(AF_INET6, &(addr_in6->sin6_addr), buf, sizeof(buf));
		break; }
	default:
		snprintf(buf, sizeof(buf), "addr_family=%d", addr->sa_family);
	}
	return buf;
}

void print_iff_flags(unsigned int flags)
{
	if (flags & IFF_UP) printf(" IFF_UP");
	if (flags & IFF_BROADCAST) printf(" IFF_BROADCAST");
	if (flags & IFF_LOOPBACK) printf(" IFF_LOOPBACK");
	if (flags & IFF_LOWER_UP) printf(" IFF_LOWER_UP");
}

int main() {
	struct ifaddrs *ifaddrs = NULL;
	int res = getifaddrs(&ifaddrs);
	if (res != 0) {
		fprintf(stderr, "Error: could not get interface list\n");
		perror("getifaddrs()");
		return res;
	}

	printf(" [*] Interface list:\n");
	for(struct ifaddrs *ifaddr = ifaddrs; ifaddr != NULL; ifaddr = ifaddr->ifa_next)
	{
		printf(" %s (%s", ifaddr->ifa_name,
			(ifaddr->ifa_addr == NULL ? "no addr" : ip_to_string(ifaddr->ifa_addr)));
		print_iff_flags(ifaddr->ifa_flags);
		printf(")\n");
	}

	int loopback_found = 0;
	struct ifaddrs *lo_ipv4 = NULL;
	struct ifaddrs *eth0_ipv4 = NULL, *eth0_packet = NULL;
	for(struct ifaddrs *ifaddr = ifaddrs; ifaddr != NULL; ifaddr = ifaddr->ifa_next)
	{
		if ((ifaddr->ifa_flags & IFF_LOOPBACK) != 0) {
			loopback_found = 1;
			if (ifaddr->ifa_addr->sa_family == AF_INET)
				lo_ipv4 = ifaddr;
		}
		else if ((ifaddr->ifa_flags & IFF_BROADCAST) != 0) {
			if (ifaddr->ifa_addr->sa_family == AF_INET)
				eth0_ipv4 = ifaddr;
			else if (ifaddr->ifa_addr->sa_family == AF_PACKET)
				eth0_packet = ifaddr;
		}
	}
	if (!loopback_found) {
		fprintf(stderr, "Error: no loopback interface\n");
		exit(1);
	} else if (lo_ipv4 == NULL) {
		fprintf(stderr, "Error: no address on loopback\n");
		exit(2);
	}
	if (eth0_packet == NULL) {
		fprintf(stderr, "Error: no eth0 interface\n");
		exit(3);
	} else if(eth0_ipv4 == NULL) {
		fprintf(stderr, "Error: no address on eth0\n");
		exit(4);
	}

	//struct rtnl_link_stats *stats = (struct rtnl_link_stats*)eth0_packet->ifa_data;
	//printf("RX=%lld TX=%lld\n", (long long)stats->rx_bytes, (long long)stats->tx_bytes);

	freeifaddrs(ifaddrs);

	while(getchar() != '\n') { }
	return 0;
}
