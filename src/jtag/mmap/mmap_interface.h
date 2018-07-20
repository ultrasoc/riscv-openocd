#ifndef OPENOCD_JTAG_MMAP_MMAP_INTERFACE_H
#define OPENOCD_JTAG_MMAP_MMAP_INTERFACE_H

struct mmap_interface {
	int (*read)(uint64_t addr, int byte_len, uint8_t *data);
	int (*write)(uint64_t addr, int byte_len, uint8_t *data);
};

#endif /* OPENOCD_JTAG_MMAP_MMAP_INTERFACE_H */
