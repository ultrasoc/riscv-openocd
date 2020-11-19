#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../../jtag.h"
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

extern int ust_version;

#define V1_DATA_START           4
#define V2_DATA_START           6
#define V2_PAM_LOCATION_OUT     6
#define V2_PAM_LOCATION_IN      4



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

int ust_jtagprobe_send_scan(ust_jtagprobe_t *s, int is_data, int no_response, int bit_length, uint8_t *bits, struct jtag_tap *tap) {
	uint8_t buffer[4096];
	int bytes_sent;
	int err;

	int bytelen = (bit_length + 7) / 8;

	int msglen = 4 + bytelen;
	if(ust_version == 2)
	{
        msglen += 2;
	}

	int len = 0;

	//build up scan to send to agent
	buffer[len++] = 0;
	buffer[len++] = 0;
	buffer[len++] = (msglen >> 8) & 0xFF;
	buffer[len++] = msglen & 0xFF;
	buffer[len++] = is_data ? JTAGPROBE_SHIFT_DR : JTAGPROBE_SHIFT_IR;
	buffer[len++] = no_response ? JTAGPROBE_FLAG_NO_RESPONSE : 0;

	//If the pam index has been set pass it to agent
	if(ust_version == 2)
	{
        assert(len == V2_PAM_LOCATION_OUT);
        assert((tap != NULL) && (tap->pam != 0));
        
        buffer[len++] = ((int)tap->pam >> 8) & 0xFF;
        buffer[len++] = (int)tap->pam & 0xFF;
	}

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
    LOG_ERROR("ust_jtagprobe_send_cmd - DONT HAVE TAP!\n");

	uint8_t buffer[4+2+256*4];
	int len = 0;
	int bytes_sent;
	int err;
	int i;

	int payload_length = 2 + num_args * 4;
    if (ust_version == 2)
	{
        msglen += 2;
	}

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

int ust_jtagprobe_recv_scan(ust_jtagprobe_t *s, int bit_length, uint8_t *bits, struct jtag_tap *tap) {
	uint8_t buffer[4096];
	int bytelen = (bit_length + 7) / 8;
	int len = 0;
	int recv_len = 0;
	int msglen = bytelen + ((ust_version == 2) ? V2_DATA_START : V1_DATA_START);
    
	do {
		recv_len += recv(s->skt, buffer + recv_len, msglen - recv_len, 0);
		EnableQuickAck(s->skt);
	} while (recv_len < msglen);
	if (recv_len != msglen) {
		LOG_ERROR("received %d bytes expected %d\n", recv_len, msglen);
		LOG_ERROR("sw_link len %d\n", (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3]);
		return ERROR_FAIL;
	}
    
    if (ust_version == 1)
    {
        memcpy(bits, buffer + V1_DATA_START, bytelen);
    }
    else
    {
        assert(tap);
        assert(tap->pam != 0);
        
        // Check we have received data from correct PAM
        uint16_t pam = *((uint16_t*)(buffer + V2_PAM_LOCATION_IN));
        if (pam != tap->pam)
        {
            LOG_ERROR("Incoming data received from incorrect debug probe(%d != %d). Expect errors.", pam, tap->pam);
        }
        
        memcpy(bits, buffer + V2_DATA_START, bytelen);
    }
        
	return ERROR_OK;
}
