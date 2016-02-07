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
PotentialOP::PotentialOP(ModelAction *op, string label) :
	operation(op),
	label(label)
{

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
				model_print("%s_%d (T%d)", m1->name.c_str(),
					begin1->get_seq_number(), tid1);
				model_print(" <-> ");
				model_print("%s_%d (T%d)", m2->name.c_str(),
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

bool ExecutionGraph::checkOneHistory(MethodList *history) {
	return checkStateSpec(history);
}

bool ExecutionGraph::checkAllHistories(MethodListVector *histories) {
	for (unsigned i = 0; i < histories->size(); i++) {
		MethodList *history = (*histories)[i];
		if (!checkOneHistory(history))
			return false;
	}
	return true;
}


/********** Several public printing functions (ExecutionGraph) **********/

void ExecutionGraph::printOneHistory(MethodList *history) {
	model_print("-------------    A random history    -------------\n");
	int idx = 1;
	for (MethodList::iterator it = history->begin(); it != history->end(); it++) {
		Method m = *it;
		model_print("%d. ", idx++);
		m->print();
	}
	model_print("-------------    A random history (end)    -------------\n");
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
			m->print();
		}
		if (i != histories->size() - 1)
			model_print("\n");
	}
	model_print("-------------    All histories (exec #%d) (end) "
		"-------------\n\n", execution->get_execution_number());
}

void ExecutionGraph::print() {
	model_print("\n");
	model_print("------------------  Execution Graph (exec #%d)"
	"  ------------------\n", execution->get_execution_number());
	for (MethodList::iterator iter = methodList->begin(); iter !=
		methodList->end(); iter++) {
		Method m = *iter;
		/* Print the info the this method */
		m->print();
		model_print("\n");
		/* Print the info the edges directly from this node */
		for (SnapSet<Method>::iterator nextIter = m->next->begin(); nextIter !=
			m->next->end(); nextIter++) {
			Method next = *nextIter;
			model_print("\t\t--> ");
			next->print();
		}
	}
	model_print("------------------  End Graph (exec #%d)"
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
	MODEL_ASSERT(anno && anno->type == INTERFACE_BEGIN);

	// Partially initialize the commit point node with the already known fields
	//FIXME: Seems like the SNAPSHOT new would not call non-default constructor?
	AnnoInterfaceInfo *info = (AnnoInterfaceInfo*) anno->annotation;
	Method m = new MethodCall(info->name, info->value, act);

	// Some declaration for potential ordering points and its check
	string *labelPtr;
	PotentialOP *potentialOP= NULL;
	// A list of potential ordering points
	PotentialOPList *popList = new PotentialOPList;
	// Ordering point operation
	ModelAction *op = NULL;
	// Whether the potential ordering points were defined
	bool hasAppeared = false;

	for (iter++; iter != actions->end(); iter++) {
		act = *iter;
		SpecAnnotation *anno = getAnnotation(act);
		if (!anno)
			continue;
		// Now we are dealing with one annotation
		switch (anno->type) {
			case POTENTIAL_OP:
				//model_print("POTENTIAL_OP\n");
				labelPtr = (string*) anno->annotation;
				op = findPrevAction(actions, iter);
				if (!op) {
					model_print("Potential ordering point annotation should"
						"follow an atomic operation.\n");
					model_print("%s_%d\n", labelPtr->c_str(),
						act->get_seq_number());
					broken = true;
					return NULL;
				}
				potentialOP = new PotentialOP(op, *labelPtr);
				popList->push_back(potentialOP);
				break;
			case OP_CHECK:
				//model_print("OP_CHECK\n");
				labelPtr = (string*) anno->annotation;
				// Check if corresponding potential ordering point has appeared.
				hasAppeared = false;
				// However, in the current version of spec, we take the most
				// recent one in the list as the commit point (so we use
				// reverse iterator)
				for (PotentialOPList::reverse_iterator popIter = popList->rbegin();
					popIter != popList->rend(); popIter++) {
					potentialOP = *popIter;
					if (*labelPtr == potentialOP->label) {
						m->addOrderingPoint(potentialOP->operation);
						hasAppeared = true;
						break; // Done when find the "first" PCP
					}
				}
				if (!hasAppeared) {
					model_print("Ordering point check annotation should"
						"have previous potential ordering point.\n");
					model_print("%s_%d\n", labelPtr->c_str(),
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
				break;
			default:
				model_print("We should not get here.\n");
				//MODEL_ASSERT(false);
				return NULL;
		}
	}
	delete popList;
	if (m->orderingPoints->size() == 0) {
		model_print("There is no ordering points for method %s.\n",
			m->name.c_str());
		m->begin->print();
		broken = true;
		return NULL;
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
		return NULL; SpecAnnotation *anno = (SpecAnnotation*) act->get_location();
	MODEL_ASSERT (anno);
	return anno;
}

void ExecutionGraph::processInitAnnotation(AnnoInit *annoInit) {
	// Assign state initial (and copy) function 
	initial= annoInit->initial;
	final= annoInit->final;
	copy= annoInit->copy;
	MODEL_ASSERT(initial);

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
		SnapSet<Method>::iterator setIt;
		for (setIt = m->prev->begin(); setIt != m->prev->end(); setIt++) {
			Method prevMethod = *setIt;
			if (MethodCall::disjoint(m->prev, prevMethod->allNext))
				m->prev->insert(prevMethod);
		}
		// next -- nodes in allNext that are after none in allNext
		for (setIt = m->next->begin(); setIt != m->next->end(); setIt++) {
			Method nextMethod = *setIt;
			if (MethodCall::disjoint(m->next, nextMethod->allPrev))
				m->next->insert(nextMethod);
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
	action_list_t *OPs1= m1->orderingPoints;
	action_list_t *OPs2= m2->orderingPoints;
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
				return 0;
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

/** FIXME */
MethodVector* ExecutionGraph::getEndNodes() {
	MethodVector *vec = new MethodVector;
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



bool ExecutionGraph::checkStateSpec(MethodList *history) {
	// Initialize state (with a fake starting node)
	Method startPoint = new MethodCall("START");
	(*initial)(startPoint);
	// Put the fake starting point in the very beginning of the history
	history->push_front(startPoint);

	for (MethodList::iterator it = history->begin(); it != history->end();
		it++) {
		Method m = *it;
		// Execute @Transition
		// Optimization: find the previous fixed nodes --- a node that has no
		// concurrent method calls (everyone is either before or after me).
		MethodList::iterator fixedIter = it;
		while (fixedIter != history->begin()) {
			fixedIter--;
			Method fixed = *fixedIter;
			if (fixed->concurrent->size() == 0 && (MethodCall::belong(m->allPrev,
				fixed) || fixed->name == "START")) {
				(*copy)(m, fixed);
				break;
			}
		}
		for (MethodList::iterator execIter = fixedIter++; execIter != it;
			execIter++) {
			Method exec = *execIter;
			if (MethodCall::belong(m->allPrev, exec)) {
				StateFunctions *funcs = funcMap->at(exec->name);
				MODEL_ASSERT (funcs);
				StateTransition_t transition = funcs->transition;
				// Execute the transition on the state of Method m
				(*transition)(m, exec);
			}
		}

		// Execute @EvaluateState
		StateFunctions *funcs = funcMap->at(m->name);
		MODEL_ASSERT (funcs);
		UpdateState_t evaluateState = funcs->evaluateState;
		CheckState_t preCondition = funcs->preCondition ;
		UpdateState_t sideEffect = funcs->sideEffect;
		CheckState_t postCondition = funcs->postCondition ;
		// Evaluate the state of Method m
		if (evaluateState)
			(*evaluateState)(m);
		bool satisfied = false;
		// Check PreCondition of Mehtod m
		if (preCondition) {
			satisfied = (*preCondition)(m);
			if (!satisfied) {
				model_print("PreCondition is not satisfied (Problematic method call"
				 " printed");
				m->print();
				return false;
			}
		}
		
		// Execute the side effect of Method m
		if (sideEffect)
			(*sideEffect)(m);
		// Check PostCondition of Method m
		if (postCondition) {
			satisfied = (*postCondition)(m);
			if (!satisfied) {
				model_print("PostCondition is not satisfied (Problematic method call"
				 " printed");
				m->print();
				return false;
			}
		}
	}
	return true;
}


/**
	To take a list of actions and print it out in a nicer way
*/
void ExecutionGraph::printActions(action_list_t *actions, const char *header) {
	model_print("%s\n", header);
	model_print("---------- thread list (begin) ---------\n");
	for (action_list_t::iterator it = actions->begin(); it != actions->end();
		it++) {
		ModelAction *act = *it;
		SpecAnnotation *anno = getAnnotation(act);
		if (anno) {
			model_print("%d -> ", anno->type);
		}
		act->print();
	}
	model_print("---------- thread list (end) ---------\n");
}
