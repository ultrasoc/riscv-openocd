#ifndef OPENOCD_UST_MMAP_H
#define OPENOCD_UST_MMAP_H

#include <jtag/jtag.h>

typedef struct ust_mmap_t ust_mmap_t;

ust_mmap_t * ust_mmap_create(void);
void ust_mmap_destroy(ust_mmap_t *s);
int ust_mmap_connect(ust_mmap_t *s, const char *host, const char *port);
int ust_mmap_disconnect(ust_mmap_t *s);
int ust_mmap_read(ust_mmap_t *s, uint64_t addr, int byte_len, uint8_t *data);
int ust_mmap_write(ust_mmap_t *s, uint64_t addr, int byte_len, uint8_t *data);

#endif /* OPENOCD_UST_MMAP_H */
