#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "arm.h"
#include "arm_adi_v5.h"
#include <helper/time_support.h>

#include <transport/transport.h>
#include <jtag/interface.h>

#include <jtag/mmap/mmap_interface.h>

static int mmap_connect(struct adiv5_dap *dap)
{
	LOG_DEBUG("mmap_connect dummy call");
	return ERROR_OK;
}

static int mmap_queue_dp_read(struct adiv5_dap *dap, unsigned reg,
							  uint32_t *data)
{
	LOG_DEBUG("mmap_queue_dp_read dummy call");
	return ERROR_OK;
}

static int mmap_queue_dp_write(struct adiv5_dap *dap, unsigned reg,
							   uint32_t data)
{
	LOG_DEBUG("mmap_queue_dp_write dummy call");
	return ERROR_OK;
}

static int mmap_queue_ap_read(struct adiv5_ap *ap, unsigned reg,
		uint32_t *data)
{
	struct adiv5_dap *dap = ap->dap;
	const struct mmap_interface *mmap = adiv5_dap_mmap_interface(dap);
	uint64_t value;
	mmap->read(reg, 4, &value);
	*data = (uint32_t) value;
	LOG_DEBUG("mmap_queue_ap_read addr: 0x%08x val: 0x%08x", reg, *data);
	return ERROR_OK;
}

static int mmap_queue_ap_write(struct adiv5_ap *ap, unsigned reg,
		uint32_t data)
{
	LOG_DEBUG("mmap_queue_ap_write addr: 0x%08x val: 0x%08x", reg, data); 
	struct adiv5_dap *dap = ap->dap;
	const struct mmap_interface *mmap = adiv5_dap_mmap_interface(dap);
	mmap->write((uint64_t) reg, 4, (uint64_t) data);
	return ERROR_OK;
}

static int mmap_queue_ap_abort(struct adiv5_dap *dap, uint8_t *ack)
{
	LOG_DEBUG("mmap_queue_ap_abort dummy call");
	return ERROR_OK;
}

static int mmap_run(struct adiv5_dap *dap)
{
	LOG_DEBUG("mmap_run dummy call");
	return ERROR_OK;
}

static int mmap_queue_ap_read_buf(struct adiv5_ap *ap,
							  uint8_t *buffer, uint32_t size,
							  uint32_t count, uint32_t address)
{
	struct adiv5_dap *dap = ap->dap;
	const struct mmap_interface *mmap = adiv5_dap_mmap_interface(dap);
	if (size != 1 && size != 2 && size != 4 && size != 8) {
		LOG_ERROR("Unsupported size %d (not 1, 2, 4 or 8)", size);
		return ERROR_FAIL;
	}
	for (uint32_t i = 0; i < count; i++) {
		// TODO: Assuming target and host are little-endian
		uint64_t value;
		mmap->read(address + (i * size), size, &value);
		if (size == 1) {
			*(buffer + (i * size)) = (uint8_t) value;
		} else if (size == 2) {
			*((uint16_t *)(buffer + (i * size))) = (uint16_t) value;
		} else if (size == 4) {
			*((uint32_t *)(buffer + (i * size))) = (uint32_t) value;
		} else if (size == 8) {
			*((uint64_t *)(buffer + (i * size))) = value;
		}
	}
	return ERROR_OK;
}

static int mmap_queue_ap_write_buf(struct adiv5_ap *ap,
							  const uint8_t *buffer, uint32_t size,
							  uint32_t count, uint32_t address)
{
	struct adiv5_dap *dap = ap->dap;
	const struct mmap_interface *mmap = adiv5_dap_mmap_interface(dap);
	if (size != 1 && size != 2 && size != 4 && size != 8) {
		LOG_ERROR("Unsupported size %d (not 1, 2, 4 or 8)", size);
		return ERROR_FAIL;
	}

	for (uint32_t i = 0; i < count; i++) {
		// TODO: Assuming target and host are little-endian
		uint64_t value = 0;
		if (size == 1) {
			value = (uint64_t)(*((uint8_t *)(buffer + (i * size))));
		} else if (size == 2) {
			value = (uint64_t)(*((uint16_t *)(buffer + (i * size))));
		} else if (size == 4) {
			value = (uint64_t)(*((uint32_t *)(buffer + (i * size))));
		} else if (size == 8) {
			value = (uint64_t)(*((uint64_t *)(buffer + (i * size))));
		}
		mmap->write((uint64_t) address + (i * size), size, value);
	}
	return ERROR_OK;
}

const struct dap_ops mmap_dap_ops = {
	.connect = mmap_connect,
	.queue_dp_read = mmap_queue_dp_read,
	.queue_dp_write = mmap_queue_dp_write,
	.queue_ap_read = mmap_queue_ap_read,
	.queue_ap_write = mmap_queue_ap_write,
	.queue_ap_abort = mmap_queue_ap_abort,
	.run = mmap_run,
	.queue_ap_read_buf = mmap_queue_ap_read_buf,
	.queue_ap_write_buf = mmap_queue_ap_write_buf,
};
