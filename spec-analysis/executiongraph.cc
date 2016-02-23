#include <algorithm>
#include "executiongraph.h"
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
#include "time.h"

/********************    PotentialOP    ********************/
PotentialOP::PotentialOP(ModelAction *op, CSTR label) :
	operation(op),
	label(label)
{

}


/********************    StateFunctionRecord    ********************/
StateFunctionRecord::StateFunctionRecord(NamedFunction *func, Method m1,
	Method m2) : name(func->name), type(func->type), m1(m1), m2(m2) { }

void StateFunctionRecord::print() {
	switch (type) {
		case INITIAL:
			model_print("@Initial %s on ", name);
			m1->print(false, false);
			break;
		case COPY:
			model_print("@Copy %s from ", name);
			m2->print(false, false);
			model_print(" to ");
			m1->print(false, false);
			break;
		case FINAL:
			model_print("@Final %s on ", name);
			m1->print(false, false);
			break;
		case TRANSITION:
			model_print("@Transition %s on ", name);
			m2->print(false, false);
			model_print(", target ->");
			m1->print(false, false);
			break;
		case PRE_CONDITION:
			model_print("@PreCondition %s on ", name);
			m1->print(false, false);
			break;
		case SIDE_EFFECT:
			model_print("@SideEffect %s on ", name);
			m1->print(false, false);
			break;
		case POST_CONDITION:
			model_print("@PostCondition %s on ", name);
			m1->print(false, false);
			break;
		default:
			model_print("Unknown CheckFunctionTyep (never should be here!!\n");
			ASSERT(false);
			break;
	}
	model_print("\n");
}


/********************    HistoryRecordItem     ********************/
HistoryRecordItem::HistoryRecordItem(Method m) : method(m), recordList(new
	FunctionRecordList) { }

void HistoryRecordItem::addFunctionRecord(StateFunctionRecord *r) {
	recordList->push_back(r);
}


/********************    ExecutionGraph    ********************/
/*********    ExecutionGraph (public functions)   **********/
/**
	Initialze the necessary fields 
*/
ExecutionGraph::ExecutionGraph(ModelExecution *e) {
	execution = e;
	methodList = new MethodList;
	randomHistory = NULL;

	broken = false;
	noOrderingPoint = false;
	cyclic = false;
	threadLists = new SnapVector<action_list_t*>;
}

void ExecutionGraph::buildGraph(action_list_t *actions) {
	buildThreadLists(actions);
	
	// First process the initialization annotation
	bool hasInitAnno = false;
	action_list_t::iterator iter = actions->begin();
	for (; iter != actions->end(); iter++) {
		ModelAction *act = *iter;
		SpecAnnotation *anno = getAnnotation(act);
		if (!anno)
			continue;
		// Deal with the Initialization annotation
		if (anno->type == INIT) {
			hasInitAnno = true;
			AnnoInit *annoInit = (AnnoInit*) anno->annotation;
			processInitAnnotation(annoInit);
			break;
		}
	}
	if (!hasInitAnno) {
		model_print("There is no initialization annotation\n");
		broken = true;
		return;
	}

	// Process the nodes for method calls of each thread
	buildNodesFromThreads();
	// Establish the edges
	buildEdges();
}

bool ExecutionGraph::isBroken() {
	return broken;
}

bool ExecutionGraph::isNoOrderingPoint() {
	return noOrderingPoint;
}

bool ExecutionGraph::hasCycle() {
	if (cyclic)
		return true;
	if (randomHistory)
		return false;
	randomHistory = generateOneHistory();
	return cyclic;
}

void ExecutionGraph::resetBroken() {
	broken = false;
}

bool ExecutionGraph::checkAdmissibility() {
	MethodList::iterator iter1, iter2;
	bool admissible = true;
	for (iter1 = methodList->begin(); iter1 != methodList->end(); iter1++) {
		Method m1 = *iter1;
		iter1++;
		iter2 = iter1;
		iter1--;
		for (; iter2 != methodList->end(); iter2++) {
			Method m2 = *iter2;
			if (isReachable(m1, m2) || isReachable(m2, m1))
				continue;
			
			/* Now we need to check whether we have a commutativity rule */
			bool commute = false;
			for (int i = 0; i < commuteRuleNum; i++) {
				CommutativityRule rule = *(commuteRules + i);
				bool satisfied = false;
				/* Check whether condition is satisfied */
				if (!rule.isRightRule(m1, m2)) // Not this rule
					continue;
				else
					satisfied = rule.checkCondition(m1, m2);
				if (satisfied) { // The rule allows commutativity
					commute = true;
					break;
				}
			}
			// We have no rules at all or the condition of the rules is not satisfied
			if (!commute) {
				admissible = false;
				model_print("These two nodes should not commute:\n");
				model_print("\t");
				ModelAction *begin1 = m1->begin;
				ModelAction *begin2 = m2->begin;
				int tid1 = id_to_int(begin1->get_tid());
				int tid2 = id_to_int(begin2->get_tid());
				model_print("%s_%d (T%d)", m1->name,
					begin1->get_seq_number(), tid1);
				model_print(" <-> ");
				model_print("%s_%d (T%d)", m2->name,
					begin2->get_seq_number(), tid2);
				model_print("\n");
			}
		}
	}

	return admissible;
}


/**
	To generate one random topological sorting
*/
MethodList* ExecutionGraph::generateOneHistory() {
	if (cyclic)
		return NULL;
	if (randomHistory) {
		return randomHistory;
	}

	MethodListVector *results = new MethodListVector;
	MethodList *curList = new MethodList;
	int numLiveNodes = methodList->size();
	bool generateOne = true;
	bool found = false;
	generateHistoriesHelper(results, curList, numLiveNodes, generateOne, found);
	
	if (results->size() > 0)
		return (*results)[0];
	else
		return NULL; // To indicate there is a cycle
}

/**
	To generate all possible histories 
*/
MethodListVector* ExecutionGraph::generateAllHistories() {
	MethodListVector *results = new MethodListVector ;
	MethodList *curList = new MethodList;
	int numLiveNodes = methodList->size();
	bool generateOne = false;
	bool found = false;
	generateHistoriesHelper(results, curList, numLiveNodes, generateOne, found);
	
	return results;
}

bool ExecutionGraph::checkOneHistory(MethodList *history, bool verbose) {
	return checkStateSpec(history, verbose);
}

bool ExecutionGraph::checkAllHistories(MethodListVector *histories, bool verbose) {
	for (unsigned i = 0; i < histories->size(); i++) {
		MethodList *history = (*histories)[i];
		if (!checkOneHistory(history, verbose))
			return false;
	}
	return true;
}


/********** Several public printing functions (ExecutionGraph) **********/

void ExecutionGraph::printOneHistory(MethodList *history, CSTR header) {
	model_print("-------------    %s (exec #%d)   -------------\n", header,
		execution->get_execution_number());
	int idx = 1;
	for (MethodList::iterator it = history->begin(); it != history->end(); it++) {
		Method m = *it;
		model_print("%d. ", idx++);
		m->print(false);
	}
	model_print("-------------    %s (exec #%d) (end)    -------------\n",
		header, execution->get_execution_number());
}

void ExecutionGraph::printAllHistories(MethodListVector *histories) {
	model_print("-------------    All histories (exec #%d)    -------------\n",
		execution->get_execution_number());
	for (unsigned i = 0; i < histories->size(); i++) {
		model_print("***********************    # %d    ***********************\n", i + 1);
		MethodList *history = (*histories)[i];
		int idx = 1;
		for (MethodList::iterator it = history->begin(); it != history->end();
			it++) {
			Method m = *it;
			model_print("%d. ", idx++);
			m->print(false);
		}
		if (i != histories->size() - 1)
			model_print("\n");
	}
	model_print("-------------    All histories (exec #%d) (end) "
		"-------------\n\n", execution->get_execution_number());
}

/**
	By default we print only all the edges that are directly from this mehtod
	call node. If passed allEdges == true, we will print all the edges that are
	reachable (SC/hb after) the current node.
*/
void ExecutionGraph::print(bool allEdges) {
	model_print("\n");
	const char *extraStr = allEdges ? "All Edges" : "";
	model_print("------------------  Execution Graph -- %s (exec #%d)"
	"  ------------------\n", extraStr, execution->get_execution_number());
	for (MethodList::iterator iter = methodList->begin(); iter !=
		methodList->end(); iter++) {
		Method m = *iter;
		/* Print the info the this method */
		m->print(false);
		/* Print the info the edges directly from this node */
		SnapSet<Method> *theSet = allEdges ? m->allNext : m->next;
		for (SnapSet<Method>::iterator nextIter = theSet->begin(); nextIter !=
			theSet->end(); nextIter++) {
			Method next = *nextIter;
			model_print("\t--> ");
			next->print(false);
		}
	}
	model_print("------------------  End Graph (exec #%d)"
		"  ------------------\n", execution->get_execution_number());
	model_print("\n");
}

/**
	By default we print only all the edges that are directly to this mehtod
	call node. If passed allEdges == true, we will print all the edges that are
	reachable (SC/hb before) from the current node.
	
	Only for debugging!!
*/
void ExecutionGraph::PrintReverse(bool allEdges) {
	model_print("\n");
	const char *extraStr = allEdges ? "All Edges" : "";
	model_print("------------------  Reverse Execution Graph -- %s (exec #%d)"
	"  ------------------\n", extraStr, execution->get_execution_number());
	for (MethodList::iterator iter = methodList->begin(); iter !=
		methodList->end(); iter++) {
		Method m = *iter;
		/* Print the info the this method */
		m->print(false);
		/* Print the info the edges directly from this node */
		SnapSet<Method> *theSet = allEdges ? m->allPrev : m->prev;
		for (SnapSet<Method>::iterator prevIter = theSet->begin(); prevIter !=
			theSet->end(); prevIter++) {
			Method prev = *prevIter;
			model_print("\t--> ");
			prev->print(false);
		}
	}
	model_print("------------------  End Reverse Graph (exec #%d)"
		"  ------------------\n", execution->get_execution_number());
	model_print("\n");
}


/********** Internal member functions (ExecutionGraph) **********/

void ExecutionGraph::buildThreadLists(action_list_t *actions) {
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

/**
	Outside of this function, we have already processed the INIT type of
	annotation, and we focus on extracting all the method call nodes in the
	current thread list. This routine basically iterates the list, finds the
	INTERFACE_BEGIN annotation, call the function extractMethod(), and advance
	the iterator. That is to say, we would only see INIT or INTERFACE_BEGIN
	annotations; if not, the specification annotations are wrong (we mark the
	graph is broken)
*/
void ExecutionGraph::buildNodesFromThread(action_list_t *actions) {
	action_list_t::iterator iter = actions->begin();

	// FIXME: Just for the purpose of debugging
	//printActions(actions, "BuildNodesFromThread");
	
	// annoBegin == NULL means we are looking for the beginning annotation
	while (iter != actions->end()) {
		ModelAction *act = *iter;
		SpecAnnotation *anno = getAnnotation(act);
		if (!anno) { // Means this is not an annotation action
			iter++;
			continue;
		}
		if (anno->type == INTERFACE_BEGIN) { // Interface beginning anno
			Method m = extractMethod(actions, iter);
			if (m) {
				// Get a complete method call node and store it
				methodList->push_back(m);
			} else {
				broken = true;
				model_print("Error with constructing a complete node.\n");
				return;
			}
		} else if (anno->type != INIT) {
			broken = true;
			model_print("Missing beginning annotation.\n");
			return;
		} else { // This is an INIT annotation
			iter++;
		}
	}
}

void ExecutionGraph::buildNodesFromThreads() {
	/* We start from the 1st real thread */
	for (unsigned i = 1; i < threadLists->size(); i++) {
		buildNodesFromThread((*threadLists)[i]);
		if (broken) // Early exit when detecting errors
			return;
	}
}

/**
    Find the previous non-annotation model action (ordering point from the
	current iterator
*/
ModelAction* ExecutionGraph::findPrevAction(action_list_t *actions, action_list_t::iterator 
        iter) {
	while (iter != actions->begin()) {
		iter--;
		ModelAction *res = *iter;
		if (res->get_type() != ATOMIC_ANNOTATION)
			return res;
	}
	return NULL;
}

/** 
	When called, the current iter points to a beginning annotation; when done,
	the iter points to either the end of the list or the next INTERFACE_BEGIN
	annotation. 
*/
Method ExecutionGraph::extractMethod(action_list_t *actions, action_list_t::iterator &iter) {
	ModelAction *act = *iter;
	SpecAnnotation *anno = getAnnotation(act);
	ASSERT(anno && anno->type == INTERFACE_BEGIN);

	// Partially initialize the commit point node with the already known fields
	AnnoInterfaceInfo *info = (AnnoInterfaceInfo*) anno->annotation;
	Method m = new MethodCall(info->name, info->value, act);

	// Some declaration for potential ordering points and its check
	CSTR label;
	PotentialOP *potentialOP= NULL;
	// A list of potential ordering points
	PotentialOPList *popList = new PotentialOPList;
	// Ordering point operation
	ModelAction *op = NULL;
	// Whether the potential ordering points were defined
	bool hasAppeared = false;
	
	bool methodComplete = false;
	for (iter++; iter != actions->end(); iter++) {
		act = *iter;
		SpecAnnotation *anno = getAnnotation(act);
		if (!anno)
			continue;
		// Now we are dealing with one annotation
		switch (anno->type) {
			case POTENTIAL_OP:
				//model_print("POTENTIAL_OP\n");
				label = (CSTR) anno->annotation;
				op = findPrevAction(actions, iter);
				if (!op) {
					model_print("Potential ordering point annotation should"
						"follow an atomic operation.\n");
					model_print("%s_%d\n", label,
						act->get_seq_number());
					broken = true;
					return NULL;
				}
				potentialOP = new PotentialOP(op, label);
				popList->push_back(potentialOP);
				break;
			case OP_CHECK:
				//model_print("OP_CHECK\n");
				label = (CSTR) anno->annotation;
				// Check if corresponding potential ordering point has appeared.
				hasAppeared = false;
				// However, in the current version of spec, we take the most
				// recent one in the list as the commit point (so we use
				// reverse iterator)
				for (PotentialOPList::reverse_iterator popIter = popList->rbegin();
					popIter != popList->rend(); popIter++) {
					potentialOP = *popIter;
					if (label == potentialOP->label) {
						m->addOrderingPoint(potentialOP->operation);
						hasAppeared = true;
						break; // Done when find the "first" PCP
					}
				}
				if (!hasAppeared) {
					model_print("Ordering point check annotation should"
						"have previous potential ordering point.\n");
					model_print("%s_%d\n", label,
						act->get_seq_number());
					broken = true;
					return NULL;
				}
				break;
			case OP_DEFINE:
				//model_print("CP_DEFINE_CHECK\n");
				op = findPrevAction(actions, iter);
				if (!op) {
					model_print("Ordering point define should "
						"follow an atomic operation.\n");
					act->print();
					broken = true;
					return NULL;
				}
				m->addOrderingPoint(op);
				break;
			case OP_CLEAR: 
				//model_print("OP_CLEAR\n");
				// Reset everything
				// Clear the list of potential ordering points
				popList->clear();
				// Clear the previous list of commit points
				m->orderingPoints->clear();
				break;
			case OP_CLEAR_DEFINE: 
				//model_print("OP_CLEAR_DEFINE\n");
				// Reset everything
				popList->clear();
				m->orderingPoints->clear();
				// Define the ordering point
				op = findPrevAction(actions, iter);
				if (!op) {
					model_print("Ordering point clear define should "
						"follow an atomic operation.\n");
					act->print();
					broken = true;
					return NULL;
				}
				m->addOrderingPoint(op);
				break;
			case INTERFACE_BEGIN:
				methodComplete = true;		
				break;
			default:
				model_print("Unknown type!! We should never get here.\n");
				ASSERT(false);
				return NULL;
		}
		if (methodComplete) // Get out of the loop when we have a complete node
			break;
	}

	ASSERT (iter == actions->end() || (getAnnotation(*iter) &&
		getAnnotation(*iter)->type == INTERFACE_BEGIN));

	delete popList;
	// XXX: We just allow methods to have no specified ordering points. In that
	// case, the method is concurrent with every other method call
	
	if (m->orderingPoints->size() == 0) {
		noOrderingPoint = true;
		/*
		model_print("There is no ordering points for method %s.\n",
			m->name);
		m->begin->print();
		broken = true;
		return NULL;
		*/
		return m;
	} else {
		// Get a complete method call
		return m;
	}
}

/** 
	A utility function to extract the actual annotation
	pointer and return the actual annotation if this is an annotation action;
	otherwise return NULL.
*/
SpecAnnotation* ExecutionGraph::getAnnotation(ModelAction *act) {
	if (act->get_type() != ATOMIC_ANNOTATION)
		return NULL;
	if (act->get_value() != SPEC_ANALYSIS)
		return NULL;
	SpecAnnotation *anno = (SpecAnnotation*) act->get_location();
	ASSERT (anno);
	return anno;
}

void ExecutionGraph::processInitAnnotation(AnnoInit *annoInit) {
	// Assign state initial (and copy) function
	NamedFunction *func = annoInit->initial;
	ASSERT (func && func->type == INITIAL);
	initial= annoInit->initial;

	func = annoInit->final;
	ASSERT (func && func->type == FINAL);
	final= annoInit->final;

	func = annoInit->copy;
	ASSERT (func && func->type == COPY);
	copy= annoInit->copy;

	func = annoInit->printState;
	ASSERT (func && func->type == PRINT_STATE);
	printState = annoInit->printState;

	// Assign the function map (from interface name to state functions)
	funcMap = annoInit->funcMap;

	// Initialize the commutativity rules array and size
	commuteRules = annoInit->commuteRules;
	commuteRuleNum = annoInit->commuteRuleNum; 
}


/**
	This is a very important interal function to build the graph. When called,
	we assume that we have a list of method nodes built (extracted from the raw
	execution), and this routine is responsible for building the connection
	edges between them to yield an execution graph for checking
*/
void ExecutionGraph::buildEdges() {
	// Add two special nodes (START & FINISH) at the beginning & end of methodList
	Method startMethod = new MethodCall(GRAPH_START);
	Method finishMethod = new MethodCall(GRAPH_FINISH);
	
	methodList->push_front(startMethod);
	methodList->push_back(finishMethod);

	MethodList::iterator iter1, iter2;
	// First build the allPrev and allNext set (don't care if they are right
	// previous or right next first)
	for (iter1 = methodList->begin(); iter1 != methodList->end(); iter1++) {
		Method m1 = *iter1;
		iter1++;
		iter2 = iter1;
		iter1--;
		for (; iter2 != methodList->end(); iter2++) {
			Method m2 = *iter2;
			int val = conflict(m1, m2);
			if (val == 1) {
				m1->allNext->insert(m2);
				m2->allPrev->insert(m1);
			} else if (val == -1) {
				m2->allNext->insert(m1);
				m1->allPrev->insert(m2);
			}
		}
	}
	
	// Now build the prev, next and concurrent sets
	for (MethodList::iterator iter = methodList->begin(); iter != methodList->end();
		iter++) {
		Method m = *iter;
		// prev -- nodes in allPrev that are before none in allPrev
		// next -- nodes in allNext that are after none in allNext (but we
		// actually build this set together with building prev
		SnapSet<Method>::iterator setIt;
		for (setIt = m->allPrev->begin(); setIt != m->allPrev->end(); setIt++) {
			Method prevMethod = *setIt;
			if (MethodCall::disjoint(m->allPrev, prevMethod->allNext)) {
				m->prev->insert(prevMethod);
				prevMethod->next->insert(m);
			}
		}
		
		// concurrent -- all other nodes besides MYSELF, allPrev and allNext
		for (MethodList::iterator concurIter = methodList->begin(); concurIter !=
			methodList->end(); concurIter++) {
			Method concur = *concurIter;
			if (concur != m && !MethodCall::belong(m->allPrev, concur)
				&& !MethodCall::belong(m->allNext, concur))
				m->concurrent->insert(concur);
		}
	}

	AssertEdges();

	//model_print("Right after calling buildEdges\n");
	//print(false);
	//PrintReverse(false);
}

/**
	This method call is used to check whether the edge sets of the nodes are
	built correctly --- consistently. We should only use this routine after the
	builiding of edges when debugging
*/
void ExecutionGraph::AssertEdges() {
	// Assume there is no self-cycle in execution (ordering points are fine)
	ASSERT (!cyclic);

	MethodList::iterator it;
	for (it = methodList->begin(); it != methodList->end(); it++) {
		Method m = *it;
		SnapSet<Method>::iterator setIt, setIt1;
		int val = 0;

		// Soundness of sets
		// 1. allPrev is sound
		for (setIt = m->allPrev->begin(); setIt != m->allPrev->end(); setIt++) {
			Method prevMethod = *setIt;
			val = conflict(prevMethod, m);
			ASSERT (val == 1);
		}
		// 2. allNext is sound
		for (setIt = m->allNext->begin(); setIt != m->allNext->end(); setIt++) {
			Method nextMethod = *setIt;
			val = conflict(m, nextMethod);
			ASSERT (val == 1);
		}
		// 3. concurrent is sound
		for (setIt = m->concurrent->begin(); setIt != m->concurrent->end(); setIt++) {
			Method concur = *setIt;
			val = conflict(m, concur);
			ASSERT (val == 0);
		}
		// 4. allPrev & allNext are complete
		ASSERT (1 + m->allPrev->size() + m->allNext->size() + m->concurrent->size()
			== methodList->size());
		// 5. prev is sound
		for (setIt = m->prev->begin(); setIt != m->prev->end(); setIt++) {
			Method prev = *setIt;
			ASSERT (MethodCall::belong(m->allPrev, prev));
			for (setIt1 = m->allPrev->begin(); setIt1 != m->allPrev->end();
				setIt1++) {
				Method middle = *setIt1;
				if (middle == prev)
					continue;
				val = conflict(prev, middle);
				// prev is before none
				ASSERT (val != 1);
			}
		}
		// 6. prev is complete 
		for (setIt = m->allPrev->begin(); setIt != m->allPrev->end(); setIt++) {
			Method prev = *setIt;
			if (MethodCall::belong(m->prev, prev))
				continue;
			// Ensure none of other previous nodes should be in the prev set
			bool hasMiddle = false;
			for (setIt1 = m->allPrev->begin(); setIt1 != m->allPrev->end();
				setIt1++) {
				Method middle = *setIt1;
				if (middle == prev)
					continue;
				val = conflict(prev, middle);
				if (val == 1)
					hasMiddle = true;
			}
			ASSERT (hasMiddle);
		}

		// 7. next is sound
		for (setIt = m->next->begin(); setIt != m->next->end(); setIt++) {
			Method next = *setIt;
			ASSERT (MethodCall::belong(m->allNext, next));
			for (setIt1 = m->allNext->begin(); setIt1 != m->allNext->end();
				setIt1++) {
				Method middle = *setIt1;
				if (middle == next)
					continue;
				val = conflict(middle, next);
				// next is after none
				ASSERT (val != 1);
			}
		}
		// 8. next is complete 
		for (setIt = m->allNext->begin(); setIt != m->allNext->end(); setIt++) {
			Method next = *setIt;
			if (MethodCall::belong(m->next, next))
				continue;
			// Ensure none of other next nodes should be in the next set
			bool hasMiddle = false;
			for (setIt1 = m->allNext->begin(); setIt1 != m->allNext->end();
				setIt1++) {
				Method middle = *setIt1;
				if (middle == next)
					continue;
				val = conflict(middle, next);
				if (val == 1)
					hasMiddle = true;
			}
			ASSERT (hasMiddle);
		}
	}
}

/**
	The conflicting relation between two model actions by hb/SC. If act1 and
	act2 commutes, it returns 0; Otherwise, if act1->act2, it returns 1; and if
	act2->act1, it returns -1
*/
int ExecutionGraph::conflict(ModelAction *act1, ModelAction *act2) {
	if (act1->happens_before(act2))
		return 1;
	else if (act2->happens_before(act1))
		return -1;
	
	if (act1->is_seqcst() && act2->is_seqcst()) {
		if (act1->get_seq_number() < act2->get_seq_number())
			return 1;
		else
			return -1;
	} else
		return 0;
}

/**
	If there is no conflict between the ordering points of m1 and m2, then it
	returns 0; Otherwise, if m1->m2, it returns 1; and if m2->m1, it returns -1.
	If some ordering points have inconsistent conflicting relation, we print out
	an error message (Self-cycle) and set the broken flag and return
*/
int ExecutionGraph::conflict(Method m1, Method m2) {
	ASSERT (m1 != m2);
	
	if (m1->name == GRAPH_START)
		return true;
	if (m2->name == GRAPH_FINISH)
		return true;

	action_list_t *OPs1= m1->orderingPoints;
	action_list_t *OPs2= m2->orderingPoints;
	// Method calls without ordering points are concurrent with any others
	if (OPs1->empty() || OPs2->empty())
		return 0;
	int val = 0;
	action_list_t::iterator iter1, iter2;
	for (iter1 = OPs1->begin(); iter1 != OPs1->end(); iter1++) {
		ModelAction *act1 = *iter1;
		for (iter2 = OPs2->begin(); iter2 != OPs2->end(); iter2++) {
			ModelAction *act2 = *iter2;
			int res = conflict(act1, act2);
			if (val == 0)
				val = res;
			else if (val != res) { // Self cycle
				cyclic = true;
				model_print("There is a self cycle between the following two "
					"methods\n");
				m1->print();
				m2->print();
				broken = true;
				return UNKNOWN_CONFLICT;
			}
		}
	}
	return val;
}

/**
	Whether m2 is before m2 in the execution graph
*/
bool ExecutionGraph::isReachable(Method m1, Method m2) {
	return MethodCall::belong(m1->allNext, m2);
}

MethodVector* ExecutionGraph::getRootNodes() {
	MethodVector *vec = new MethodVector;
	for (MethodList::iterator it = methodList->begin(); it != methodList->end();
		it++) {
		Method m = *it;
		if (!m->exist)
			continue;
		MethodSet prevMethods = m->allPrev;
		if (prevMethods->size() == 0) { // Fast check (naturally a root node)
			vec->push_back(m);
			continue;
		}
		
		// Also a root when all previous nodes no longer exist
		bool isRoot = true;
		for (SnapSet<Method>::iterator setIt = prevMethods->begin(); setIt !=
			prevMethods->end(); setIt++) {
			Method prev = *setIt;
			if (prev->exist) { // It does have an incoming edge
				isRoot = false;
				break;
			}
		}
		// It does not have an incoming edge now
		if (isRoot)
			vec->push_back(m);
	}
	return vec;
}

/** 
	Collects the set of method call nodes that do NOT have any following nodes
	(the tail of the graph)
*/
MethodVector* ExecutionGraph::getEndNodes() {
	MethodVector *vec = new MethodVector;
	for (MethodList::iterator it = methodList->begin(); it != methodList->end();
		it++) {
		Method m = *it;
		if (m->next->size() == 0)
			vec->push_back(m);
	}
	return vec;
}

/**
	This is a helper function for generating all the topological sortings of the
	execution graph. The idea of this function is recursively do the following
	process: logically mark whether a method call node is alive or not
	(initially all are alive), find a list of nodes that are root nodes (that
	have no incoming edges), continuously pick one node in that list, add it to the curList
	(which stores one topological sorting), and then recursively call itself to
	resolve the rest.
	
	Arguments:
	results -> all the generated topological sortings (each as a list) will be
		added in this vector
	curList -> it represents a temporal result of a specific topological
		sorting; keep in mind that before calling this function, pass an empty list
	numLiveNodes -> the number of nodes that are logically alive (that have not
		been selected and added in a specific topological sorting yet). We keep
		such a number as an optimization since when numLinveNodes equals to 0,
		we can backtrack. Initially it is the size of the method call list.
	generateOne -> we incorporate the routine of generating one random
		topological sorting into this method. When this is true, the whole
		routine will recursively return and end the routine when it finds one
		history, and the result will be stored as the first element in the
		results vector. Otherwise, the routine will run until it collects the
		whole set of all possible histories, and the whole results can be found
		in the results vector.
	found -> this argument is also used for generating one random history. We
		have this argument for the purpose of fast exit when we find one
		history.
*/
void ExecutionGraph::generateHistoriesHelper(MethodListVector* results, MethodList
	*curList, int &numLiveNodes, bool generateOne, bool &found) {
	if (cyclic)
		return;

	if (generateOne && found) {
		return;
	}
	
	if (numLiveNodes == 0) { // Found one sorting, and we can backtrack
		found = true;
		results->push_back(new MethodList(*curList));
		return;
	}

	MethodVector *roots = getRootNodes();
	// Cycle exists (no root nodes but still have live nodes
	if (roots->size() == 0) {
		model_print("There is a cycle in this graph so we cannot generate"
			" sequential histories\n");
		cyclic = true;
		return;
	}

	for (unsigned i = 0; i < roots->size(); i++) {
		Method m;
		if (generateOne) { // Just randomly pick one
			srand (time(NULL));
			int pick = rand() % roots->size();
			m = (*roots)[pick];
		} else {
			m = (*roots)[i];
		}
		m->exist = false;
		numLiveNodes--;
		// Copy the whole current list and use that copy for the next recursive
		// call (because we will need the same copy for other iterations at the
		// same level of recursive calls)
		MethodList *newList = new MethodList(*curList);
		newList->push_back(m);

		generateHistoriesHelper(results, newList, numLiveNodes, generateOne,
			found);

		// Recover
		m->exist = true;
		numLiveNodes++;
		delete newList;
		
		// Early exit for the case of just generating one random sorting
		if (generateOne) 
			break;
	}
	delete roots;
}

Method ExecutionGraph::getStartMethod() {
	return methodList->front();
}

Method ExecutionGraph::getFinishMethod() {
	return methodList->back();
}

bool ExecutionGraph::isFakeMethod(Method m) {
	return m->name == GRAPH_START || m->name == GRAPH_FINISH;
}


void ExecutionGraph::printHistoryRecord(HistoryRecord *records) {
	for (HistoryRecord::iterator it = records->begin(); it != records->end();
		it++) {
		HistoryRecordItem *item = *it;
		Method m = item->method;
		FunctionRecordList *funcList = item->recordList;
		m->print(false, false);
		model_print(":\n");
		if (!isFakeMethod(m)) {
			StateFunctions *funcs = funcMap->get(m->name);
			ASSERT (funcs);
			UpdateState_t print = (UpdateState_t) funcs->print->function;
			if (print) {
				model_print("**********  dynamic info  **********\n");
				(*print)(m);
				model_print("**********  dynamic info  **********\n");
			}
		}
		for (FunctionRecordList::iterator funcIt = funcList->begin(); funcIt !=
			funcList->end(); funcIt++) {
			StateFunctionRecord *func = *funcIt;
			model_print("   ");
			func->print();
		}
	}
}

/**
	Checking the state specification (in sequential order)
*/
bool ExecutionGraph::checkStateSpec(MethodList *history, bool verbose) {
	if (verbose)
		model_print("---- Start to check on state specification ----\n");

	// Record the histroy execution
	HistoryRecord *historyRecord = new HistoryRecord;
	HistoryRecordItem *recordItem = NULL;

	// @Initial state (with a fake starting node)
	Method startMethod = getStartMethod();
	UpdateState_t initialFunc = (UpdateState_t) initial->function;
	UpdateState_t printStateFunc = (UpdateState_t) printState->function;

	(*initialFunc)(startMethod);
	if (verbose) {
		startMethod->print(false, false);
		model_print("\t@Initial on START\n");
		if (printStateFunc) { // If user has defined the print-out function
			model_print("\t**********  State Info  **********\n");
			(*printStateFunc)(startMethod);
		}
	}
	
	// Record @Initial
	// Create a record item for the startMethod
	recordItem = new HistoryRecordItem(startMethod);
	// Add the @Initial call to the record item
	recordItem->addFunctionRecord(
		new StateFunctionRecord(initial, startMethod, NULL));
	// Add this record to the history record
	historyRecord->push_back(recordItem);

	for (MethodList::iterator it = history->begin(); it != history->end();
		it++) {
		Method m = *it;
		if (isFakeMethod(m))
			continue;

		// Create a record item for method m 
		recordItem = new HistoryRecordItem(m);
		// Add this record to the history record
		historyRecord->push_back(recordItem);
	
		StateFunctions *funcs = NULL;
		StateTransition_t transition = NULL;
		
		if (verbose) {
			m->print(false, false);
			model_print("\n");
			
			funcs = funcMap->get(m->name);
			ASSERT (funcs);
			UpdateState_t printValue = (UpdateState_t) funcs->print->function;
			if (printValue) {
				model_print("\t**********  Value Info  **********\n");
				(*printValue)(m);
			}	
		}

		// Execute a list of transitions till Method m
		// Optimization: find the previous justified nodes --- a node that has no
		// concurrent method calls (everyone is either before or after me).
		MethodList::iterator justifiedIter = it;
		while (justifiedIter != history->begin()) {
			justifiedIter--;
			Method justified = *justifiedIter;
			if (justified->concurrent->size() == 0 &&
				MethodCall::belong(m->allPrev, justified)) {
				// @Copy function from Method justified to Method m
				CopyState_t copyFunc = (CopyState_t) copy->function;
				(*copyFunc)(m, justified);

				if (verbose) {
					model_print("\t@Copy from justified node: ");
					justified->print(false, false);
					model_print("\n");
					if (printStateFunc) {
						model_print("\t**********  State Info  **********\n");
						(*printStateFunc)(m);
					}
				}

				// Record @Copy
				// Add the @Copy call to the record item
				recordItem->addFunctionRecord(
					new StateFunctionRecord(copy, m, justified));
				break;
			}
		}

		// Now m has the copied state of the most recent justified node, and
		// justifiedIter points to that justified node
		for (MethodList::iterator execIter = ++justifiedIter; execIter != it;
			execIter++) {
			Method exec = *execIter;
			if (MethodCall::belong(m->allPrev, exec)) {
				ASSERT (!isFakeMethod(exec));
				funcs = funcMap->get(exec->name);
				ASSERT (funcs);
				transition = (StateTransition_t)
					funcs->transition->function;
				// @Transition on the state of Method m with Method exec
				(*transition)(m, exec);
	
				if (verbose) {
					model_print("\t@Transition on ");
					exec->print(false, false);
					model_print("\n");

					funcs = funcMap->get(exec->name);
					ASSERT (funcs);
					UpdateState_t printValue = (UpdateState_t) funcs->print->function;
					if (printValue) {
						model_print("\t**********  Value Info  **********\n");
						(*printValue)(exec);
					}
					if (printStateFunc) {
						model_print("\t**********  State Info  **********\n");
						(*printStateFunc)(m);
					}
				}

				// Add the @Transition call to the record item
				recordItem->addFunctionRecord(
					new StateFunctionRecord(funcs->transition, m, exec));

			}
		}

		funcs = funcMap->get(m->name);
		ASSERT (funcs);
		CheckState_t preCondition = (CheckState_t)
			funcs->preCondition->function;
		//UpdateState_t sideEffect = (UpdateState_t) funcs->sideEffect->function;
		//CheckState_t postCondition = (CheckState_t)
		//	CheckState_t funcs->postCondition->function;

		bool satisfied;
		// @PreCondition of Mehtod m
		if (preCondition) {
			satisfied = (*preCondition)(m);
			
			// Add the @PreCondition call to the record item
			recordItem->addFunctionRecord(
				new StateFunctionRecord(funcs->preCondition, m, NULL));

			if (!satisfied) {
				model_print("PreCondition is not satisfied. Problematic method"
					" is as follow: \n");
				m->print(true, true);
				model_print("Problmatic execution graph: \n");
				print();
				printOneHistory(history, "Problematic Seqneutial History");
				model_print("Problematic history execution record: \n");
				//printHistoryRecord(historyRecord);
				if (verbose)
					model_print("---- Check on state specification ends"
						" ----\n\n");
				return false;
			}
		}

		// After checking the PreCondition, we also run the transition on the
		// current node to update its state
		funcs = funcMap->get(m->name);
		ASSERT (funcs);
		transition = (StateTransition_t) funcs->transition->function;
		// @Transition on the state of Method m by itself
		(*transition)(m, m);

		if (verbose) {
			model_print("\t@Transition on myself\n");
			if (printStateFunc) {
				model_print("\t**********  State Info  **********\n");
				(*printStateFunc)(m);
			}
		}

		// Add the @Transition call to the record item
		recordItem->addFunctionRecord(
			new StateFunctionRecord(funcs->transition, m, m));
		
		// @SideEffect of Method m
		//if (sideEffect)
		//	(*sideEffect)(m);
		// @PostCondition of Method m
		/*
		if (postCondition) {
			satisfied = (*postCondition)(m);

			// Add the @PostCondition call to the record item
			recordItem->addFunctionRecord(
				new StateFunctionRecord(funcs->postCondition, m, NULL));

			if (!satisfied) {
				model_print("PostCondition is not satisfied (Problematic method call"
				 " printed");
				m->print(true, true);
				model_print("Problmatic execution graph: \n");
				print();
				printOneHistory(history, "Problematic Seqneutial History");
				model_print("Problematic history execution record: \n");
				printHistoryRecord(historyRecord);

				return false;
			}
		}
		*/
	}

	if (verbose)
		model_print("---- Check on state specification ends ----\n\n");
	return true;
}


/**
	To take a list of actions and print it out in a nicer way
*/
void ExecutionGraph::printActions(action_list_t *actions, const char *header) {
	model_print("%s\n", header);
	model_print("---------- Thread List (Begin) ---------\n");
	for (action_list_t::iterator it = actions->begin(); it != actions->end();
		it++) {
		ModelAction *act = *it;
		SpecAnnotation *anno = getAnnotation(act);
		if (anno) {
			model_print("%s -> ", specAnnoType2Str(anno->type));
		}
		act->print();
	}
	model_print("---------- Thread List (End) ---------\n");
}
