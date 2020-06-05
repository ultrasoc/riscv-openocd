#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "ust_mmap.h"
#include "ust_tcf.h"

#define MAX_DMA_NAME_SIZE (7)
#define MAX_FUNCTION_NAME_SIZE (32)
#define MAX_DATA_SIZE (128)

extern bool sendFilterConfigure;

typedef struct ust_mmap_t
{
	ust_tcf_t *tcf;
	char dma_name[MAX_DMA_NAME_SIZE];
} ust_mmap_t;

ust_mmap_t * ust_mmap_create(char *dma_name)
{
	ust_mmap_t * s = calloc(1, sizeof(ust_mmap_t));
	if (!s) {
		LOG_ERROR("Allocation failed");
		return NULL;
	}
	s->tcf = ust_tcf_create();
	strcpy(s->dma_name, dma_name);
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

int ust_mmap_check_memory_service(ust_mmap_t *s) {
	char *service = "ddma";
	char *data;
	int data_len;
	int err;
	err = ust_tcf_run_cmd(s->tcf, "smgr", "isreg", service, strlen(service) + 1);
	if (err != ERROR_OK) {
		return err;
	}
	err = ust_tcf_wait_for_response(s->tcf, &data, &data_len);
	if (err != ERROR_OK) {
		return err;
	}
	if (data_len >= 1 && data[0] == '1') {
		return ERROR_OK;
	} else {
		LOG_ERROR("Memory service not registered in ust udagent");
		return ERROR_FAIL;
	}
}

int ust_mmap_initialise_filter(ust_mmap_t *s, const char *filter) {
	char function[MAX_FUNCTION_NAME_SIZE];
	char send_data[MAX_DATA_SIZE];
	int size;
	int err;
	char *resp;
	int resp_len;

	size = snprintf(function, sizeof(function), "set-prop(%s)", s->dma_name);
	if (size >= (int)sizeof(function)) {
		LOG_ERROR("Function name larger than buffer");
		return ERROR_FAIL;
	}

	size = snprintf(send_data, sizeof(send_data), "filter=%s->%s", s->dma_name,
					filter);
	if (size >= (int)sizeof(send_data)) {
		LOG_ERROR("Data larger than buffer");
		return ERROR_FAIL;
	}

	ust_tcf_run_cmd(s->tcf, "ddma", function, send_data, size + 1);
	err = ust_tcf_wait_for_response(s->tcf, &resp, &resp_len);
	if (err != ERROR_OK) {
		LOG_ERROR("tcf response gives error");
		return err;
	}

	return ERROR_OK;
}

int ust_mmap_set_axprot_mode(ust_mmap_t *s, const char *val) 
{
	char function[MAX_FUNCTION_NAME_SIZE];
	char send_data[MAX_DATA_SIZE];
	size_t size;
	int err;
	char *resp;
	int resp_len;

	size = snprintf(function, sizeof(function), "set-prop(%s)", s->dma_name);
	if (size >= (int)sizeof(function)) {
		LOG_ERROR("Function name larger than buffer");
		return ERROR_FAIL;
	}

	// Having base as 0: automatically converts using hex if "0x" is prefixed. Else, base 10 is used
	long int converted_val = strtol(val, NULL, 0);
	if( converted_val < 0 )
	{
		LOG_ERROR("Negative number given");
		return ERROR_FAIL;
	} else if( converted_val >= 8 )	// Make sure only three bit values are defined
	{
		LOG_ERROR("AxPROT only has three bits. Value defined is bigger than 7");
		return ERROR_FAIL;
	}
	else
	{
		uint8_t* low_byte = (uint8_t *)converted_val; // Get the lowest byte with the 
		bool privilege = (*low_byte & 0x1);
		bool secure =     (*low_byte & 0x2) >> 1; 
		bool class =      (*low_byte & 0x4) >> 2;

		/** Privilege */
		size = snprintf(send_data, sizeof(send_data), "privilege=%d", privilege);
		ust_tcf_run_cmd(s->tcf, "ddma", function, send_data, size + 1);
		err = ust_tcf_wait_for_response(s->tcf, &resp, &resp_len);
		if (err != ERROR_OK) {
			LOG_ERROR("tcf response gives error");
			return err;
		}

		/** Secure-bit */
		memset(&send_data, 0, MAX_DATA_SIZE); // Clear the array 
		size = snprintf(send_data, sizeof(send_data), "secure=%d", secure);
		err = ust_tcf_run_cmd(s->tcf, "ddma", function, send_data, size + 1);
		if (err != ERROR_OK) {
			LOG_ERROR("tcf response gives error");
			return err;
		}

		/** Class */
		memset(&send_data, 0, MAX_DATA_SIZE); // Clear the array 
		size = snprintf(send_data, sizeof(send_data), "class=%d", class);
		err = ust_tcf_run_cmd(s->tcf, "ddma", function, send_data, size + 1);
		if (err != ERROR_OK) {
			LOG_ERROR("tcf response gives error");
			return err;
		}

	}

	return ERROR_OK;

}

int ust_mmap_disconnect(ust_mmap_t *s)
{
	return ust_tcf_disconnect(s->tcf);
}

int ust_mmap_read(ust_mmap_t *s, uint64_t addr, int byte_len, uint64_t *value)
{
	char function[MAX_FUNCTION_NAME_SIZE];
	char send_data[MAX_DATA_SIZE];
	int size;
	int err;
	char *resp;
	int resp_len;

	if (byte_len <= 0 || byte_len > 8) {
		// TODO: Support larger byte_len
		LOG_ERROR("Invalid byte_len = %d. Expected 0 < byte_len <= 8", byte_len);
	}

	size = snprintf(function, MAX_FUNCTION_NAME_SIZE, "read(%s%s)", s->dma_name,
			sendFilterConfigure ? ",fltr" : "");
	if (size >= MAX_FUNCTION_NAME_SIZE) {
		LOG_ERROR("Function name larger than buffer");
		return ERROR_FAIL;
	}

	size = snprintf(send_data, MAX_DATA_SIZE, "0x%" PRIx64 " %d",
					addr, byte_len);
	if (size >= MAX_DATA_SIZE) {
		LOG_ERROR("Data larger than buffer");
		return ERROR_FAIL;
	}

	ust_tcf_run_cmd(s->tcf, "ddma", function, send_data, size + 1);
	err = ust_tcf_wait_for_response(s->tcf, &resp, &resp_len);
	if (err != ERROR_OK) {
		LOG_ERROR("tcf response gives error");
		return err;
	}

	/* Wait for the read response in an event */
	while (1) {
		char *service;
		char *event_name;
		char *data;
		int data_len;
		err = ust_tcf_wait_for_event(s->tcf, &service, &event_name,
									 &data, &data_len);
		if (err != ERROR_OK) {
			return err;
		}
		if (!strcmp(service, "ddma")) {
			char buffer[MAX_DATA_SIZE + 1];
			memcpy(buffer, data, data_len);
			buffer[data_len] = '\0';
			*value = strtoul(buffer, 0, 16);
			break;
		}
	}

	return ERROR_OK;
}

int ust_mmap_write(ust_mmap_t *s, uint64_t addr, int byte_len, uint64_t value)
{
	char function[MAX_FUNCTION_NAME_SIZE];
	char data[MAX_DATA_SIZE];
	int size;
	char *resp;
	int resp_len;
	int err;

	if (byte_len < 0 || byte_len > 8) {
		// TODO: Support larger byte_len
		LOG_ERROR("Invalid byte_len = %d. Expected 0 < byte_len <= 8", byte_len);
		return ERROR_FAIL;
	}

	size = snprintf(function, MAX_FUNCTION_NAME_SIZE, "write(%s%s)", s->dma_name,
			sendFilterConfigure ? ",fltr" : "");

	if (size >= MAX_FUNCTION_NAME_SIZE) {
		LOG_ERROR("Function name larger than buffer");
		return ERROR_FAIL;
	}

	size = snprintf(data, MAX_DATA_SIZE, "0x%" PRIx64 " %d 0x%" PRIx64,
					addr, byte_len, value);
	if (size >= MAX_DATA_SIZE) {
		LOG_ERROR("Data larger than buffer");
		return ERROR_FAIL;
	}

	ust_tcf_run_cmd(s->tcf, "ddma", function, data, size + 1);
	err = ust_tcf_wait_for_response(s->tcf, &resp, &resp_len);
	if (err != ERROR_OK) {
		LOG_ERROR("tcf response gives error");
		return err;
	}
	return ERROR_OK;
}

/* Test code */
int ust_mmap_test(ust_mmap_t *s) {
	uint64_t addr = 0x70000000;
	uint64_t value_write = 0x12344321;
	uint64_t value_read;
	int err;

	err = ust_mmap_write(s, addr, 4, value_write);
	if (err != ERROR_OK) {
		LOG_ERROR("Error testing mmap driver write");
		return err;
	}
	err = ust_mmap_read(s, addr, 4, &value_read);
	if (err != ERROR_OK) {
		LOG_ERROR("Error testing mmap driver read");
		return err;
	}
	if (value_write != value_read) {
		LOG_ERROR("Read value does not match written value");
		return ERROR_FAIL;
	}
	return err;
}
