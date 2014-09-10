#include "specanalysis.h"
#include "action.h"
#include "cyclegraph.h"
#include "threads-model.h"
#include "clockvector.h"
#include "execution.h"
#include <sys/time.h>
#include <assert.h>


SPECAnalysis::SPECAnalysis()
{
	execution = NULL;
	func_table = NULL;
	hb_rules = new hbrule_list_t();
}

SPECAnalysis::~SPECAnalysis() {
}

void SPECAnalysis::setExecution(ModelExecution * execution) {
	this->execution = execution;
}

const char * SPECAnalysis::name() {
	const char * name = "SPEC";
	return name;
}

void SPECAnalysis::finish() {
	freeCPNodes();
	model_print("SPECAnalysis finished!\n");
}

bool SPECAnalysis::option(char * opt) {
	return true;
}

void SPECAnalysis::analyze(action_list_t *actions) {
	if (trace_num_cnt % 1000 == 0)
		model_print("SPECAnalysis analyzing: %d!\n", trace_num_cnt);
	trace_num_cnt++;
	//traverseActions(actions);
	
	buildCPGraph(actions);
	if (cpActions->size() == 0) return;
	buildEdges();
	node_list_t *sorted_commit_points = sortCPGraph();
	bool passed = check(sorted_commit_points);
	if (!passed) {
		model_print("Error exists!!\n");
	} else {
		//model_print("Passed all the safety checks!\n");
	}
}

bool SPECAnalysis::check(node_list_t *sorted_commit_points) {
	bool passed = true;
	// Actions and simple checks first
	node_list_t::iterator iter;
	for (iter = sorted_commit_points->begin(); iter !=
		sorted_commit_points->end(); iter++) {
		commit_point_node *node = *iter;
		int interface_num = node->interface_num;
		id_func_t id_func = (id_func_t) func_table->table[2 * interface_num];
		check_action_func_t check_action = (check_action_func_t) func_table->table[2 * interface_num + 1];
		call_id_t __ID__ = id_func();
		node->__ID__ = __ID__;
		void *info = node->info;
		passed = check_action(info, __ID__);
		if (!passed) {
			model_print("%d interface call failded\n", interface_num);
			model_print("ID: %llu\n", __ID__);
			model_print("Error exists in simple check!!\n");
			return false;
		}
		//model_print("%d interface call passed\n", interface_num);
	}

	// Happens-before checks
	if (hb_rules->size() == 0) // No need to check
		return passed;
	node_list_t::iterator next_iter;
	// Iterate commit point nodes
	for (iter = sorted_commit_points->begin(); iter !=
		sorted_commit_points->end(); iter++) {
		commit_point_node *n1 = *iter;
		if (n1->hb_conds == NULL) continue; // No happens-before check
		// Iterate HB conditions of n1
		for (hbcond_list_t::iterator cond_iter1 = n1->hb_conds->begin();
			cond_iter1 != n1->hb_conds->end(); cond_iter1++) {
			anno_hb_condition *cond1 = *cond_iter1;
			// Find a rule that matches n1
			for (hbrule_list_t::iterator rule_iter = hb_rules->begin();
				rule_iter != hb_rules->end(); rule_iter++) {
				anno_hb_init *rule = *rule_iter;
				if (rule->interface_num_before != cond1->interface_num
					|| rule->hb_condition_num_before != cond1->hb_condition_num) {
					continue;
				}
				// Iterate the later nodes n2
				for (next_iter = iter, next_iter++; next_iter !=
					sorted_commit_points->end(); next_iter++) {
					commit_point_node *n2 = *next_iter;
					if (n1->__ID__ == n2->__ID__) {
						if (n2->hb_conds == NULL) continue; // No hb conditions
						// Find a later node that matches the rule
						for (hbcond_list_t::iterator cond_iter2 = n2->hb_conds->begin();
							cond_iter2 != n2->hb_conds->end(); cond_iter2++) {
							anno_hb_condition *cond2 = *cond_iter2;
							if (rule->interface_num_after !=
								cond2->interface_num ||
								rule->hb_condition_num_after !=
								cond2->hb_condition_num) {
								continue;
							}
							// Check hb here
							if (!n1->begin->happens_before(n2->end)) {
								model_print("Error exists in hb check!!\n");
								return false;
							} else {
								//model_print("hey passed one HB check!\n");
							}
						}
					}
				}
			}
		}
	}
	return passed;
}

/**
	Topologically sort the commit points graph
*/
node_list_t* SPECAnalysis::sortCPGraph() {
	node_list_t *sorted_list = new node_list_t();
	node_list_t *stack = new node_list_t();
	int time_stamp = 1;
	commit_point_node *node;
	action_list_t::iterator iter = cpActions->begin();
	node = cpGraph->get(*iter);
	stack->push_back(node);
	for (; iter != cpActions->end(); iter++) {
		node = cpGraph->get(*iter);
		if (node->color == 0) { // Not discovered yet
			stack->push_back(node);
		}
		while (stack->size() > 0) {
			node = stack->back();
			stack->pop_back();
			if (node->color == 0) { // Just discover this node before
				node->color = 1;
				stack->push_back(node);
				if (node->edges != NULL) { // Push back undiscovered nodes
					// Cautious!! Push HB & MO edges first then RF
					for (edge_list_t::reverse_iterator rit =
						node->edges->rbegin(); rit != node->edges->rend(); rit++) {
						commit_point_node *next_node = (*rit)->next_node;
						if (next_node->color == 0) {
							stack->push_back(next_node);
						}
					}
				}
			} else if (node->color == 1) { // Ready to visit
				node->color = 2;
				node->finish_time_stamp = time_stamp++;
				sorted_list->push_front(node);
			}
		}
	}
	return sorted_list;
}

void SPECAnalysis::buildEdges() {
	//model_print("Building edges...\n");
	action_list_t::iterator iter1, iter2;
	CycleGraph *mo_graph = execution->get_mo_graph();
	for (iter1 = cpActions->begin(); iter1 != cpActions->end(); iter1++) {
		for (iter2 = iter1, iter2++; iter2 != cpActions->end(); iter2++) {
			// Build happens-before edges
			const ModelAction *act1 = *iter1,
				*act2 = *iter2;
			commit_point_node *node1 = cpGraph->get(act1),	
				*node2 = cpGraph->get(act2);
			// When sorting, we favor RF, so we add RF edge first
			if (act2->get_reads_from() == act1) {
				node1->addEdge(node2, RF);
			} else if (act1->get_reads_from() == act2) {
				node2->addEdge(node1, RF);
			} else { // Deal with HB or MO
				if (act1->happens_before(act2)) {
					node1->addEdge(node2, HB);
				} else if (act2->happens_before(act1)) {
					node2->addEdge(node1, HB);
				} else { // Deal with MO
					if (mo_graph->checkReachable(act1, act2)) {
						node1->addEdge(node2, MO);
					} else if (mo_graph->checkReachable(act2, act1)) {
						node2->addEdge(node1, MO);
					}
				}
			}
		}
	}
	//model_print("Finish building edges!\n");
}

ModelAction* SPECAnalysis::getPrevAction(action_list_t *actions,
	action_list_t::iterator *iter, const ModelAction *anno) {
	action_list_t::iterator it = *iter;
	int cnt = 0;
	ModelAction *act;
	while (true) {
		it--;
		cnt++;
		act = *it;
		if (anno->get_tid() == act->get_tid()
			&& act->get_type() != ATOMIC_ANNOTATION) {
			for (int i = 0; i < cnt; i++)
				it++;
			return act;
		}
	}
}

commit_point_node* SPECAnalysis::getCPNode(action_list_t *actions, action_list_t::iterator
	*iter) {
	action_list_t::iterator it = *iter;
	const ModelAction *act = *it;
	thread_id_t tid = act->get_tid();
	commit_point_node *node = new commit_point_node();
	spec_annotation *anno = (spec_annotation*) act->get_location();
	assert(anno->type == INTERFACE_BEGIN);
	anno_interface_begin *begin_anno = (anno_interface_begin*) anno->annotation;
	int interface_num = begin_anno->interface_num;
	node->interface_num = interface_num;
	node->begin = act;
	anno_cp_define *cp_define;
	anno_potential_cp_define *pcp_define;
	// A list of potential commit points
	pcp_list_t *pcp_list = new pcp_list_t();
	potential_cp_info *pcp_info;
	bool hasCommitPoint = false, pcp_defined = false;
	for (it++; it != actions->end(); it++) {
		act = *it;
		if (act->get_type() != ATOMIC_ANNOTATION || act->get_tid() != tid
			|| act->get_value() != SPEC_ANALYSIS)
			continue;
		spec_annotation *anno = (spec_annotation*) act->get_location();
		anno_hb_condition *hb_cond;
		switch (anno->type) {
			case POTENTIAL_CP_DEFINE:
				//model_print("POTENTIAL_CP_DEFINE\n");
				// Just wanna make a faster check
				pcp_defined = true;
				pcp_define = (anno_potential_cp_define*) anno->annotation;
				pcp_info = new potential_cp_info();
				pcp_info->label_num = pcp_define->label_num;
				pcp_info->operation = getPrevAction(actions, &it, act);
				pcp_list->push_back(pcp_info);
				break;
			case CP_DEFINE:
				//model_print("CP_DEFINE\n");
				cp_define = (anno_cp_define*) anno->annotation;
				if (!pcp_defined) {
					// Fast check 
					break;
				} else {
					// Check if potential commit has been checked
					for (pcp_list_t::iterator pcp_it = pcp_list->begin(); pcp_it
						!= pcp_list->end(); pcp_it++) {
						pcp_info = *pcp_it;
						if (cp_define->label_num == pcp_info->label_num) {
							if (!hasCommitPoint) {
								hasCommitPoint = true;
								node->operation = pcp_info->operation;
							} else {
								model_print("Multiple commit points.\n");
								return NULL;
							}
						}
					}
				}
				break;
			case CP_DEFINE_CHECK:
				//model_print("CP_DEFINE_CHECK\n");
				node->operation = getPrevAction(actions, &it, act);
				if (hasCommitPoint) {
					model_print("Multiple commit points.\n");
					return NULL;
				}
				hasCommitPoint = true;
				break;
			case HB_CONDITION:
				//model_print("HB_CONDITION\n");
				hb_cond = (anno_hb_condition*) anno->annotation;
				if (hb_cond->interface_num != interface_num) {
					model_print("Bad interleaving of the same thread.\n");
					return NULL;
				}
				if (node->hb_conds == NULL) {
					node->hb_conds = new hbcond_list_t();	
				}
				node->hb_conds->push_back(hb_cond);
				break;
			case INTERFACE_END:
				//model_print("INTERFACE_END\n");
				if (!hasCommitPoint) {
					model_print("Exception: interface without commit points!\n");
					return NULL;
				}
				node->end = act;
				node->info = ((anno_interface_end*) anno->annotation)->info;
				return node;
			default:
				break;
		}
	}
	// Free the potential commit points list
	for (pcp_list_t::iterator free_it = pcp_list->begin(); free_it !=
		pcp_list->end(); free_it++) {
		delete *free_it;
	}
	delete pcp_list;
	return node;
}

void SPECAnalysis::buildCPGraph(action_list_t *actions) {
	cpGraph = new node_table_t();
	cpActions = new action_list_t();
	action_list_t::iterator begin_anno_iter = actions->begin(), iter;
	for (; begin_anno_iter != actions->end(); begin_anno_iter++) {
		const ModelAction *act = *begin_anno_iter;
		if (act->get_type() != ATOMIC_ANNOTATION)
			continue;
		if (act->get_value() != SPEC_ANALYSIS)
			continue;
		spec_annotation *anno = (spec_annotation*) act->get_location();
		if (anno == NULL) {
			model_print("Wrong annotation!\n");
			return;
		}
		anno_hb_init *hb_rule = NULL;
		commit_point_node *node;
		anno_func_table_init *func_table_init;

		anno_interface_begin *begin_anno;
		int interface_num;
		switch (anno->type) {
			case FUNC_TABLE_INIT:
				func_table_init = (anno_func_table_init*) anno->annotation;
				//model_print("FUNC_TABLE_INIT\n");
				if (func_table != NULL) {
					model_print("Multiple function table init annotations!\n");
					break;
				}
				func_table = func_table_init;
				break;
			case HB_INIT:
				//model_print("HB_INIT\n");
				hb_rule = (anno_hb_init*) anno->annotation;
				hb_rules->push_back(hb_rule);
				break;
			case INTERFACE_BEGIN:
				//model_print("INTERFACE_BEGIN\n");
				iter = begin_anno_iter;
				begin_anno = (anno_interface_begin*) anno->annotation;
				interface_num = begin_anno->interface_num;
				node = getCPNode(actions, &iter);
				if (node != NULL) {
					// Store the graph in a hashtable
					cpGraph->put(node->operation, node);
					cpActions->push_back(node->operation);
				}
				break;
			default:
				break;
		}
	}
}

void SPECAnalysis::freeCPNodes() {
}

void SPECAnalysis::dumpGraph() {
	action_list_t::iterator iter = cpActions->begin();
	for (; iter != cpActions->end(); iter++) {
		ModelAction *act = *iter;
		commit_point_node *node = cpGraph->get(act);
		model_print("Node: %d, %d\n", act->get_seq_number(),
			node->interface_num);
		if (node->edges == NULL) continue;
		for (edge_list_t::reverse_iterator rit =
			node->edges->rbegin(); rit != node->edges->rend(); rit++) {
			commit_point_node *next_node = (*rit)->next_node;
			const char *relationMsg = (*rit)->type == HB ? "HB" : (*rit)->type == RF ?
				"RF" : "MO";
			model_print("Edge: %d, %d, %s\n", next_node->operation->get_seq_number(),
				next_node->interface_num, relationMsg);
		}
	}
}

void SPECAnalysis::traverseActions(action_list_t *actions) {
	action_list_t::iterator iter = actions->begin();
	for (; iter != actions->end(); iter++) {
		const ModelAction *act = *iter;
		if (act->get_type() != ATOMIC_ANNOTATION)
			continue;
		if (act->get_value() != SPEC_ANALYSIS)
			continue;
		spec_annotation *anno = (spec_annotation*) act->get_location();
		if (anno == NULL) {
			model_print("Wrong annotation!\n");
			return;
		}
		anno_hb_init *hb_rule = NULL;
		anno_func_table_init *func_table_init;

		anno_interface_begin *begin_anno;
		int interface_num;
		switch (anno->type) {
			case FUNC_TABLE_INIT:
				func_table_init = (anno_func_table_init*) anno->annotation;
				model_print("FUNC_TABLE_INIT\n");
				if (func_table != NULL) {
					model_print("Multiple function table init annotations!\n");
					break;
				}
				break;
			case HB_INIT:
				model_print("HB_INIT\n");
				hb_rule = (anno_hb_init*) anno->annotation;
				break;
			case INTERFACE_BEGIN:
				model_print("INTERFACE_BEGIN\n");
				begin_anno = (anno_interface_begin*) anno->annotation;
				interface_num = begin_anno->interface_num;
				break;
			default:
				break;
		}
	}
}

