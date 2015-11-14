#ifndef _SPECANNOTATION_H
#define _SPECANNOTATION_H

#include "spec_lib.h"
#include "modeltypes.h"

#define SPEC_ANALYSIS 1

typedef call_id_t (*id_func_t)(void *info, thread_id_t tid);
typedef bool (*check_action_func_t)(void *info, call_id_t id, thread_id_t tid);
//typedef bool (*check_action_func_t)(void *info, call_id_t id);

typedef bool (*check_commutativity_t)(void *info1, void *info);

typedef enum spec_anno_type {
	INIT, INTERFACE_BEGIN, POTENTIAL_CP_DEFINE, CP_DEFINE,
	CP_DEFINE_CHECK, CP_CLEAR, INTERFACE_END, HB_CONDITION, COMMUTATIVITY_RULE
} spec_anno_type;

typedef
struct spec_annotation {
	spec_anno_type type;
	void *annotation;
} spec_annotation;


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


/**
	This struct contains a commutativity rule: two method calls represented by
	two unique integers and a function that takes two "info" pointers (with the
	return value and the arguments) and returns a boolean to represent whether
	the two method calls are commutable.
*/
typedef
struct anno_commutativity_rule {
	int interface_num_before;
	int interface_num_after;
	check_commutativity_t condition;
} anno_commutativity_rule;


typedef
struct anno_init {
	void **func_table;
	int func_table_size;
	
	anno_hb_init **hb_init_table;
	int hb_init_table_size;

	// For commutativity rules
	anno_commutativity_rule **commutativity_rule_table;
	int commutativity_rule_size;
} anno_init;


inline void free_anno_init(anno_init *tgt) {
	free(tgt->func_table);
	for (int i = 0; i < tgt->hb_init_table_size; i++) {
		free(tgt->hb_init_table[i]);
	}
	free(tgt->hb_init_table);
}


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
	int interface_num;
} anno_cp_define;

typedef
struct anno_cp_define_check {
	int label_num;
	int interface_num;
} anno_cp_define_check;

typedef
struct anno_cp_clear {
	int label_num;
	int interface_num;
} anno_cp_clear;


typedef
struct anno_hb_condition {
	int interface_num;
	int hb_condition_num;
} anno_hb_condition;

#endif
