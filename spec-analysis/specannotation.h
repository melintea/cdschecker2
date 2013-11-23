#ifndef _SPECANNOTATION_H
#define _SPECANNOTATION_H
#include "spec_lib.h"

const unsigned int SPEC_ANALYSIS = 1 << 31;

typedef call_id_t (*id_func_t)();
typedef bool (*check_action_func_t)(void *info, call_id_t id);

typedef enum spec_anno_type {
	FUNC_TABLE_INIT, HB_INIT, INTERFACE_BEGIN, POTENTIAL_CP_DEFINE, CP_DEFINE,
	CP_DEFINE_CHECK, INTERFACE_END, HB_CONDITION
} spec_anno_type;

typedef
struct spec_annotation {
	spec_anno_type type;
	void *annotation;
} spec_annotation;

typedef
struct anno_func_table_init {
	int size;
	void **table;
} anno_func_table_init;

/**
	The interfaces and their HB conditions are represented by integers.
*/
typedef
struct anno_hb_init {
	int interface_num_before;
	int hb_condition_num_before;
	int interface_num_after;
	int hb_condition_num_after;
} anno_hb_init;

typedef
struct anno_interface_begin {
	int interface_num;
	char *msg; // For debugging only
} anno_interface_begin;

/* 
	The follwoing function pointers can be put in a static table, not
	necessarily stored in every function call.

	id_func_t id;
	check_action_func_t check_action;
*/
typedef
struct anno_interface_end {
	int interface_num;
	void *info; // Return value & arguments
} anno_interface_end;

typedef
struct anno_potential_cp_define {
	int label_num;
} anno_potential_cp_define;

typedef
struct anno_cp_define {
	int label_num;
	int potential_cp_label_num;
} anno_cp_define;

typedef
struct anno_cp_define_check {
	int label_num;
} anno_cp_define_check;

typedef
struct anno_hb_condition {
	int interface_num;
	int hb_condition_num;
} anno_hb_condition;

#endif
