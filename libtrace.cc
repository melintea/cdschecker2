#include <list>
#include <cdstrace.h>
#include "execution.h"

/** libtrace.cc */
/**
	This file contains interface definitions that can be used by the
	specification analysis.
*/

/**
	Pass the thread id and index, and it will return the "index"th most recent
	atomic operation.
*/
static ModelAction* getLatestAction(thread_id_t tid, int index) {
	ModelExecution execution = model->get_execution();
	action_list_t *actions = execution->get_action_trace();
	list<ModelAction*>::reverse_iterator iter = actions->rbegin();
	int cnt = index;
	for (; iter != actions->rend(); ++iter) {
		ModelAction *action = *iter;
		if (action->get_tid() != tid)
			continue;
		if (--cnt == 0)
			return action;
	}
}

/**
	Pass the current thread id, it returns the value of the previous atomic
	operation (used by __ATOMIC_RET__)
*/
uint64_t get_prev_value(thread_id_t tid) {
	ModelAction *action = getLatestAction(tid, 1);
	// FIXME: CAS, general RMW and load should be all diferent
	if (action->get_type() == ATOMIC_READ) {
		return action->get_reads_from_value();
	}
	// ...
}

/**
	Pass the corresponding label of the @Commit_point_define, and it checks if
	that potential commit point is satisfied.
*/
bool potential_commit_point_satisfied(const char *label) {
	//...
	return true;
}

