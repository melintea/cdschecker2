#ifndef _SPECANNOTATION_H
#define _SPECANNOTATION_H

enum spec_anno_type {
	HB_INIT, INTERFACE_BEGIN, POTENTIIAL_CP_DEFINE, CP_DEFINE, CP_DEFINE_CHECK,
	HB_CONDITION, POST_CHECK, INTERFACE_END, ID
};

typedef
spec_pair {
	int left;
	int right;
} spec_pair;

typedef
struct spec_annotation {
	spec_anno_type type;
	void *annotation;
} spec_anno_t;


/**
	The interfaces and their HB conditions are represented by integers.
*/
typedef
struct anno_HB_init {
	int interface_num_before;
	int hb_condition_num_before;
	int interface_num_after;
	int hb_condition_num_after;
} anno_HB_init;

typedef
struct anno_interface_boundary {
	int interface_num;
	uint64_t call_sequence_num;
} anno_interface_boundary;

typedef
struct anno_id {
	int interface_num;
	uint64_t id;
} anno_id;

typedef
struct anno_potential_cp_define {
	int interface_num;
	int label_num;
	uint64_t call_sequence_num;
} anno_potential_cp_define;

typedef
struct anno_cp_define {
	bool check_passed;
	int interface_num;
	int label_num;
	int potential_cp_label_num;
	uint64_t call_sequence_num;
} anno_cp_define;

typedef
struct anno_cp_define_check {
	bool check_passed;
	int interface_num;
	int label_num;
	uint64_t call_sequence_num;
} anno_cp_define_check;

typedef
struct anno_hb_condition {
	int interface_num;
	int hb_condition_num;
	uint64_t id;
	uint64_t call_sequence_num;
} anno_hb_condition;

typedef
struct anno_post_check {
	bool check_passed;
	int interface_num;
	uint64_t call_sequence_num;
} anno_post_check;

#endif
