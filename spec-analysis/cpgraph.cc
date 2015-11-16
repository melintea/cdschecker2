#include "cpgraph.h"
#include "action.h"
#include "cyclegraph.h"
#include "threads-model.h"
#include "clockvector.h"
#include "execution.h"
#include <sys/time.h>
#include <assert.h>
#include <iterator>
#include "modeltypes.h"
#include "model-assert.h"


/********************    PotentialCPInfo    ********************/
PotentialCPInfo::PotentialCPInfo(ModelAction *op, char *label, int num) :
	operation(op),
	labelName(label),
	labelNum(num)
{

}



/********************    CommitPoint    ********************/
CommitPoint::CommitPoint(ModelAction *op, char *label, int num) :
	operation(op),
	labelName(label),
	labelNum(num)
{

}


/********************    CPEdge    ********************/
CPEdge::CPEdge(CPEdgeType t, CPNode *next) {
	type = t;
	nextNode = next;
}

CPEdgeType CPEdge::getType() {
	return type;
}

CPNode* CPEdge::getNextNode() {
	return nextNode;
}



/********************    CPNode    ********************/

/**
	Initialize some necessary lists that we do not allow to be null.
*/
CPNode::CPNode() {
	commitPoints = new CPList;
	hbConditions = new HBConditionList;
	edges = new CPEdgeList;
}

ModelAction* CPNode::getBegin() {
	return begin;
}

void CPNode::setBegin(ModelAction *act) {
	begin = act;
}

ModelAction* CPNode::getEnd() {
	return end;
}

void CPNode::setEnd(ModelAction *act) {
	end = act;
}

CPList* CPNode::getCommitPoints() {
	return commitPoints;
}


HBConditionList* CPNode::getHBConditions() {
	return hbConditions;
}


call_id_t CPNode::getID() {
	return __ID__;
}

void CPNode::setID(call_id_t id) {
	__ID__ = id;
}

int CPNode::getInterfaceNum() {
	return interfaceNum;
}

void CPNode::setInterfaceNum(int num) {
	interfaceNum = num;
}

char* CPNode::getInterfaceName() {
	return interfaceName;
}

void CPNode::setInterfaceName(char *name) {
	interfaceName = name;
}

void* CPNode::getInfo() {
	return info;
}

void CPNode::setInfo(void *ptr) {
	info = ptr;
}

int CPNode::getColor() {
	return color;
}

void CPNode::setColor(int c) {
	color = c;
}


CPEdgeList* CPNode::getEdgeList() {
	return edges;
}

void CPNode::addEdge(CPNode *next, CPEdgeType type)
{
	// MODEL_ASSERT (edges != NULL); // always be true

	// First check if this edge is already added
	for (CPEdgeList::iterator iter = edges->begin(); iter != edges->end();
		iter++) {
		CPEdge *e = *iter;
		if (e->getNextNode() == next && e->getType() == type)
			return;
	}
	CPEdge *newEdge = new CPEdge(type, next);
	edges->push_back(newEdge);
}


/********************    CPGraph    ********************/

/**
	Initialze the necessary lists and tables
*/
CPGraph::CPGraph(ModelExecution *e) {
	execution = e;
	nodeList = new CPNodeList;
	nodeTable = new CPNodeTable;
	threadLists = new SnapVector<action_list_t*>;

	hbRules = new HBRuleList;
	commuteRules = new CommuteRuleList;
}

/**
	Return the actual annotation if this is an annotation action; otherwise
	return NULL.
*/
spec_annotation* CPGraph::getAnnotation(ModelAction *act) {
	if (act->get_type() != ATOMIC_ANNOTATION)
		return NULL;
	if (act->get_value() != SPEC_ANALYSIS)
		return NULL;
	spec_annotation *anno = (spec_annotation*) act->get_location();
	MODEL_ASSERT (anno);
	return anno;
}

void CPGraph::processInitAnnotation(anno_init *annoInit) {
	// Assign initialization function 
	initFunc = annoInit->init_func;
	MODEL_ASSERT(initFunc);

	// Assign the function table
	funcTable = annoInit->func_table;
	MODEL_ASSERT(funcTable);

	// Initialize the funtion table size
	funcTableSize = annoInit->func_table_size;
	
	// Initialize the hb rules list and size 
	for (int i = 0; i < annoInit->hb_rule_table_size; i++) {
		hb_rule *rule = annoInit->hb_rule_table[i];
		hbRules->push_back(rule);
	}
	hbRulesSize = annoInit->hb_rule_table_size; 
	//model_print("hb_rules size: %d!\n", hb_rules->size())

	// Initialize the commutativity rules list and size 
	for (int i = 0; i < annoInit->commutativity_rule_table_size; i++) {
		commutativity_rule *rule = annoInit->commutativity_rule_table[i];
		commuteRules->push_back(rule);
	}
	commuteRulesSize = annoInit->commutativity_rule_table_size; 
}


void CPGraph::buildThreadLists(action_list_t *actions) {
	int maxthreads = 0;
	for (action_list_t::iterator it = actions->begin(); it != actions->end(); it++) {
		ModelAction *act = *it;
		int threadid = id_to_int(act->get_tid());
		if (threadid == 0)
			continue;
		if (threadid > maxthreads) {
			threadLists->resize(threadid + 1);
			maxthreads = threadid;
		}
		action_list_t *list = (*threadLists)[threadid];
		if (!list) {
			list = new action_list_t;
			(*threadLists)[threadid] = list;
		}
		list->push_back(act);
	}
}

void CPGraph::buildNodesFromThreads() {
	/* We start from the 1st real thread */
	for (unsigned int i = 1; i < threadLists->size(); i++) {
		buildNodesFromThread((*threadLists)[i]);
		if (isBroken) // Early bail when detecting error
			return;
	}
}


/** 
	When called, the current iter points to a beginning annotation; when done,
	the iter points to either the end of the list or the end annotation. It's
	the caller's responsibility to check and increment the iterator afterwards.
*/
CPNode* CPGraph::extractCPNode(action_list_t *actions, action_list_t::iterator &iter) {
	ModelAction *act = *iter;
	CPNode *node = new CPNode;
	spec_annotation *anno = getAnnotation(act);
	MODEL_ASSERT(anno && anno->type == INTERFACE_BEGIN);

	// Partially initialize the commit point node with the already known fields
	anno_interface_begin *annoBegin= (anno_interface_begin*) anno->annotation;
	anno_interface_end* annoEnd;
	int interfaceNum = annoBegin->interface_num;
	node->setInterfaceNum(interfaceNum);
	node->setInterfaceName(annoBegin->interface_name);
	node->setBegin(act);
	// Don't forget the color
	node->setColor(0);

	// Some declaration for the commit points
	anno_cp_define *cpDefine = NULL;
	anno_potential_cp_define *pcpDefine = NULL;
	
	// We can decide whether the commit point immediately
	anno_cp_define_check *cpDefineCheck = NULL;
	// A list of potential commit points
	PotentialCPList *pcpList = new PotentialCPList;
	PotentialCPInfo *pcpInfo = NULL;
	anno_hb_condition *hbCond = NULL;
	// Commit point operation
	CommitPoint *commitPoint;
	ModelAction *operation = NULL;

	// Some specification correctness guards
	bool hasCommitPoint = false;
	bool pcpDefined = false;

	for (iter++; iter != actions->end(); iter++) {
		act = *iter;
		spec_annotation *anno = getAnnotation(act);
		if (!anno)
			continue;
		// Now we are dealing with one annotation
		switch (anno->type) {
			case POTENTIAL_CP_DEFINE:
				//model_print("POTENTIAL_CP_DEFINE\n");
				// Make a faster check
				pcpDefined = true;
				pcpDefine = (anno_potential_cp_define*) anno->annotation;
				iter--; // The previous elelment
				operation = *iter;
				iter++; // Reset the iterator to the current one
				if (operation->get_type() == ATOMIC_ANNOTATION) {
					model_print("Potential commit point should follow an atomic "
						"operation.\n");
					model_print("%s_%d\n", pcpDefine->label_name,
						act->get_seq_number());
					isBroken = true;
					return NULL;
				}

				pcpInfo = new PotentialCPInfo(operation, pcpDefine->label_name,
					pcpDefine->label_num);
				pcpList->push_back(pcpInfo);
				break;
			case CP_DEFINE:
				//model_print("CP_DEFINE\n");
				cpDefine = (anno_cp_define*) anno->annotation;
				if (node->getInterfaceNum() != cpDefine->interface_num) {
					model_print("Commit point definition should be within API "
						"methods (inconsistent interface number.\n");
					isBroken = true;
					return NULL;
				}
				if (!pcpDefined) { // Fast check 
					break;
				} else {
					// Check if potential commit point has appeared
					for (PotentialCPList::iterator pcpIter = pcpList->begin();
						pcpIter != pcpList->end(); pcpIter++) {
						pcpInfo = *pcpIter;
						if (cpDefine->potential_cp_label_num == pcpInfo->labelNum) {
							commitPoint = new CommitPoint(pcpInfo->operation,
								pcpInfo->labelName, pcpInfo->labelNum);
							node->getCommitPoints()->push_back(commitPoint);
							hasCommitPoint = true;
							break; // Done when find the "first" PCP
						}
					}
				}
				break;
			case CP_DEFINE_CHECK:
				//model_print("CP_DEFINE_CHECK\n");
				cpDefineCheck = (anno_cp_define_check*) anno->annotation;
				if (node->getInterfaceNum() != cpDefineCheck->interface_num) {
					model_print("Commit point define check annotation should be "
						"within API methods (inconsistent interface number.\n");
					isBroken = true;
					return NULL;
				}
				
				iter--; // The previous elelment
				operation = *iter;
				iter++; // Reset the iterator to the current one
				if (operation->get_type() == ATOMIC_ANNOTATION) {
					model_print("Commit point (define check) should "
						"follow an atomic operation.\n");
					model_print("%s_%d\n", cpDefineCheck->label_name,
						act->get_seq_number());
					isBroken = true;
					return NULL;
				}

				// Add the commit_point_define_check operation
				commitPoint = new CommitPoint(operation,
					cpDefineCheck->label_name, cpDefineCheck->label_num);
				node->getCommitPoints()->push_back(commitPoint);
				hasCommitPoint = true;
				break;
			
			case CP_CLEAR: 
				//model_print("CP_CLEAR\n");
				// Reset everything
				pcpDefined = false;
				hasCommitPoint = false;
				// Clear the list of potential commit points
				pcpList->clear();
				// Clear the previous list of commit points
				node->getCommitPoints()->clear();
				break;
			case HB_CONDITION:
				//model_print("HB_CONDITION\n");
				hbCond = (anno_hb_condition*) anno->annotation;
				if (hbCond->interface_num != interfaceNum) {
					model_print("Bad interleaving of the same thread (the hb condition).\n");
					return NULL;
				}
				node->getHBConditions()->push_back(hbCond);
				break;
			case INTERFACE_END:
				//model_print("INTERFACE_END\n");
				annoEnd = (anno_interface_end*) anno->annotation;
				if (!hasCommitPoint) {
					model_print("Exception: %d, tid_%d\tinterface %d does not "
						"have commit points!\n", act->get_seq_number(),\
						node->getBegin()->get_tid(), node->getInterfaceNum());
					return NULL;
				} else {
					// Don't forget to finsh the left
					node->setEnd(act);
					node->setInfo(annoEnd->info);
					return node;
				}
			default:
				break;
		}
	}
	
	delete pcpList;
	// The node does not have a closing end annotation
	return NULL;
}

void CPGraph::buildNodesFromThread(action_list_t *actions) {
	action_list_t::iterator iter = actions->begin();
	
	// annoBegin == NULL means we are looking for the beginning annotation
	while (iter != actions->end()) {
		ModelAction *act = *iter;
		spec_annotation *anno = getAnnotation(act);
		if (!anno) {
			iter++;
			continue;
		}
		if (anno->type == INTERFACE_BEGIN) { // Beginning anno
			CPNode *node = extractCPNode(actions, iter);
			if (node != NULL) {
				// Store the node in the graph
				nodeTable->put(act, node);
				nodeList->push_back(node);
				if (iter == actions->end())
					return; // Okay
				else
					iter++; // Increment the iterator
			} else {
				isBroken = true;
				model_print("Error with constructing a complete node.\n");
				return;
			}
		} else if (anno->type != INIT) {
			isBroken = true;
			model_print("Missing beginning annotation.\n");
			return;
		} else { // This is an INIT annotation
			iter++;
		}
	}
}

bool CPGraph::addEdge(CPNode *n1, CPNode *n2) {
	CPList *commitPoints1 = n1->getCommitPoints();
	CPList *commitPoints2 = n2->getCommitPoints();

	// Has an endge from n1 to n2
	bool forwardEdge = false;
	// Has an endge from n2 to n1
	bool backwardEdge = false;

	CPList::iterator iter1, iter2;
	for (iter1 = commitPoints1->begin(); iter1 != commitPoints1->end();
		iter1++) {
		CommitPoint *cp1 = *iter1;
		ModelAction *act1 = cp1->operation;
		for (iter2 = commitPoints2->begin(); iter2 != commitPoints2->end();
			iter2++) {
			CommitPoint *cp2 = *iter2;
			ModelAction *act2 = cp2->operation;
		
			/** Currently use happens-before edge to order the commit points */
			if (act1->happens_before(act2)) {
				n1->addEdge(n2, HB);
				forwardEdge = true;
			} else if (act2->happens_before(act1)) {
				n2->addEdge(n1, HB);
				backwardEdge = true;
			}
		}
	}

	/** When done, check whether the edges are entangled */
	if (forwardEdge && backwardEdge) {
		isBroken = true;
		return false;
	} else {
		return true;
	}
}

/**
	The node will be printed as such:
	Dequeue_8 (T2): LoadNext_10 + UpdateHead_18
*/
void CPGraph::printNode(CPNode *n) {
	ModelAction *begin = n->getBegin();
	int tid = id_to_int(begin->get_tid());
	char *interfaceName = n->getInterfaceName();
	CPList *list = n->getCommitPoints();
	model_print("%s_%d (T%d): ", interfaceName, begin->get_seq_number(), tid);
	CPList::iterator iter = list->begin();
	CommitPoint *cp = *iter;
	model_print("%s_%d", cp->labelName, cp->operation->get_seq_number());
	iter++;
	for (; iter != list->end(); iter++) {
		CommitPoint *cp = *iter;
		model_print(" + %s (%d)", cp->labelName, cp->operation->get_seq_number()); }
	model_print("\n");
}


/**
	The edges will be printed as such:
	"-> Dequeue_8 (T2)"
	"-> Enqueue_20 (T3)"
*/
void CPGraph::printEdges(CPNode *n) {
	CPEdgeList *edges = n->getEdgeList();
	for (CPEdgeList::iterator iter = edges->begin(); iter != edges->end();
		iter++) {
		CPEdge *e = *iter;
		CPNode *next = e->getNextNode();
		ModelAction *begin = next->getBegin();
		int tid = id_to_int(begin->get_tid());
		model_print("\t-> %s_%d (T%d)\n", next->getInterfaceName(),
			begin->get_seq_number(), tid);
	}
}

void CPGraph::printGraph() {
	model_print("\n");
	model_print("------------------  Commit Point Graph (%d)"
	"  ------------------\n", execution->get_execution_number());
	for (CPNodeList::iterator iter = nodeList->begin(); iter != nodeList->end();
		iter++) {
		CPNode *n = *iter;
		/* Print the info the this node */
		printNode(n);
		/* Print the info the edges directly from this node */
		printEdges(n);
	}
	model_print("------------------  End Graph (%d)"
	"  ------------------\n", execution->get_execution_number());
	model_print("\n");
}

void CPGraph::buildEdges() {
	CPNodeList::iterator iter1, iter2;
	for (iter1 = nodeList->begin(); iter1 != nodeList->end(); iter1++) {
		CPNode *n1 = *iter1;
		iter1++;
		iter2 = iter1;
		iter1--;
		for (; iter2 != nodeList->end(); iter2++) {
			CPNode *n2 = *iter2;
			bool added = addEdge(n1, n2);
			if (!added)
				return;
		}
	}
}

void CPGraph::build(action_list_t *actions) {
	buildThreadLists(actions);
	
	// First process the initialization annotation
	bool hasInitAnno = false;
	action_list_t::iterator iter = actions->begin();
	for (; iter != actions->end(); iter++) {
		ModelAction *act = *iter;
		spec_annotation *anno = getAnnotation(act);
		if (anno == NULL)
			continue;
		// Deal with the Initialization annotation
		if (anno->type == INIT) {
			hasInitAnno = true;
			anno_init *annoInit = (anno_init*) anno->annotation;
			processInitAnnotation(annoInit);
			break;
		}
	}
	if (!hasInitAnno) {
		model_print("There is no initialization annotation\n");
		isBroken = true;
		return;
	}

	// Process the nodes for method calls of each thread
	buildNodesFromThreads();
	// Establish the edges
	buildEdges();
}

bool CPGraph::hasEdge(CPNode *n1, CPNode *n2) {
	CPEdgeList *edges = n1->getEdgeList();
	for (CPEdgeList::iterator i = edges->begin(); i != edges->end(); i++) {
		CPEdge *e = *i;
		CPNode *next = e->getNextNode();
		if (next == n2) {
			return true;
		}
	}
	return false;
}

bool CPGraph::checkAdmissibility() {
	CPNodeList::iterator iter1, iter2;
	for (iter1 = nodeList->begin(); iter1 != nodeList->end(); iter1++) {
		CPNode *n1 = *iter1;
		iter2 = iter1++;
		iter1--;
		for (; iter2 != nodeList->end(); iter2++) {
			CPNode *n2 = *iter2;
			bool isEdge = hasEdge(n1, n2);
			if (!isEdge)
				isEdge = hasEdge(n2, n1);
			if (isEdge)
				continue;

			/* Now we need to check whether we have a commutativity rule */
			for (CommuteRuleList::iterator ruleIter = commuteRules->begin();
				ruleIter != commuteRules->end(); ruleIter++) {
				commutativity_rule *rule = *ruleIter;
				if (n1->getInterfaceNum() != rule->interface_num_before ||
					n2->getInterfaceNum() != rule->interface_num_after)
					continue;
				/* Check whether condition is satisfied */
				check_commutativity_t func = rule->condition;
				void *info1 = n1->getInfo();
				void *info2 = n2->getInfo();
				bool satisfied = (*func)(info1, info2);
				if (satisfied) { // The rule allows commutativity
					return true;
				}
			}
		}
	}
	// We have no rules at all or the condition of the rules is not satisfied
	return false;
}


bool CPGraph::checkSequentialSpec() {
	return false;
}

bool CPGraph::checkSynchronization() {
	return false;
}

bool CPGraph::getBroken() {
	return isBroken;
}

void CPGraph::resetBroken() {
	isBroken = false;
}
