#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ust_jtagprobe.h"
#include "../ust_common/network.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#define close closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif



typedef struct ust_jtagprobe_t {
	int skt;
} ust_jtagprobe_t;

ust_jtagprobe_t * ust_jtagprobe_create() {
	ust_jtagprobe_t * s = calloc(1, sizeof(ust_jtagprobe_t));
	return s;
}
void ust_jtagprobe_destroy(ust_jtagprobe_t * s) {
	free(s);
}

int ust_jtagprobe_connect(ust_jtagprobe_t *s, const char *host, const char *port) {
	struct addrinfo hints = { 0 };
	struct addrinfo *addrs, *addr;
	char host_name[NI_MAXHOST];
	char serv_name[NI_MAXSERV];
	int rc;

	hints.ai_socktype = SOCK_STREAM;

	rc = getaddrinfo(host, port, &hints, &addrs);
	if (rc) {
		LOG_ERROR("getaddrinfo failed");
		return ERROR_FAIL;
	}
	for (addr = addrs; addr != NULL; addr = addr->ai_next) {
		rc = getnameinfo(addr->ai_addr, addr->ai_addrlen, host_name, sizeof host_name, serv_name, sizeof serv_name, NI_NUMERICSERV);
		if (rc) {
			LOG_ERROR("getnameinfo failed");
			return ERROR_FAIL;
		}
		s->skt = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (s->skt == -1) 
			continue;
		rc = connect(s->skt, addr->ai_addr, addr->ai_addrlen);
        EnableTcpNoDelay(s->skt);
        
		if (rc != -1) 
			break;
		close(s->skt);
	}
	if (!addr) {
		LOG_ERROR("Failed to connect to a jtagprobe port at %s:%s", host, port);
		return ERROR_FAIL;
	}
	return ERROR_OK;
}


int ust_jtagprobe_disconnect(ust_jtagprobe_t *s) {
	int err;
	err = close(s->skt);
	if (err) {
		LOG_ERROR("Failed to close jtagprobe socket");
		return ERROR_FAIL;
	} else {
		return ERROR_OK;
	}
}

unsigned long jtag_data_in[1024];
unsigned long jtag_data_out[1024];

int ust_jtagprobe_send_scan(ust_jtagprobe_t *s, int is_data, int no_response, int bit_length, uint8_t *bits) {
	uint8_t buffer[4096];
	int bytes_sent;
	int err;

	int bytelen = (bit_length + 7) / 8;
	int msglen = 4 + bytelen;
	int len = 0;

	buffer[len++] = 0;
	buffer[len++] = 0;
	buffer[len++] = (msglen >> 8) & 0xFF;
	buffer[len++] = msglen & 0xFF;
	buffer[len++] = is_data ? JTAGPROBE_SHIFT_DR : JTAGPROBE_SHIFT_IR;
	buffer[len++] = no_response ? JTAGPROBE_FLAG_NO_RESPONSE : 0;
	buffer[len++] = (bit_length >> 8) & 0xFF;
	buffer[len++] = bit_length & 0xFF;
	memcpy(buffer + len, bits, bytelen);
	len += bytelen;

	bytes_sent = send(s->skt, buffer, len, 0);
    EnableQuickAck(s->skt);
	
	if (bytes_sent != len) {
		LOG_ERROR("sent %d bytes expected to send %d\n", bytes_sent, len);
		return ERROR_FAIL;
	} else {
		return ERROR_OK;
	}
}

int	 ust_jtagprobe_send_cmd(ust_jtagprobe_t *s, int request, uint8_t num_args, uint32_t args[]) {
	uint8_t buffer[4+2+256*4];
	int len = 0;
	int bytes_sent;
	int err;
	int i;

	int payload_length = 2 + num_args * 4;

	buffer[len++] = 0;
	buffer[len++] = 0;
	buffer[len++] = (payload_length>>8) & 0xFF;
	buffer[len++] = payload_length & 0xFF;
	buffer[len++] = request;
	buffer[len++] = num_args;

	for (i = 0; i != num_args; i++) {
		buffer[len++] = (args[i] >> 24) & 0xFF;
		buffer[len++] = (args[i] >> 16) & 0xFF;
		buffer[len++] = (args[i] >>	 8) & 0xFF;
		buffer[len++] = (args[i] >>	 0) & 0xFF;
	}

	bytes_sent = send(s->skt, buffer, len, 0);
    EnableQuickAck(s->skt);

	if (bytes_sent != len) {
		LOG_ERROR("sent %d bytes expected to send %d\n", bytes_sent, len);
		return ERROR_FAIL;
	} else {
		return ERROR_OK;
	}
}

int ust_jtagprobe_recv_scan(ust_jtagprobe_t *s, int bit_length, uint8_t *bits) {
	uint8_t buffer[4096];
	int bytelen = (bit_length + 7) / 8;
	int msglen = 4 + bytelen;
	int len = 0;
	int recv_len = 0;

	do {
		recv_len += recv(s->skt, buffer + recv_len, msglen - recv_len, 0);
	} while (recv_len < msglen);
	if (recv_len != msglen) {
		LOG_ERROR("received %d bytes expected %d\n", recv_len, msglen);
		LOG_ERROR("sw_link len %d\n", (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]);
		return ERROR_FAIL;
	}
	
	#ifndef _WIN32
		/* This disables delayed acks, windows does not support this option, BUT does disable
		   delayed acks on loopback adapter. NOTE: Change is not permanent so needs resetting
		   after all TCP socket communications */
		char flag = 1;
		setsockopt(s->skt, IPPROTO_TCP, TCP_QUICKACK, &flag, sizeof(int));
	#endif

	memcpy(bits, buffer+4, bytelen);

	return ERROR_OK;
}
