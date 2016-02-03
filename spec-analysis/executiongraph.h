#ifndef _CPGRAPH_H
#define _CPGRAPH_H

#include <stack>

#include "hashtable.h"
#include "specannotation.h"
#include "mymemory.h"
#include "modeltypes.h"
#include "action.h"
#include "common.h"
#include "execution.h"

#include "methodcall.h"
#include "specannotation.h"

using std::string;

/**
	Record the a potential commit point information, including the potential
	commit point label number and the corresponding operation
*/
typedef struct PotentialOP {
	ModelAction *operation;
	string label;

	PotentialOP(ModelAction *op, string name);

	SNAPSHOTALLOC
} PotentialOP;

class Graph;


typedef SnapList<PotentialOP*> PotentialOPList;
typedef SnapList<Method> MethodList;
typedef SnapVector<Method> MethodVector;
typedef SnapVector<MethodList*> MethodListVector;

/**
	This represents the execution graph at the method call level. Each node is a
	MethodCall type that has the runtime info (return value & arguments) and the
	state info the method call. The edges in this graph are the hb/SC edges
	dereived from the ordering points of the method call. Those edges
	information are stoed in the PREV and NEXT set of MethodCall struct.
*/
class ExecutionGraph {
	public:
	ExecutionGraph(ModelExecution *e);

	/********** Public class interface for the core checking engine **********/
	/**
		Build the graph for an execution. It should returns true when the graph
	 	is correctly built
	*/
	void buildGraph(action_list_t *actions);

	/** Check whether this is a broken execution */
	bool isBroken();

	/**
		Check whether this is a cyclic execution graph, meaning that the graph
	 	has an cycle derived from the ordering points' hb/SC relation
	*/
	bool hasCycle();

	/** Reset the internal graph "broken" state to okay again */
	void resetBroken();

	/** Check whether the execution is admmisible */
	bool checkAdmissibility();

	/** Generate one random topological sorting */
	MethodList* generateOneHistory();

	/** Generate all topological sortings */
	MethodListVector* generateAllHistories();

	bool checkOneHistory(MethodList *list);

	bool checkAllHistories(MethodListVector *sortings);
	
	/********** A few public printing functions for DEBUGGING **********/
	/** Print a random sorting */
	void printOneHistory(MethodList *list);

	/** Print all the possible sortings */
	void printAllHistories(MethodListVector *sortings);

	/** Print out the graph for the purpose of debugging */
	void print();

	SNAPSHOTALLOC

	private:
	/** The corresponding execution */
	ModelExecution *execution;

	/** All the threads each in a separate list of actions */
	SnapVector<action_list_t*> *threadLists;

	/** A plain list of all method calls (Method) in this execution */
	MethodList *methodList;

	/**
		A list that represents some random history. The reason we have this here
		is that before checking anything, we have to check graph acyclicity, and
		that requires us to check whether we can generate a topological sorting.
		Therefore, when we check it, we store that random result here.
	*/
	MethodList *randomHistory;

	/** Whether this is a broken graph */
	bool broken;

	/** Whether there is a cycle in the graph */
	bool cyclic;

	/** The state initialization function */
	UpdateState_t initial;

	/** FIXME: The final check function; we might delete this */
	CheckState_t final;

	/** The state copy function */
	CopyState_t copy;

	/** The map from interface label name to the set of spec functions */
	SnapMap<string, StateFunctions*> *funcMap;

	/** The commutativity rule array and its size */
	CommutativityRule *commuteRules;
	int commuteRuleNum;
	
	/********** Internal member functions **********/

	/**
		Simply build a vector of lists of thread actions. Such info will be very
		useful for later constructing the execution graph
	*/
	void buildThreadLists(action_list_t *actions);

	/** Build the nodes from a thread list */
	void buildNodesFromThread(action_list_t *actions);

	/** Build the nodes from all the thread lists */
	void buildNodesFromThreads();


	/**
		Find the previous non-annotation model action (ordering point from the
		current iterator
	*/
	ModelAction* findPrevAction(action_list_t *actions, action_list_t::iterator
		iter);

	/**
		Extract a MethodCall node starting from the current iterator, and the
	 	iter iterator will be advanced to the next INTERFACE_BEGIN annotation.
	 	When called, the iter must point to an INTERFACE_BEGIN annotation
	*/
	Method extractMethod(action_list_t *actions, action_list_t::iterator &iter);

	/**
		Process the initialization annotation block to initialize the
		commutativity rule info and the checking function info 
	*/
	void processInitAnnotation(AnnoInit *annoInit);

	/**
		After extracting the MethodCall info for each method call, we use this
	 	function to build the edges (a few different sets of edges that
		represent the edge of the execution graph (PREV, NEXT & CONCURRENT...)
	*/
	void buildEdges();

	/**
		The conflicting relation between two model actions by hb/SC. If act1 and
		act2 commutes, it returns 0; Otherwise, if act1->act2, it returns 1; and
		if act2->act1, it returns -1
	*/
	int conflict(ModelAction *act1, ModelAction *act2);
	
	/**
		If there is no conflict between the ordering points of m1 and m2, then
		it returns 0; Otherwise, if m1->m2, it returns 1; and if m2->m1, it
		returns -1.  If some ordering points have inconsistent conflicting
		relation, we print out an error message (Self-cycle) and set the broken
		flag and return
	*/
	int conflict(Method m1, Method m2);

	/**
		Check whether m1 is before m2 in hb/SC; Reachability <=> m1 -hb/SC-> m2.
		We need to call this method when checking admissibility properties
	*/
	bool isReachable(Method m1, Method m2);

	/**
		Find the list of nodes that do not have any incoming edges (root nodes).
		Be careful that when generating histories, this function will also
		consider whether a method node logicall exists.
	*/
	MethodVector* getRootNodes();

	/**
		Find the list of nodes that do not have any outgoing edges (end nodes)
	*/
	MethodVector* getEndNodes();

	/**
		A helper function for generating sequential histories, and we cau call
		this function to either generate one random history or all
		possible histories. Before calling this function, initialize an empty
		MethodListVector and an empty MethodList, and pass them as the results
		and curList. Also pass the size of the method list as the numLiveNodes,
		and found should be passed as false.

		If you only want to generate one random history, pass true for
		generateOne, and the result would be in (*results)[0]. Otherwise, you
		can pass false to generateOne, and this routine will recursively run and
		collect the whole set of all possible histories and store it in the
		results vector.
	*/
	void generateHistoriesHelper(MethodListVector* results,
		MethodList *curList, int &numLiveNodes, bool generateOne, bool &found);

	/** 
		Check the state specifications (PreCondition & PostCondition & state
		transition and evaluation) based on the graph (called after
		admissibility check)
	*/
	bool checkStateSpec(MethodList *history);

	/** Print a problematic thread list */
	void printActions(action_list_t *actions, const char *header = "The problematic thread list:");

};

#endif
