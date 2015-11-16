#ifndef _SPECANNOTATION_H
#define _SPECANNOTATION_H

#include "spec_lib.h"
#include "modeltypes.h"

#define SPEC_ANALYSIS 1

typedef call_id_t (*id_func_t)(void *info, thread_id_t tid);
typedef bool (*check_action_func_t)(void *info, call_id_t id, thread_id_t tid);
//typedef bool (*check_action_func_t)(void *info, call_id_t id);

typedef void (*void_func_t)();

typedef bool (*check_commutativity_t)(void *info1, void *info);

typedef enum spec_anno_type {
	INIT, INTERFACE_BEGIN, POTENTIAL_CP_DEFINE, CP_DEFINE,
	CP_DEFINE_CHECK, CP_CLEAR, INTERFACE_END, HB_CONDITION, COMMUTATIVITY_RULE } spec_anno_type; 
typedef
struct spec_annotation {
	spec_anno_type type;
	void *annotation;
} spec_annotation;


/**
	The interfaces and their HB conditions are represented by integers.
*/
typedef
struct hb_rule {
	int interface_num_before;
	int hb_condition_num_before;
	int interface_num_after;
	int hb_condition_num_after;
} hb_rule;


/**
	This struct contains a commutativity rule: two method calls represented by
	two unique integers and a function that takes two "info" pointers (with the
	return value and the arguments) and returns a boolean to represent whether
	the two method calls are commutable.
*/
typedef
struct commutativity_rule {
	int interface_num_before;
	int interface_num_after;
	check_commutativity_t condition;
} commutativity_rule;


typedef
struct anno_init {
	void_func_t init_func;
	void_func_t cleanup_func;

	void **func_table;
	int func_table_size;
	
	hb_rule **hb_rule_table;
	int hb_rule_table_size;

	// For commutativity rules
	commutativity_rule **commutativity_rule_table;
	int commutativity_rule_table_size;
} anno_init;


typedef
struct anno_interface_begin {
	int interface_num;
	char *interface_name; // For debugging only
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
	char *label_name; // For debugging
} anno_potential_cp_define;

typedef
struct anno_cp_define {
	int label_num;
	int potential_cp_label_num;
	int interface_num;
	
	char *label_name; // For debugging
	char *potential_label_name; // For debugging
} anno_cp_define;

typedef
struct anno_cp_define_check {
	int label_num;
	int interface_num;
	char *label_name; // For debugging
} anno_cp_define_check;

typedef
struct anno_cp_clear {
	int label_num;
	int interface_num;
	char* label_name;
} anno_cp_clear;


typedef
struct anno_hb_condition {
	int interface_num;
	int hb_condition_num;
} anno_hb_condition;

#endif
