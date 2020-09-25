/***************************************************************************
 *	 Copyright (C) 2016 UltraSoC Techonologies Ltd						   *
 *	 ben.horgan@ultrasoc.com											   *
 *																		   *
 *	 This program is free software; you can redistribute it and/or modify  *
 *	 it under the terms of the GNU General Public License as published by  *
 *	 the Free Software Foundation; either version 2 of the License, or	   *
 *	 (at your option) any later version.								   *
 *																		   *
 *	 This program is distributed in the hope that it will be useful,	   *
 *	 but WITHOUT ANY WARRANTY; without even the implied warranty of		   *
 *	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the		   *
 *	 GNU General Public License for more details.						   *
 *																		   *
 *	 You should have received a copy of the GNU General Public License	   *
 *	 along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ust_jtagprobe.h"

#include "jtag/interface.h"
#include "jtag/commands.h"

char * ust_jtagprobe_host;
char * ust_jtagprobe_port;
ust_jtagprobe_t * ust_ctx;

bool ust_version_info_sent = false;
int ust_version = 1;

static int ust_jtagprobe_init(void)
{
	int err;
	LOG_INFO("ust_jtagprobe intializing driver");

	ust_ctx = ust_jtagprobe_create();
	err = ust_jtagprobe_connect(ust_ctx,
								ust_jtagprobe_host,
								ust_jtagprobe_port);
	if (err != ERROR_OK) {
		LOG_ERROR("Error initializing ust_jtagprobe driver");
		ust_jtagprobe_destroy(ust_ctx);
		return ERROR_FAIL;
	}

	LOG_INFO("ust_jtagprobe driver initialized");
	return ERROR_OK;
}

static int ust_jtagprobe_quit(void)
{
	LOG_INFO("ust_jtagprobe quit");

	ust_jtagprobe_disconnect(ust_ctx);
	ust_jtagprobe_destroy(ust_ctx);

	return ERROR_OK;
}

double avg = 0.0;
uint64_t avg_count = 0;
bool start_avg = true;
static int ust_scan(bool ir_scan, enum scan_type type,
					uint8_t *buffer, int scan_size, struct jtag_tap *tap)
{
    LARGE_INTEGER start, end, elapsed, freq;
    QueryPerformanceFrequency(&freq);
    
	int err;
	// We assume this is the only tap or that chain position is dealt
	// with by the connection utility.
	if (type == SCAN_IN) {
		// Just push zeros through
		uint8_t *zeros = calloc(scan_size, sizeof(uint8_t));
		LOG_WARNING("Pushing zeros through the scan chain to get a scan from the device.");
        QueryPerformanceCounter(&start);
		err = ust_jtagprobe_send_scan(ust_ctx, !ir_scan, 0, scan_size, zeros, tap);
		free(zeros);
	} else {
        QueryPerformanceCounter(&start);
		err = ust_jtagprobe_send_scan(ust_ctx, !ir_scan, type == SCAN_OUT,
									  scan_size, buffer, tap);
	}

	if (err != ERROR_OK) {
		LOG_ERROR("ust_jtagprobe send scan failed");
		return ERROR_FAIL;
	}

	if (type != SCAN_OUT) {
		err = ust_jtagprobe_recv_scan(ust_ctx, scan_size, buffer, tap);
        QueryPerformanceCounter(&end);
        elapsed.QuadPart = end.QuadPart - start.QuadPart;
        if (start_avg && avg_count == 3) 
        {
            avg = (elapsed.QuadPart / (double)freq.QuadPart);
            avg_count = 0;
            start_avg = false;
        }
        else if (avg_count > 3)
            avg = ((avg * avg_count) + (elapsed.QuadPart / (double)freq.QuadPart)) / (avg_count + 1);
        
        ++avg_count;
        
        printf("Round trip time(scan size %d): %f (%f)\n", scan_size, elapsed.QuadPart / (double)freq.QuadPart, avg);
        
		if (err != ERROR_OK) {
			LOG_ERROR("ust_jtagprobe recv scan failed");
			return ERROR_FAIL;
		}
	}
	return ERROR_OK;
}

COMMAND_HANDLER(ust_jtagprobe_handle_port_command)
{
	if (CMD_ARGC == 1) {
		free(ust_jtagprobe_port);
		ust_jtagprobe_port = strdup(CMD_ARGV[0]);
		return ERROR_OK;
	}
	return ERROR_COMMAND_SYNTAX_ERROR;
}

COMMAND_HANDLER(ust_jtagprobe_handle_host_command)
{
	if (CMD_ARGC == 1) {
		free(ust_jtagprobe_host);
		ust_jtagprobe_host = strdup(CMD_ARGV[0]);
		return ERROR_OK;
	}
	return ERROR_COMMAND_SYNTAX_ERROR;
}

static const struct command_registration ust_jtagprobe_command_handlers[] = {
	{
		.name = "ust_jtagprobe_port",
		.handler = ust_jtagprobe_handle_port_command,
		.mode = COMMAND_CONFIG,
		.help = "Set the port to use to connect to the remote jtag.\n",
		.usage = "port_number",
	},
	{
		.name = "ust_jtagprobe_host",
		.handler = ust_jtagprobe_handle_host_command,
		.mode = COMMAND_CONFIG,
		.help = "Set the host to use to connect to the remote jtag.\n",
		.usage = "host_name",
	},
	COMMAND_REGISTRATION_DONE,
};

int ust_jtagprobe_execute_queue(void)
{
	struct jtag_command *cmd = jtag_command_queue;  //this has the tap in - luke
	int scan_size;
	enum scan_type type;
	uint8_t *buffer;
	int retval;

	retval = ERROR_OK;

	if(ust_version == 2 && !(ust_version_info_sent))
	{
		uint32_t args[1]  = {2};
		ust_jtagprobe_send_cmd(ust_ctx, JTAGPROBE_NETWORK_VERSION, 1, args);
		ust_version_info_sent = true;
		ust_version = args[0];
	}

	while (cmd) {

	    /*if(cmd->tap == NULL)
	    {
		printf("\n Command type with null pam %d \n\n", cmd->type);
		cmd = cmd->next;
		continue;
	    }*/



		switch (cmd->type) {
		case JTAG_RESET:
			LOG_WARNING("Ignoring request to reset trst: %i srst %i",
						cmd->cmd.reset->trst,
						cmd->cmd.reset->srst);
			// ignore
			break;
		case JTAG_RUNTEST:
		{
			int i;
			LOG_DEBUG("runtest %i cycles, end in %s",
					  cmd->cmd.runtest->num_cycles,
					  tap_state_name(cmd->cmd.runtest->end_state));
			if (cmd->cmd.runtest->end_state != TAP_IDLE) {
				LOG_ERROR("Ignoring non-idle end state, %s. Exiting",
						  tap_state_name(cmd->cmd.runtest->end_state) );
				exit(-1);
			}
			for (i=0; i<cmd->cmd.runtest->num_cycles; i++) {
				if (ust_jtagprobe_send_cmd(ust_ctx, JTAGPROBE_IDLE,
										   0, NULL) != 0) {
					retval = ERROR_JTAG_QUEUE_FAILED;
				}
			}
			break;
		}
		case JTAG_STABLECLOCKS:
			LOG_WARNING("Ignoring: stableclocks %i cycles",
						cmd->cmd.stableclocks->num_cycles);
			// ignore
			break;
		case JTAG_TLR_RESET:
			LOG_WARNING("Ignoring request to do TLR Reset.");
			// ignore
			break;
		case JTAG_PATHMOVE:
			LOG_ERROR("UNSUPPORTED: pathmove: %i states, end in %s. Exiting.",
					  cmd->cmd.pathmove->num_states,
					  tap_state_name(cmd->cmd.pathmove->path[cmd->cmd.pathmove->num_states - 1]));
			exit(-1);
			break;
		case JTAG_SCAN:
			LOG_DEBUG("%s scan end in %s",
					  (cmd->cmd.scan->ir_scan) ? "IR" : "DR",
					  tap_state_name(cmd->cmd.scan->end_state));
			if (cmd->cmd.scan->end_state != TAP_IDLE) {
				LOG_WARNING("Ignoring: End state %s",
							tap_state_name(cmd->cmd.scan->end_state));
			}
			scan_size = jtag_build_buffer(cmd->cmd.scan, &buffer);
            if (scan_size == 0)
            {
                printf("----- ERROR SCAN SIZE 0 -----\n");
            }
			type = jtag_scan_type(cmd->cmd.scan);
			if (ust_scan(cmd->cmd.scan->ir_scan,
						 type, buffer, scan_size, cmd->tap) != ERROR_OK) {
				retval = ERROR_JTAG_QUEUE_FAILED;
			}
			if (jtag_read_buffer(buffer, cmd->cmd.scan) != ERROR_OK)
				retval = ERROR_JTAG_QUEUE_FAILED;
			if (buffer)
				free(buffer);
			break;
		case JTAG_SLEEP:
			LOG_WARNING("Ignoring request to sleep %" PRIi32 "us", cmd->cmd.sleep->us);
			// ignore
			break;
		case JTAG_TMS:
			LOG_ERROR("UNSUPPORTED: Request to explicitly set tms. Exiting.");
			exit(-1);
			break;
		default:
			LOG_ERROR("BUG: unknown JTAG command type encountered");
			exit(-1);
		}
		cmd = cmd->next;
	}

	return retval;
}


struct jtag_interface ust_jtagprobe_interface = {
	.name = "ust_jtagprobe",
	.execute_queue = &ust_jtagprobe_execute_queue,
	.commands = ust_jtagprobe_command_handlers,
	.init = &ust_jtagprobe_init,
	.quit = &ust_jtagprobe_quit,
};
