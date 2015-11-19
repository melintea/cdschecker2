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

// Record the a potential commit point information, including the potential
// commit point label number and the corresponding operation
typedef struct PotentialCPInfo {
	ModelAction *operation;
	char *labelName;
	int labelNum;

	PotentialCPInfo(ModelAction *op, char *label, int num);

	SNAPSHOTALLOC
} PotentialCPInfo;


/**
	We define a struct here rather than just use the ModelAction. It stores
	extra information such as the label name here to help debugging.
*/
typedef struct CommitPoint {
	ModelAction *operation; // The memory operation
	char *labelName; // The name of this commit point (mostly for debugging)
	int labelNum;

	CommitPoint(ModelAction *op, char *label, int num);

	SNAPSHOTALLOC
} CommitPoint;

class CPEdge;
class CPNode;
class CPGraph;

typedef enum CPEdgeType {
	HB, SC, MO, RF, // Happens-before, SC, Modification-order & Reads-from
	RBW // Read-before-write(for RF)
} CPEdgeType;

typedef HashTable<ModelAction*, CPNode*, uintptr_t, 4> CPNodeTable;
typedef SnapVector<CPNode*> CPNodeList;
typedef SnapVector<CPEdge*> CPEdgeList;
typedef SnapVector<CPNodeList*> CPNodeListVector;

typedef SnapList<PotentialCPInfo*> PotentialCPList;
typedef SnapList<CommitPoint*> CPList;
typedef SnapList<hb_rule*> HBRuleList;
typedef SnapList<commutativity_rule*> CommuteRuleList;
typedef SnapVector<anno_hb_condition*> HBConditionList;
typedef HashTable<call_id_t, CPNodeList*, uintptr_t, 4> ID2NodesTable;

class CPEdge {
	private:
	CPEdgeType type;
	CPNode *node;

	public:
	CPEdge(CPEdgeType t, CPNode *node);

	CPEdgeType getType();

	CPNode* getNode();

	SNAPSHOTALLOC
};


/**
	CPNode represents each node of the CPGraph. In each node,
	we wrap the information of the list of commit point operations, the
	hb_conditions, the __RET__ and arguments info blcok.
*/
class CPNode {
	private:
	ModelAction *begin; // Interface begin annotation
	ModelAction *end; // Interface end annotation
	CPList *commitPoints; // List of commit point operations
	HBConditionList *hbConditions; // List of happens-before conditions
	call_id_t __ID__;
	int interfaceNum; // Interface number
	char *interfaceName; // The name of the interface
	// The pointer that points to the struct with the __RET__ and arguments
	void *info;

	// The list of outgoing edges from this node
	CPEdgeList *outEdges;
	
	// The list of incoming edges to this node
	CPEdgeList *inEdges;

	// Logically exist (for topological sort)
	bool exist;

	// For DFS
	int color; // 0 -> undiscovered; 1 -> discovered; 2 -> visited
	int finishTimeStamp;

	public:

	CPNode();

	/** Basic getters & setters */
	ModelAction* getBegin();

	void setBegin(ModelAction *act);

	ModelAction* getEnd();

	void setEnd(ModelAction *act);
	
	CPList* getCommitPoints();

	HBConditionList* getHBConditions();

	call_id_t getID();
	
	void setID(call_id_t id);

	int getInterfaceNum();
	
	void setInterfaceNum(int num);

	char* getInterfaceName();
	
	void setInterfaceName(char *name);

	void* getInfo();

	void setInfo(void *ptr);

	bool getExist();

	void setExist(bool e);

	int getColor();

	void setColor(int c);

	CPEdgeList* getOutgoingEdgeList();

	CPEdgeList* getIncomingEdgeList();

	/** More functions for building up and traverse the graph */

	void addEdge(CPNode *node, CPEdgeType type);

	SNAPSHOTALLOC
};


// The commit point graph of the current execution
class CPGraph {
	public:
	CPGraph(ModelExecution *e);

	// Build the graph for an execution. It should returns true when the graph
	// is correctly built
	void build(action_list_t *actions);
	
	/** Check whether n1->n2 is an edge */
	bool hasEdge(CPNode *n1, CPNode *n2);

	// Check whether the execution is admmisible
	bool checkAdmissibility();

	// Print a random sorting
	void printOneSorting(CPNodeList *list);

	// Print all the possible sortings
	void printAllSortings(CPNodeListVector *sortings);

	/** A helper function that will be called recursively to generate all
	 * topological sortings */
	void generateAllSortingsHelper(CPNodeListVector* results,
		CPNodeList *curList, int &numLiveNodes, bool generateOne, bool &found);

	/** Generate all topological sortings */
	CPNodeListVector* generateAllSortings();

	/** Generate one random topological sorting */
	CPNodeList* generateOneSorting();

	// Check basic specifications by the sequential version
	bool checkSequentialSpec(CPNodeList *list);

	// Whether we should check the syncrhonization property between n1->n2
	bool shouldCheckSync(CPNode *n1, CPNode *n2);

	// Check the synchronization properties
	bool checkSynchronization(CPNodeList *list);

	bool checkOne(CPNodeList *list);

	bool checkAll(CPNodeListVector *sortings);

	// Check whether this is a broken execution 
	bool getBroken();

	bool hasCycle();

	// Reset the isBroken variable to be false again 
	void resetBroken();

	/** Print out the current node */
	void printNode(CPNode *n);

	/** Print out the current edges directly from this node */
	void printEdges(CPNode *n);


	/** Print out the graph for the purpose of debugging */
	void printGraph();

	SNAPSHOTALLOC

	private:
	// The corresponding execution
	ModelExecution *execution;

	// A plain list of nodes
	CPNodeList *nodeList;
	
	// A hashtable that make node search faster
	// (from ModelAction* [begin annotation] -> CPNode) 
	CPNodeTable *nodeTable;

	// One random topological sortings, initially a NULL pointer
	CPNodeList *oneSorting;

	// Whether this is a broken graph
	bool isBroken;

	// Whether there is a cycle in the graph
	bool cyclic;

	// The annotation block that has the information of init_func, func_table,
	// hb_init_table and commutativity_rule_table 
	void_func_t initFunc;

	cleanup_func_t cleanupFunc;

	void **funcTable;
	int funcTableSize;

	HBRuleList *hbRules;
	int hbRulesSize;

	CommuteRuleList *commuteRules;
	int commuteRulesSize;

	SnapVector<action_list_t*> *threadLists;

	/** Extract the actual annotation pointer */
	spec_annotation* getAnnotation(ModelAction *act);

	/** Process the initialization annotation block to initialize part of the
	 * graph */
	void processInitAnnotation(anno_init *annoInit);

	/** Print a problematic thread list */
	void printActions(action_list_t *actions);

	/** Build the nodes from all the thread lists */
	void buildNodesFromThreads();

	/** Build the nodes from a thread list */
	void buildNodesFromThread(action_list_t *actions);

	/** Add the edges (n1 -> n2); in the process we need to check wheter the two
	 * nodes either has an edge, comute or entangled */
	bool addEdge(CPNode *n1, CPNode *n2);

	/** Build the edges, should be called when nodes are built */
	void buildEdges();


	/** Find previous the acutal operation (commit point operation) starting from the current iterator */
	ModelAction* findPrevCPAction(action_list_t *actions,
		action_list_t::iterator iter);

	/** Extract a commit point node starting from the current iterator */
	CPNode* extractCPNode(action_list_t *actions, action_list_t::iterator &iter);

	void buildThreadLists(action_list_t *actions);

	/** Based on the current situation of node existence, get a list of nodes
	 * that do not have any incoming edges */
	CPNodeList* getRootNodes();
};

#endif
