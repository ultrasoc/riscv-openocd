#ifndef ARM11_DBGTAP_H
#define ARM11_DBGTAP_H

#include "arm11.h"

/* ARM11 internals */

void arm11_setup_field(struct arm11_common *arm11, int num_bits,
		void *in_data, void *out_data, struct scan_field *field);
void arm11_add_IR(struct arm11_common *arm11,
		uint8_t instr, tap_state_t state);
int arm11_add_debug_SCAN_N(struct arm11_common *arm11,
		uint8_t chain, tap_state_t state);
void arm11_add_debug_INST(struct arm11_common *arm11,
		uint32_t inst, uint8_t *flag, tap_state_t state);
int arm11_read_DSCR(struct arm11_common *arm11, uint32_t *dscr);
int arm11_write_DSCR(struct arm11_common *arm11, uint32_t dscr);

enum target_debug_reason arm11_get_DSCR_debug_reason(uint32_t dscr);

int arm11_run_instr_data_prepare(struct arm11_common *arm11);
int arm11_run_instr_data_finish(struct arm11_common *arm11);
int arm11_run_instr_no_data(struct arm11_common *arm11,
		uint32_t *opcode, size_t count);
int arm11_run_instr_no_data1(struct arm11_common *arm11, uint32_t opcode);
int arm11_run_instr_data_to_core(struct arm11_common *arm11,
		uint32_t opcode, uint32_t *data, size_t count);
int arm11_run_instr_data_to_core_noack(struct arm11_common *arm11,
		uint32_t opcode, uint32_t *data, size_t count);
int arm11_run_instr_data_to_core1(struct arm11_common *arm11,
		uint32_t opcode, uint32_t data);
int arm11_run_instr_data_from_core(struct arm11_common *arm11,
		uint32_t opcode, uint32_t *data, size_t count);
int arm11_run_instr_data_from_core_via_r0(struct arm11_common *arm11,
		uint32_t opcode, uint32_t *data);
int arm11_run_instr_data_to_core_via_r0(struct arm11_common *arm11,
		uint32_t opcode, uint32_t data);

int arm11_add_dr_scan_vc(int num_fields, struct scan_field *fields,
		tap_state_t state);
int arm11_add_ir_scan_vc(int num_fields, struct scan_field *fields,
		tap_state_t state);

/**
 * Used with arm11_sc7_run to make a list of read/write commands for
 * scan chain 7
 */
struct arm11_sc7_action
{
	bool write; /**< Access mode: true for write, false for read. */
	uint8_t	address; /**< Register address mode. Use enum #arm11_sc7 */
	/**
	 * If write then set this to value to be written.  In read mode
	 * this receives the read value when the function returns.
	 */
	uint32_t value;
};

int arm11_sc7_run(struct arm11_common *arm11,
		struct arm11_sc7_action *actions, size_t count);

/* Mid-level helper functions */
void arm11_sc7_clear_vbw(struct arm11_common *arm11);
void arm11_sc7_set_vcr(struct arm11_common *arm11, uint32_t value);

int arm11_read_memory_word(struct arm11_common *arm11,
		uint32_t address, uint32_t *result);

#endif // ARM11_DBGTAP_H
