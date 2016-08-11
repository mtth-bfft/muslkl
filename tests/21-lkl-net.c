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

void print_iff_list()
{
	struct ifaddrs *ifaddrs = NULL;
	int res = getifaddrs(&ifaddrs);
	if (res != 0) {
		fprintf(stderr, "Error: could not get interface list\n");
		perror("getifaddrs()");
		exit(1);
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
		exit(2);
	} else if (lo_ipv4 == NULL) {
		fprintf(stderr, "Error: no address on loopback\n");
		exit(3);
	}
	if (eth0_packet == NULL) {
		fprintf(stderr, "Error: no eth0 interface\n");
		exit(4);
	} else if(eth0_ipv4 == NULL) {
		fprintf(stderr, "Error: no address on eth0\n");
		exit(5);
	}
	freeifaddrs(ifaddrs);
}

int main() {
	print_iff_list();
	sleep(2);
	return 0;
}

