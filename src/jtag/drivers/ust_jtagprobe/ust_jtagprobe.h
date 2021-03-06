#ifndef OPENOCD_UST_JTAGPROBE_H
#define OPENOCD_UST_JTAGPROBE_H

#include <jtag/jtag.h>

typedef enum jtagprobe_request_t {
	JTAGPROBE_RESET_TRST = 1,
	JTAGPROBE_RESET = 2,
	JTAGPROBE_IDLE = 3,
	JTAGPROBE_SHIFT_IR = 4,
	JTAGPROBE_SHIFT_DR = 5,
	JTAGPROBE_NETWORK_VERSION = 7,

	JTAGPROBE_ENABLE = 100,        // ENABLE/DISABLE
	JTAGPROBE_CLOCK_DIVIDER = 101, // CLOCK_DIVIDER
	JTAGPROBE_PAUSELESS = 102,      // PAUSE/PAUSELESS MODE
	JTAGPROBE_FREERUN = 103
} jtagprobe_request_t;

typedef enum jtagprobe_flags_t {
	JTAGPROBE_FLAG_NO_RESPONSE = (1 << 0),
	JTAGPROBE_FLAG_NO_RETURN_TO_IDLE = (1 << 1)
} jtagprobe_flags_t;

typedef struct ust_jtagprobe_t ust_jtagprobe_t;

ust_jtagprobe_t * ust_jtagprobe_create(void);
void ust_jtagprobe_destroy(ust_jtagprobe_t *s);
int  ust_jtagprobe_connect(ust_jtagprobe_t *s, const char *host, const char *port);
int  ust_jtagprobe_disconnect(ust_jtagprobe_t *s);
int  ust_jtagprobe_send_scan(ust_jtagprobe_t *s, int is_data, int no_response, int bit_length, uint8_t *bits, struct jtag_tap *tap);
int  ust_jtagprobe_recv_scan(ust_jtagprobe_t *s, int bit_length, uint8_t *bits, struct jtag_tap *tap);

int  ust_jtagprobe_send_cmd(ust_jtagprobe_t *s, int request, uint8_t num_args, uint32_t args[], struct jtag_tap *tap);

#endif /* OPENOCD_UST_JTAGPROBE_H */
