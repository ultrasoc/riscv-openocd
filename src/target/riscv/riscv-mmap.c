/*
 * MMAP transport for RISC-V, debug version 0.13, which is currently (2/4/17) the
 * latest draft.
 */
#include <stdint.h>
#include <assert.h>
#include "target/target.h"
#include "jtag/interface.h"
#include "jtag/mmap/mmap_interface.h"
#include "transport/transport.h"

extern struct jtag_interface *jtag_interface;

uint32_t emulate_dtmcontrol_scan(struct target *target, uint32_t out)
{
	assert( 0 == ( out & ((1<<17)|(1<<16)))); // FIXME: emulate reset some other way

	//extern struct jtag_interface *jtag_interface;
	//uint64_t val = out;
	//jtag_interface->mmap->write(DTMCONTROL, 4, val );
	//jtag_interface->mmap->read(DTMCONTROL, 4, &val );
	//in = (uint32_t)val;
	uint32_t in = (23<<4)|1;

	return in;
}

uint32_t riscv_mmap_read32( uint64_t address )
{
	uint64_t val;
	jtag_interface->mmap->read( address, 4, &val );

	return (uint32_t)val;
}

void riscv_mmap_write32( uint64_t address, uint32_t value )
{
	jtag_interface->mmap->write( address, 4, value );
}
