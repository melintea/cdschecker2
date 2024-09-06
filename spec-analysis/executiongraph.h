#ifndef _EXECUTIONGRAPH_H
#define _EXECUTIONGRAPH_H

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


const int SELF_CYCLE = 0xfffe;

/**
    Record the a potential commit point information, including the potential
    commit point label number and the corresponding operation
*/
typedef struct PotentialOP {
    ModelAction *operation;
    CSTR label;

    PotentialOP(ModelAction *op, CSTR name);

    SNAPSHOTALLOC
} PotentialOP;

class Graph;


typedef SnapList<PotentialOP*> PotentialOPList;

/**
    This represents the execution graph at the method call level. Each node is a
    MethodCall type that has the runtime info (return value & arguments) and the
    state info the method call. The edges in this graph are the hb/SC edges
    dereived from the ordering points of the method call. Those edges
    information are stoed in the PREV and NEXT set of MethodCall struct.
*/
class ExecutionGraph {
    public:
    ExecutionGraph(ModelExecution *e, bool allowCyclic);

    /********** Public class interface for the core checking engine **********/
    /**
        Build the graph for an execution. It should returns true when the graph
        is correctly built
    */
    void buildGraph(action_list_t *actions);

    /** Check whether this is a broken execution */
    bool isBroken();

    /** Check whether this is an execution that has method calls without
     * ordering points */
    bool isNoOrderingPoint();

    /**
        Check whether this is a cyclic execution graph, meaning that the graph
        has an cycle derived from the ordering points' hb/SC relation
    */
    bool hasCycle();

    /** Reset the internal graph "broken" state to okay again */
    void resetBroken();

    /** Check whether the execution is admmisible */
    bool checkAdmissibility();

    /** Checking cyclic graph specification */
    bool checkCyclicGraphSpec(bool verbose);

    /** Check whether a number of random history is correct */
    bool checkRandomHistories(int num = 1, bool stopOnFail = true, bool verbose = false);

    /** Check whether all histories are correct */
    bool checkAllHistories(bool stopOnFailure = true, bool verbose = false);
    
    /********** A few public printing functions for DEBUGGING **********/

    /**
        Print out the ordering points and dynamic calling info (return value &
        arguments) of all the methods in the methodList
    */
    void printAllMethodInfo(bool verbose);

    /** Print a random sorting */
    void printOneHistory(MethodList *list, CSTR header = "A random history");

    /** Print all the possible sortings */
    void printAllHistories(MethodListVector *sortings);

    /** Print out the graph for the purpose of debugging */
    void print(bool allEdges = false);

    /**
        Print out the graph in reverse order (with previous edges) for the
        purpose of debugging
    */
    void PrintReverse(bool allEdges);

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

    /** Whether this graph has method calls that have no ordering points */
    bool noOrderingPoint;

    /** Whether there is a cycle in the graph */
    bool cyclic;

    /** Whether users expect us to check cyclic graph */
    bool allowCyclic;


    /** The state initialization function */
    NamedFunction *initial;

    /** FIXME: The final check function; we might delete this */
    NamedFunction *final;

    /** The state copy function */
    NamedFunction *copy;

    /** The state clear function */
    NamedFunction *clear;

    /** The state print-out function */
    NamedFunction *printState;

    /** The map from interface label name to the set of spec functions */
    StateFuncMap *funcMap;

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
        A utility function to extract the actual annotation
        pointer and return the actual annotation if this is an annotation action;
        otherwise return NULL.
    */
    SpecAnnotation* getAnnotation(ModelAction *act);

    /**
        After building up the graph (both the nodes and egdes are correctly
        built), we also call this function to initialize the most recent
        justified node of each method node.

        A justified method node of a method m is a method that is in the allPrev
        set of m, and all other nodes in the allPrev set of m are either before
        or after it. The most recent justified node is the most recent one in
        the hb/SC order.
    */
    void initializeJustifiedNode();

    /**
        After extracting the MethodCall info for each method call, we use this
        function to build the edges (a few different sets of edges that
        represent the edge of the execution graph (PREV, NEXT & CONCURRENT...)
    */
    void buildEdges();

    /**
        This method call is used to check whether the edge sets of the nodes are
        built correctly --- consistently. We should only use this routine after the
        builiding of edges when debugging
    */
    void AssertEdges();

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
        A helper function for generating and check all sequential histories. Before calling this function, initialize an empty
        MethodList , and pass it as curList. Also pass the size of the method
        list as the numLiveNodes.

        If you only want to stop the checking process when finding one failed
        history, pass true to stopOnFailure.
    */
    bool checkAllHistoriesHelper(MethodList *curList, int &numLiveNodes, int
    &historyNum, bool stopOnFailure, bool verbose);


    /** Check whether a specific history is correct */
    bool checkHistory(MethodList *history, int historyIndex, bool verbose = false);

    /** Generate one random topological sorting */
    MethodList* generateOneRandomHistory();

    /**
        The helper function to generate one random topological sorting
    */
    void generateOneRandomHistoryHelper(MethodList
        *curList, int &numLiveNodes);

    /** Return the fake nodes -- START & END */
    Method getStartMethod();
    Method getFinishMethod();
    
    /** Whether method call node m is a fake node */
    inline bool isFakeMethod(Method m) {
        return isStartMethod(m) || isFinishMethod(m);
    }

    /** Whether method call node m is a starting node */
    inline bool isStartMethod(Method m) {
        return m->name == GRAPH_START;
    }

    /** Whether method call node m is a finish node */
    inline bool isFinishMethod(Method m) {
        return m->name == GRAPH_FINISH;
    }

    /**
        Print out the ordering points and dynamic calling info (return value &
        arguments).
    */
    void printMethodInfo(Method m, bool verbose);

    /** Clear the states of the method call */
    void clearStates();

    /** 
        Check the state specifications (PreCondition & PostCondition & state
        transition and evaluation) based on the graph (called after
        admissibility check). The verbose option controls whether we print a
        detailed list of checking functions that we have called
    */
    bool checkStateSpec(MethodList *history, bool verbose, int historyNum);

    /**
        Check the justifying subhistory with respect to history of m.
    */
    bool checkJustifyingSubhistory(MethodList *subhistory, Method cur,
        bool verbose, int historyIndex);

    /** Print a problematic thread list */
    void printActions(action_list_t *actions, const char *header = "The problematic thread list:");

    /** Print one jusifying subhistory of method call cur */
    void printOneSubhistory(MethodList *history, Method cur,
        CSTR header);
};

#endif
