#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ust_mmap.h"

#include "jtag/interface.h"
#include "jtag/commands.h"

char * ust_mmap_host;
char * ust_mmap_port;

static int ust_mmap_init(void)
{
	/* TODO */
	return ERROR_OK;
}

static int ust_mmap_quit(void)
{
	/* TODO */
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
	COMMAND_REGISTRATION_DONE,
};

static int ust_mmap_execute_queue(void)
{
	/* TODO */
	return ERROR_OK;
}

struct jtag_interface ust_mmap_interface = {
	.name = "ust_mmap",
	.execute_queue = &ust_mmap_execute_queue,
	.commands = ust_mmap_command_handlers,
	.init = &ust_mmap_init,
	.quit = &ust_mmap_quit,
};
