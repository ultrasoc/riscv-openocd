#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <process.h>
#include <ws2tcpip.h>
#define close closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#include "ust_tcf.h"
#include "../ust_common/network.h"

#define RECV_BUFFER_DEFAULT_CAPACITY 1024
#define PKT_DATA_DEFAULT_CAPACITY 1024

#define TOKEN_RADIX (10000)
#define MAX_TOKEN_SIZE (15)
#define MAX_LABEL_SIZE (15)
#define MAX_EVENT_NAME_SIZE (50)
#define MAX_HEADER_SIZE (8)
#define MAX_SENT_PKT_SIZE (256)
#define MAX_CMD_SIZE (MAX_SENT_PKT_SIZE - 5)

enum ust_tcf_type {
	UST_TCF_COMMAND = 'C',
	UST_TCF_RESULT = 'R',
	UST_TCF_EVENT = 'E',
	UST_TCF_PROGRESS = 'P',
	UST_TCF_UNRECOGNIZED = 'N',
	UST_TCF_INVALID = 'X',
};

typedef struct ust_tcf_pkt_t {
	char token[MAX_TOKEN_SIZE];
	enum ust_tcf_type type;
	char label[MAX_LABEL_SIZE];
	char event_name[MAX_EVENT_NAME_SIZE];
	char *data;
	unsigned long data_size;
	unsigned long data_capacity;
} ust_tcf_pkt_t;

typedef struct ust_tcf_t {
	int skt;
	int token_count;
	char *recv_buffer;
	unsigned long recv_buffer_size;
	unsigned long recv_buffer_capacity;
	ust_tcf_pkt_t pkt;
} ust_tcf_t;

ust_tcf_t * ust_tcf_create() {
	ust_tcf_t * s = calloc(1, sizeof(ust_tcf_t));
	if (!s) {
		LOG_ERROR("Allocation failed");
		return NULL;
	}
	s->recv_buffer = malloc(RECV_BUFFER_DEFAULT_CAPACITY);
	if (!s->recv_buffer) {
		LOG_ERROR("Allocation failed");
		free(s);
		return NULL;
	}
	s->recv_buffer_capacity = RECV_BUFFER_DEFAULT_CAPACITY;

	s->pkt.data = malloc(PKT_DATA_DEFAULT_CAPACITY);
	if (!s->pkt.data) {
		LOG_ERROR("Allocation failed");
		free(s->recv_buffer);
		free(s);
		return NULL;
	}

	return s;
}

void ust_tcf_destroy(ust_tcf_t * s) {
	free(s->pkt.data);
	free(s->recv_buffer);
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
		rc = getnameinfo(addr->ai_addr, addr->ai_addrlen, host_name,
						 sizeof host_name, serv_name, sizeof serv_name,
						 NI_NUMERICSERV);
		if (rc) {
			LOG_ERROR("getnameinfo failed");
			return ERROR_FAIL;
		}
		s->skt = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
		if (s->skt == -1)
			continue;

		//EnableTcpNoDelay(s->skt);

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

int ust_tcf_run_cmd(ust_tcf_t *s, char *label, char *function, char *data, int data_len)
{
	char buffer[MAX_SENT_PKT_SIZE];
	char token[MAX_TOKEN_SIZE];
	char header[MAX_HEADER_SIZE];
	char *empty_string = "";
	int bytes_sent;
	int len;
	int size;
	int header_size;

	if (!label) {
		label = empty_string;
	}
	if (!function) {
		function = empty_string;
	}
	if (!data) {
		data = empty_string;
	}
	size = snprintf(token, MAX_TOKEN_SIZE, "openocd%d", s->token_count);
	if (size >= MAX_TOKEN_SIZE) {
		LOG_ERROR("Tcf token larger than buffer");
		return ERROR_FAIL;
	}
	s->token_count += s->token_count % TOKEN_RADIX;

	/* Currently assuming sent 'data' doesn't have any nuls in it */
	len = 5 + strlen(token) + strlen(label) + strlen(function) + data_len;
	header_size = snprintf(buffer, MAX_HEADER_SIZE, "[%d%c]", len, '\0');
	if (header_size >= MAX_HEADER_SIZE) {
		LOG_ERROR("Tcf header larger than buffer");
		return ERROR_FAIL;
	}

	len += header_size;

	size = snprintf(buffer + header_size, MAX_SENT_PKT_SIZE - data_len - size,
					"C%c%s%c%s%c%s%c",
					'\0', token, '\0', label, '\0', function, '\0');
	if (size >= MAX_SENT_PKT_SIZE - data_len - size) {
		LOG_ERROR("Tcf header larger than buffer");
		return ERROR_FAIL;
	}

	memcpy(buffer + header_size + size, data, data_len);

	LOG_DEBUG_IO("Sending tcf cmd: %s %s %s %s %s", header, token, label, function, data);

	bytes_sent = send(s->skt, buffer, len, 0);
	EnableQuickAck(s->skt);
    
	if (bytes_sent != len) {
		LOG_ERROR("sent %d bytes expected to send %d\n", bytes_sent, len);
		return ERROR_FAIL;
	} else {
		return ERROR_OK;
	}
}

//#define TRACE_RECV

static int do_recv(int skt, char *buffer, size_t len, char *text) {
	int remaining = len;
	int length = 0;

#ifdef TRACE_RECV
    fprintf(stderr, "OCDRECV %s len=%llu\n", text, len);
#endif

    while (remaining > 0) {
		int recv_len = recv(skt, buffer + length, remaining, 0);
		EnableQuickAck(skt);

		if (recv_len <= 0) {
			/* Check the errno */
			LOG_ERROR("recv failed len: %d errno %d %s",
					  recv_len, errno, strerror(errno));
			return ERROR_FAIL;
		}
		remaining -= recv_len;
		length += recv_len;
	}
#ifdef TRACE_RECV
    {
        size_t i;
        fprintf(stderr, "OCDRECV %s: ", text);
        for (i = 0; i < len; i++) {
            uint8_t byte = *(buffer+i);
            if (!isprint(byte)) {
                fprintf(stderr, "(%02x)", byte);
            } else {
                fprintf(stderr, "%c", byte);
            }
        }
        fprintf(stderr, "\n");
        fflush(stderr);
    }
#endif

	return ERROR_OK;
}

static int recv_pkt(ust_tcf_t *s)
{
	char header[MAX_HEADER_SIZE];
	size_t length;
	unsigned long payload_len;
	int err;
	// The header is at least 4 bytes long
	length = 4;
	err = do_recv(s->skt, header, length, "HEADER");
	if (err != ERROR_OK) {
		LOG_ERROR("Failed to receive tcf header");
		return err;
	}
	LOG_DEBUG_IO("Received tcf header: %s", header);
	if (header[0] != '[') {
		LOG_ERROR("Malformed TCF packet, missing starting [");
		return ERROR_FAIL;
	}
	while (header[length - 1] != ']') {
		if (length >= MAX_HEADER_SIZE) {
			LOG_ERROR("Malformed TCF packet, missing ]");
		}
		err = do_recv(s->skt, header + length, 1, "HEADER_ERR");
		if (err != ERROR_OK) {
			LOG_ERROR("Failed to recv tcf header");
			return err;
		}
		length++;
	}
	payload_len = strtoul(header + 1, NULL, 10);
	if (payload_len == 0) {
		LOG_ERROR("Failed to parse len in header: %s",
				  header);
	}

	if (payload_len > s->recv_buffer_capacity) {
		// Grow the buffer
		s->recv_buffer = realloc(s->recv_buffer, payload_len);
		if (!s->recv_buffer) {
			LOG_ERROR("Allocation fail");
			return ERROR_FAIL;
		}
	}

	err = do_recv(s->skt, s->recv_buffer, payload_len, "PAYLOAD");
	if (err != ERROR_OK) {
		LOG_ERROR("Failed to receive tcf payload");
		return err;
	}
	s->recv_buffer_size = payload_len;

	return ERROR_OK;
}

static int parse_pkt(ust_tcf_t *s)
{
	// Assume the protocol is the UltraSoC specific "binary.1"
	if (s->recv_buffer_size < 4) {
		LOG_ERROR("Too short tcf packet");
		return ERROR_FAIL;
	}
	size_t pos = 0;
	unsigned long size;
	char error;
	s->pkt.type = s->recv_buffer[pos++];
	switch(s->pkt.type) {
	case UST_TCF_EVENT:
		LOG_DEBUG_IO("Received tcf event");
		if (s->recv_buffer[pos++] != '\0') {
			LOG_ERROR("Malformed TCF event packet. Missing NUL.");
			return ERROR_FAIL;
		}
		size = snprintf(s->pkt.label, MAX_LABEL_SIZE, "%s",
						s->recv_buffer + pos);
		if (size >= MAX_LABEL_SIZE) {
			LOG_ERROR("Malformed TCF event packet. Couldn't read label");
			return ERROR_FAIL;
		}
		LOG_DEBUG_IO("label: %s", s->pkt.label);
		pos += size + 1;
		size = snprintf(s->pkt.event_name, MAX_EVENT_NAME_SIZE, "%s",
						s->recv_buffer + pos);
		if (size >= MAX_EVENT_NAME_SIZE) {
			LOG_ERROR("Malformed TCF event packet. Couldn't read event name");
			return ERROR_FAIL;
		}
		LOG_DEBUG_IO("event_name: %s", s->pkt.event_name);
		pos += size + 1;
		if (s->pkt.data_capacity < s->recv_buffer_size - pos) {
			s->pkt.data = realloc(s->pkt.data, (s->recv_buffer_size - pos)+1);
			if (!s->pkt.data) {
				LOG_ERROR("Allocation fail");
				return ERROR_FAIL;
			}
			s->pkt.data_capacity = s->recv_buffer_size - pos;
		}
		memset(s->pkt.data, 0, s->pkt.data_capacity+1);
		memcpy(s->pkt.data, s->recv_buffer + pos, s->recv_buffer_size - pos);
		s->pkt.data_size = s->recv_buffer_size - pos;
		LOG_DEBUG_IO("event_data: %s", s->pkt.data);
		break;
	case UST_TCF_RESULT:
	case UST_TCF_UNRECOGNIZED:
	case UST_TCF_PROGRESS:
		LOG_DEBUG_IO("Received tcf response '%c'", s->pkt.type);
		if (s->recv_buffer[pos++] != '\0') {
			LOG_ERROR("Malformed TCF response packet. Missing NUL.");
			return ERROR_FAIL;
		}
		size = snprintf(s->pkt.token, MAX_TOKEN_SIZE, "%s", s->recv_buffer + pos);
		if (size >= MAX_TOKEN_SIZE) {
			LOG_ERROR("Malformed TCF response packet. Couldn't read token.");
			return ERROR_FAIL;
		}
		pos += size + 1;
		error = s->recv_buffer[pos];
		if (error != 0) {
			// TODO: Recover?
			LOG_ERROR("TCF error: %d", error);
			return ERROR_FAIL;
		}
		size = snprintf(s->pkt.token, MAX_LABEL_SIZE, "%s", s->recv_buffer + pos);
		if (size >= MAX_LABEL_SIZE) {
			LOG_ERROR("Malformed TCF response packet. Couldn't read label.");
			return ERROR_FAIL;
		}
		pos += size + 1;
		if (s->pkt.data_capacity < s->recv_buffer_size - pos) {
			s->pkt.data = realloc(s->pkt.data, (s->recv_buffer_size - pos)+1);
			if (!s->pkt.data) {
				LOG_ERROR("Allocation fail");
				return ERROR_FAIL;
			}
			s->pkt.data_capacity = s->recv_buffer_size - pos;
		}
		memset(s->pkt.data, 0, s->pkt.data_capacity+1);
		memcpy(s->pkt.data, s->recv_buffer + pos, s->recv_buffer_size - pos);
		s->pkt.data_size = s->recv_buffer_size - pos;
		break;
	default:
		LOG_ERROR("Unexpected tcf packet type: %c", s->recv_buffer[1]);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int ust_tcf_recv(ust_tcf_t *s)
{
	int err;
	err = recv_pkt(s);
	if (err != ERROR_OK) {
		return err;
	}
	err = parse_pkt(s);
	if (err != ERROR_OK) {
		return err;
	}

	return ERROR_OK;
}

/* Waits for a tcf event packet discarding
 * other packets in the mean time. */
int ust_tcf_wait_for_event(ust_tcf_t *s, char **label, char **event_name,
						   char **data, int *data_len)
{
	int err;

	do {
		err = ust_tcf_recv(s);
		if (err != ERROR_OK) {
			return err;
		}
	} while(s->pkt.type != UST_TCF_EVENT);

	// NOTE: no copy here
	if (label) {
		*label = s->pkt.label;
	}
	if (event_name) {
		*event_name = s->pkt.event_name;
	}
	if (data && data_len) {
		*data = s->pkt.data;
		*data_len = s->pkt.data_size;
	}

	return ERROR_OK;
}

/* Waits for a tcf 'result' or 'unrecognised' packet discarding
 * other packets in the mean time Including 'progress' packets. */
int ust_tcf_wait_for_response(ust_tcf_t *s, char **data, int *data_len)
{
	int err;

	do {
		err = ust_tcf_recv(s);
		if (err != ERROR_OK) {
			return err;
		}
	} while (s->pkt.type != UST_TCF_RESULT &&
			 s->pkt.type != UST_TCF_UNRECOGNIZED);

	if (s->pkt.type == UST_TCF_UNRECOGNIZED) {
		LOG_ERROR("tcf 'unrecognized' packet received");
		return ERROR_FAIL;
	}

	// NOTE: No copy here
	if (data && data_len) {
		*data = s->pkt.data;
		*data_len = s->pkt.data_size;
	}

	return ERROR_OK;
}
