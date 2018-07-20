#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

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

#include "ust_tcf.h"

typedef struct ust_tcf_t {
	int skt;
} ust_tcf_t;

ust_tcf_t * ust_tcf_create() {
	ust_tcf_t * s = calloc(1, sizeof(ust_tcf_t));
	return s;
}

void ust_tcf_destroy(ust_tcf_t * s) {
	free(s);
}

int ust_tcf_connect(ust_tcf_t *s, const char *host, const char *port) {
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
		if (rc != -1)
			break;
		close(s->skt);
	}
	if (!addr) {
		LOG_ERROR("Failed to connect to a tcf port at %s:%s", host, port);
		return ERROR_FAIL;
	}
	return ERROR_OK;
}

int ust_tcf_disconnect(ust_tcf_t *s)
{
	int err;
	err = close(s->skt);
	if (err) {
		LOG_ERROR("Failed to close tcf socket");
		return ERROR_FAIL;
	} else {
		return ERROR_OK;
	}
}

