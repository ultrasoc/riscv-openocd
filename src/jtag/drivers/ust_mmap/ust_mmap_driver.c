#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define MMAP_SANITY_TEST 0

#include "ust_mmap.h"

#include "jtag/mmap/mmap_interface.h"

#include "jtag/interface.h"
#include "jtag/commands.h"

char * ust_mmap_host;
char * ust_mmap_port;
char * ust_mmap_bpam_name;
char * ust_mmap_filter;
bool sendFilterConfigure = false;
ust_mmap_t * ust_ctx;

static int ust_mmap_init(void)
{
	int err;
	LOG_INFO("ust_mmap intializing driver");

	ust_ctx = ust_mmap_create(ust_mmap_bpam_name);
	if (!ust_ctx) {
		LOG_ERROR("Error in ust_mmap_create");
		return ERROR_FAIL;
	}
	err = ust_mmap_connect(ust_ctx, ust_mmap_host, ust_mmap_port);
	if (err != ERROR_OK) {
		LOG_ERROR("Error connecting socket for ust_mmap driver");
		ust_mmap_destroy(ust_ctx);
		return err;
	}
	err = ust_mmap_check_memory_service(ust_ctx);
	if (err != ERROR_OK) {
		LOG_ERROR("Error checking the memory service");
		ust_mmap_disconnect(ust_ctx);
		ust_mmap_destroy(ust_ctx);
		return err;
	}

	if (sendFilterConfigure) {
		err = ust_mmap_initialise_filter(ust_ctx, ust_mmap_filter);
		if (err != ERROR_OK) {
			LOG_ERROR("Error initialising filter");
			ust_mmap_disconnect(ust_ctx);
			ust_mmap_destroy(ust_ctx);
			return err;
		}
	}
	LOG_INFO("ust_mmap driver initialized");

	if (MMAP_SANITY_TEST) { // TODO: Consider adding this as a command
		err = ust_mmap_test(ust_ctx);
		if (err != ERROR_OK) {
			ust_mmap_destroy(ust_ctx);
		return err;
		}
	}

	return ERROR_OK;
}

static int ust_mmap_quit(void)
{
	LOG_INFO("ust_mmap quit");

	ust_mmap_disconnect(ust_ctx);
	ust_mmap_destroy(ust_ctx);

	return ERROR_OK;
}

COMMAND_HANDLER(ust_mmap_handle_port_command)
{
	if (CMD_ARGC == 1) {
		free(ust_mmap_port);
		ust_mmap_port = strdup(CMD_ARGV[0]);
		return ERROR_OK;
	}
	return ERROR_COMMAND_SYNTAX_ERROR;
}

COMMAND_HANDLER(ust_mmap_handle_host_command)
{
	if (CMD_ARGC == 1) {
		free(ust_mmap_host);
		ust_mmap_host = strdup(CMD_ARGV[0]);
		return ERROR_OK;
	}
	return ERROR_COMMAND_SYNTAX_ERROR;
}

COMMAND_HANDLER(ust_mmap_handle_bpam_name_command)
{
	if (CMD_ARGC == 1) {
		free(ust_mmap_bpam_name);
		ust_mmap_bpam_name = strdup(CMD_ARGV[0]);
		return ERROR_OK;
	}
	return ERROR_COMMAND_SYNTAX_ERROR;
}

COMMAND_HANDLER(ust_mmap_handle_filter_command)
{
	if (CMD_ARGC == 1) {
		free(ust_mmap_filter);
		ust_mmap_filter = strdup(CMD_ARGV[0]);
		sendFilterConfigure = true;
		return ERROR_OK;
	}
	return ERROR_COMMAND_SYNTAX_ERROR;
}

static const struct command_registration ust_mmap_command_handlers[] = {
	{
		.name = "ust_mmap_port",
		.handler = ust_mmap_handle_port_command,
		.mode = COMMAND_CONFIG,
		.help = "Set the port to use to connect to.\n",
		.usage = "port_number",
	},
	{
		.name = "ust_mmap_host",
		.handler = ust_mmap_handle_host_command,
		.mode = COMMAND_CONFIG,
		.help = "Set the host to use to connect to.\n",
		.usage = "host_name",
	},
	{
		.name = "ust_mmap_bpam_name",
		.handler = ust_mmap_handle_bpam_name_command,
		.mode = COMMAND_CONFIG,
		.help = "Set the bpam to use.\n",
		.usage = "bpam_name",
	},
	{
		.name = "ust_mmap_use_filter",
		.handler = ust_mmap_handle_filter_command,
		.mode = COMMAND_CONFIG,
		.help = "Configure the bpam to use DMA filter.\n",
		.usage = "filter_name",
	},
	COMMAND_REGISTRATION_DONE,
};

static int ust_mmap_execute_queue(void)
{
	/* Nothing to do */
	return ERROR_OK;
}

int ust_mmap_do_read(uint64_t addr, int byte_len, uint64_t *value)
{
	return ust_mmap_read(ust_ctx, addr, byte_len, value);
}

int ust_mmap_do_write(uint64_t addr, int byte_len, uint64_t value)
{
	return ust_mmap_write(ust_ctx, addr, byte_len, value);
}

const struct mmap_interface ust_mmap = {
	.read = ust_mmap_do_read,
	.write = ust_mmap_do_write,
};

static const char* const ust_mmap_transports[] = { "mmap", NULL };

struct jtag_interface ust_mmap_interface = {
	.name = "ust_mmap",
	.execute_queue = &ust_mmap_execute_queue,
	.transports = ust_mmap_transports,
	.mmap = &ust_mmap,
	.commands = ust_mmap_command_handlers,
	.init = &ust_mmap_init,
	.quit = &ust_mmap_quit,
};
