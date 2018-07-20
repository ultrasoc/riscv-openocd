#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ust_mmap.h"
#include "ust_tcf.h"

typedef struct ust_mmap_t
{
	ust_tcf_t *tcf;
} ust_mmap_t;

ust_mmap_t * ust_mmap_create()
{
	ust_mmap_t * s = calloc(1, sizeof(ust_mmap_t));
	s->tcf = ust_tcf_create();
	return s;
}

void ust_mmap_destroy(ust_mmap_t * s)
{
	ust_tcf_destroy(s->tcf);
	free(s);
}

int ust_mmap_connect(ust_mmap_t *s, const char *host, const char *port)
{
	return ust_tcf_connect(s->tcf, host, port);
}

int ust_mmap_disconnect(ust_mmap_t *s)
{
	return ust_tcf_disconnect(s->tcf);
}

int ust_mmap_read(ust_mmap_t *s, uint64_t addr, int byte_len, uint8_t *data)
{
	/* BRH: TODO */
	return ERROR_OK;
}

int ust_mmap_write(ust_mmap_t *s, uint64_t addr, int byte_len, uint8_t *data)
{
	/* BRH: TODO */
	return ERROR_OK;
}
