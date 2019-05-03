#ifndef OPENOCD_UST_TCF_H
#define OPENOCD_UST_TCF_H

#include <jtag/jtag.h>

typedef struct ust_tcf_t ust_tcf_t;

ust_tcf_t * ust_tcf_create(void);
void ust_tcf_destroy(ust_tcf_t *s);
int ust_tcf_connect(ust_tcf_t *s, const char *host, const char *port);
int ust_tcf_disconnect(ust_tcf_t *s);
int ust_tcf_run_cmd(ust_tcf_t *s, char *label, char *function, char *data,
					int data_len);
int ust_tcf_recv(ust_tcf_t *s);
int ust_tcf_wait_for_event(ust_tcf_t *s, char **label, char **event_name,
						   char **data, int *data_len);
int ust_tcf_wait_for_response(ust_tcf_t *s, char **data, int *data_len);

#endif /* OPENOCD_UST_TCF_H */
