#ifndef OPENOCD_UST_TCF_H
#define OPENOCD_UST_TCF_H

#include <jtag/jtag.h>

typedef struct ust_tcf_t ust_tcf_t;

ust_tcf_t * ust_tcf_create(void);
void ust_tcf_destroy(ust_tcf_t *s);
int ust_tcf_connect(ust_tcf_t *s, const char *host, const char *port);
int ust_tcf_disconnect(ust_tcf_t *s);

#endif /* OPENOCD_UST_TCF_H */
