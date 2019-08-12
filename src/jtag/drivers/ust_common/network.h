#ifndef UST_NETWORK_COMMON_H
#define UST_NETWORK_COMMON_H
#pragma once


#ifndef _WIN32
	#include <netinet/tcp.h>
#endif

inline void EnableTcpNoDelay(int socket)
{
	/* Debug messages tend to be small containing only 4 bytes of data, this does not fill a TCP
	   packet. Disable the "Nagle" algorithm so all data gets sent immediatly */
	char flag = 1;
	setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(int));
}

inline void EnableQuickAck(int socket)
{
#ifndef _WIN32
	/* This disables delayed acks, windows does not support this option, BUT does disable
	   delayed acks on loopback adapter. NOTE: Change is not permanent so needs resetting
	   after all TCP socket communications */
	char flag = 1;
	setsockopt(socket, IPPROTO_TCP, TCP_QUICKACK, &flag, sizeof(int));
#endif
}



#endif