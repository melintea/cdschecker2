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


/********************    ExecutionGraph    ********************/
/*********    ExecutionGraph (public functions)   **********/
/**
    Initialze the necessary fields 
*/
ExecutionGraph::ExecutionGraph(ModelExecution *e, bool allowCyclic) : execution(e), allowCyclic(allowCyclic) {
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
    randomHistory = generateOneRandomHistory();
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
            
            /* Now we need to check whether we have a commutativity rule that
             * says the two method calls should be ordered */
            bool commute = true;
            for (int i = 0; i < commuteRuleNum; i++) {
                CommutativityRule rule = *(commuteRules + i);
                /* Check whether condition is satisfied */
                if (!rule.isRightRule(m1, m2)) // Not this rule
                    continue;
                else
                    commute = rule.checkCondition(m1, m2);
                if (!commute) // The rule requires them to be ordered
                    break;
            }
            // We have a rule that require m1 and m2 to be ordered
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

/** Checking cyclic graph specification */
bool ExecutionGraph::checkCyclicGraphSpec(bool verbose) {
    if (verbose) {
        model_print("---- Start to check cyclic graph ----\n");
    }

    bool pass = false;
    for (MethodList::iterator it = methodList->begin(); it != methodList->end();
        it++) {
        Method m = *it;
        if (isFakeMethod(m))
            continue;

        StateFunctions *funcs = NULL;
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

        // Cyclic graph only supports @PreCondition
        funcs = funcMap->get(m->name);
        ASSERT (funcs);
        CheckState_t preCondition = (CheckState_t)
            funcs->preCondition->function;

        // @PreCondition of Mehtod m
        if (preCondition) {
            pass = (*preCondition)(m, m);

            if (!pass) {
                model_print("PreCondition is not satisfied. Problematic method"
                    " is as follow: \n");
                m->print(true, true);
                if (verbose) {
                    model_print("---- Check cyclic graph END ----\n\n");
                }
                return false;
            }
        }
    }

    if (!pass) {
        // Print out the graph
        model_print("Problmatic execution graph: \n");
        print(true);
    } else if (verbose) {
        // Print out the graph in verbose
        model_print("Execution graph: \n");
        print(true);
    }

    if (verbose) {
        model_print("---- Check cyclic graph END ----\n\n");
    }
    return true;
}




/** To check "num" random histories */
bool ExecutionGraph::checkRandomHistories(int num, bool stopOnFail, bool verbose) {
    ASSERT (!cyclic);
    bool pass = true;
    int i;
    for (i = 0; i < num; i++) {
        MethodList *history = generateOneRandomHistory();
        pass &= checkStateSpec(history, verbose, i + 1);
        if (!pass) {
            // Print out the graph
            model_print("Problmatic execution graph: \n");
            print();
            if (stopOnFail) { // Just stop on this
                // Recycle
                delete history;
                if (verbose)
                    model_print("We totally checked %d histories.\n", i + 1);
                return false;
            }
        } else if (verbose) {
            // Print out the graph in verbose
            model_print("Execution graph: \n");
            print();
        }
        // Recycle
        delete history;
    }
    
    if (verbose)
        model_print("We totally checked %d histories.\n", i);
    return pass;
}


/**
    To generate and check all possible histories.
    
    If stopOnFailure is true, we stop generating any history and end the
    checking process. Verbose flag controls how the checking process is exposed. 
*/
bool ExecutionGraph::checkAllHistories(bool stopOnFailure, bool verbose) {
    ASSERT (!cyclic);
    MethodList *curList = new MethodList;
    int numLiveNodes = methodList->size();
    int historyIndex = 1;
    if (verbose) {
        // Print out the graph in verbose
        print();
    }
    
    // FIXME: make stopOnFailure always true
    stopOnFailure = true;
    bool pass = checkAllHistoriesHelper(curList, numLiveNodes, historyIndex,
        stopOnFailure, verbose);
    if (pass) {
        for (MethodList::iterator it = methodList->begin(); it !=
            methodList->end(); it++) {
            Method m = *it;
            if (isFakeMethod(m))
                continue;
            if (!m->justified) {
                model_print("\t");
                m->print(false, false);
                model_print(": unjustified\n");
                pass = false;
                break;
            }
        }
    }

    if (!pass && !verbose) {
        // Print out the graph
        print();
    }
    if (verbose)
        model_print("We totally checked %d histories.\n", historyIndex - 1);
    return pass;
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
    int nestedLevel = 0;
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
                nestedLevel++;
                break;
            case INTERFACE_END:
                if (nestedLevel == 0) {
                    methodComplete = true;
                }
                else
                    nestedLevel--;
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
        getAnnotation(*iter)->type == INTERFACE_END));
    if (iter != actions->end())
        iter++;

    delete popList;
    // XXX: We just allow methods to have no specified ordering points. In that
    // case, the method is concurrent with every other method call
    if (m->orderingPoints->size() == 0) {
        noOrderingPoint = true;
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

    func = annoInit->clear;
    ASSERT (func && func->type == CLEAR);
    clear= annoInit->clear;

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
    After building up the graph (both the nodes and egdes are correctly built),
    we also call this function to initialize the most recent justified node of
    each method node.

    A justified method node of a method m is a method that is in the allPrev set
    of m, and all other nodes in the allPrev set of m are either before or after
    it. The most recent justified node is the most recent one in the hb/SC
    order.
*/
void ExecutionGraph::initializeJustifiedNode() {
    MethodList::iterator it = methodList->begin();
    // Start from the second methods in the list --- skipping the START node
    for (it++; it != methodList->end(); it++) {
        Method m = *it;
        // Walk all the way up, when we have multiple immediately previous
        // choices, pick one and check if that node is a justified node --- its
        // concurrent set should be disjoint with the whole set m->allPrev.  If
        // not, keep going up; otherwise, that node is the most recent justified
        // node
        
        MethodSet prev = NULL;
        Method justified = m;
        do {
            prev = justified->prev;
            // At the very least we should have the START nodes
            ASSERT (!prev->empty());
            
            // setIt points to the very beginning of allPrev set
            SnapSet<Method>::iterator setIt = prev->begin();
            justified = *setIt;
            // Check whether justified is really the justified node
            if (MethodCall::disjoint(justified->concurrent, m->allPrev))
                break;
        } while (true);
        
        ASSERT (justified != m);
        // Don't forget to set the method's field
        m->justifiedMethod = justified;

        // Ensure we reset the justified field to be false in the beginning
        m->justified = false;
    }
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
            } else if (val == SELF_CYCLE) {
                if (allowCyclic) {
                    // m1 -> m2
                    m1->allNext->insert(m2);
                    m2->allPrev->insert(m1);
                    // m2 -> m1
                    m2->allNext->insert(m1);
                    m1->allPrev->insert(m2);
                }
            }
        }
    }

    // Initialize two special nodes (START & FINISH)
    Method startMethod = new MethodCall(GRAPH_START);
    Method finishMethod = new MethodCall(GRAPH_FINISH);
    // Initialize startMethod and finishMethd
    startMethod->allNext->insert(finishMethod);
    finishMethod->allPrev->insert(startMethod);
    for (MethodList::iterator iter = methodList->begin(); iter !=
        methodList->end(); iter++) {
        Method m = *iter;
        startMethod->allNext->insert(m);
        m->allPrev->insert(startMethod);
        m->allNext->insert(finishMethod);
        finishMethod->allPrev->insert(m);
    }
    // Push these two special nodes to the beginning & end of methodList
    methodList->push_front(startMethod);
    methodList->push_back(finishMethod);
    
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

    if (!cyclic)
        AssertEdges();
    // Initialize the justified method of each method
    if (!cyclic)
        initializeJustifiedNode();
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
    
    if (isStartMethod(m1))
        return 1;
    if (isFinishMethod(m2))
        return 1;

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
            if (res == 0) // Commuting actions
                continue;
            if (val == 0)
                val = res;
            else if (val != res) { // Self cycle
                cyclic = true;
                if (!allowCyclic) {
                    model_print("There is a self cycle between the following two "
                        "methods\n");
                    m1->print();
                    m2->print();
                    broken = true;
                }
                return SELF_CYCLE;
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
    curList -> It represents a temporal result of a specific topological
        sorting; keep in mind that before calling this function, pass an empty
        list. 
    numLiveNodes -> The number of nodes that are logically alive (that have not
        been selected and added in a specific topological sorting yet). We keep
        such a number as an optimization since when numLinveNodes equals to 0,
        we can backtrack. Initially it is the size of the method call list.
    historyIndex -> The current history index. We should start with 1.
    stopOnFailure -> Stop the checking once we see a failed history
    verbose -> Whether the verbose mode is on
*/
bool ExecutionGraph::checkAllHistoriesHelper(MethodList *curList, int
    &numLiveNodes, int &historyIndex, bool stopOnFailure, bool verbose) {
    if (cyclic)
        return false;
    
    bool satisfied = true;
    if (numLiveNodes == 0) { // Found one sorting, and we can backtrack
        // Don't forget to increase the history number
        satisfied = checkStateSpec(curList, verbose, historyIndex++);
        // Don't forget to recycle
        delete curList;
        return satisfied;
    }

    MethodVector *roots = getRootNodes();
    // Cycle exists (no root nodes but still have live nodes
    if (roots->size() == 0) {
        model_print("There is a cycle in this graph so we cannot generate"
            " sequential histories\n");
        cyclic = true;
        return false;
    }

    for (unsigned i = 0; i < roots->size(); i++) {
        Method m = (*roots)[i];
        m->exist = false;
        numLiveNodes--;
        // Copy the whole current list and use that copy for the next recursive
        // call (because we will need the same copy for other iterations at the
        // same level of recursive calls)
        MethodList *newList = new MethodList(*curList);
        newList->push_back(m);
        
        bool oneSatisfied = checkAllHistoriesHelper(newList, numLiveNodes,
            historyIndex, stopOnFailure, verbose);
        // Stop the checking once failure or cycle detected
        if (!oneSatisfied && (cyclic || stopOnFailure)) {
            delete curList;
            delete roots;
            return false;
        }
        satisfied &= oneSatisfied;
        // Recover
        m->exist = true;
        numLiveNodes++;
    }
    delete curList;
    delete roots;
    return satisfied;
}

/** To check one generated history */
bool ExecutionGraph::checkHistory(MethodList *history, int historyIndex, bool
    verbose) {
    bool pass = checkStateSpec(history, verbose, historyIndex);
    if (!pass) {
        // Print out the graph
        model_print("Problmatic execution graph: \n");
        print();
    } else if (verbose) {
        // Print out the graph in verbose
        model_print("Execution graph: \n");
        print();
    }
    return pass;
}

/** Generate one random topological sorting */
MethodList* ExecutionGraph::generateOneRandomHistory() {
    MethodList *res = new MethodList;
    int liveNodeNum = methodList->size();
    generateOneRandomHistoryHelper(res, liveNodeNum);
    if (cyclic) {
        delete res;
        return NULL;
    }
    // Reset the liveness of each method
    for (MethodList::iterator it = methodList->begin(); it != methodList->end();
    it++) {
        Method m = *it;
        m->exist = true;
    }
    return res;
}

/**
    The helper function to generate one random topological sorting
*/
void ExecutionGraph::generateOneRandomHistoryHelper(MethodList
    *curList, int &numLiveNodes) {
    if (cyclic)
        return;

    if (numLiveNodes == 0) { // Found one sorting, and we can return 
        // Don't forget to recycle
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

    srand (time(NULL));
    int pick = rand() % roots->size();
    Method m = (*roots)[pick];

    m->exist = false;
    numLiveNodes--;
    curList->push_back(m);

    delete roots;
    generateOneRandomHistoryHelper(curList, numLiveNodes);
}

Method ExecutionGraph::getStartMethod() {
    return methodList->front();
}

Method ExecutionGraph::getFinishMethod() {
    return methodList->back();
}

/**
    Print out the ordering points and dynamic calling info (return value &
    arguments) of all the methods in the methodList
*/
void ExecutionGraph::printAllMethodInfo(bool verbose) {
    model_print("------------------  Method Info (exec #%d)"
        "  ------------------\n", execution->get_execution_number());
    for (MethodList::iterator iter = methodList->begin(); iter !=
        methodList->end(); iter++) {
        Method m = *iter;
        printMethodInfo(m, verbose);
    }
    model_print("------------------  End Info (exec #%d)"
        "  ------------------\n\n", execution->get_execution_number());
}

/**
    Print out the ordering points and dynamic calling info (return value &
    arguments).
*/
void ExecutionGraph::printMethodInfo(Method m, bool verbose) {
    StateFunctions *funcs = NULL;
    m->print(verbose, true);

    if (isFakeMethod(m))
        return;
    
    funcs = funcMap->get(m->name);
    ASSERT (funcs);
    UpdateState_t printValue = (UpdateState_t) funcs->print->function;

    model_print("\t**********  Value Info  **********\n");
    if (printValue) {
        (*printValue)(m);
    } else {
        model_print("\tNothing printed..\n");
    }
}


/** Clear the states of the method call */
void ExecutionGraph::clearStates() {
    UpdateState_t clearFunc = (UpdateState_t) clear->function;
    for (MethodList::iterator it = methodList->begin(); it != methodList->end();
        it++) {
        Method m = *it;
        if (m->state) {
            (*clearFunc)(m);
        }
    }
}


/**
    Checking the state specification (in sequential order)
*/
bool ExecutionGraph::checkStateSpec(MethodList *history, bool verbose, int
    historyIndex) {
    if (verbose) {
        if (historyIndex > 0)
            model_print("---- Start to check history #%d ----\n", historyIndex);
        else
            model_print("---- Start to check history ----\n");
    }

    // @Transition can also return a boolean. For example when a steal() and
    // take() in the deque both gets the last element, then we can see a bug in
    // the evaluating the list of @Transitions for a following operation.
    bool satisfied = true;

    // @Initial state (with a fake starting node)
    Method startMethod = getStartMethod();
    UpdateState_t initialFunc = (UpdateState_t) initial->function;
    UpdateState_t printStateFunc = (UpdateState_t) printState->function;
    UpdateState_t clearFunc = (UpdateState_t) clear->function;

    // We execute the equivalent sequential data structure with the state of the
    // startMethod node 
    (*initialFunc)(startMethod);
    if (verbose) {
        startMethod->print(false, true);
        model_print("\t@Initial on START\n");
        if (printStateFunc) { // If user has defined the print-out function
            model_print("\t**********  State Info  **********\n");
            (*printStateFunc)(startMethod);
        }
    }
    
    StateFunctions *funcs = NULL;
    /** Execute each method call in the history */
    for (MethodList::iterator it = history->begin(); it != history->end();
        it++) {
        Method m = *it;
        if (isFakeMethod(m))
            continue;
    
        StateTransition_t transition = NULL;
        
        if (verbose) {
            m->print(false, true);
            funcs = funcMap->get(m->name);
            ASSERT (funcs);
            UpdateState_t printValue = (UpdateState_t) funcs->print->function;
            if (printValue) {
                model_print("\t**********  Value Info  **********\n");
                (*printValue)(m);
            }
        }

        funcs = funcMap->get(m->name);
        ASSERT (funcs);

        CheckState_t preCondition = (CheckState_t)
            funcs->preCondition->function;
        // @PreCondition of Mehtod m
        if (preCondition) {
            satisfied = (*preCondition)(startMethod, m);

            if (!satisfied) {
                model_print("PreCondition is not satisfied. Problematic method"
                    " is as follow: \n");
                m->print(true, true);
                printOneHistory(history, "Failed History");
                if (verbose) {
                    if (historyIndex > 0)
                        model_print("---- Check history #%d END ----\n\n",
                            historyIndex);
                    else
                        model_print("---- Check history END ----\n\n");
                }
                break;
            }
        }

        // After checking the PreCondition, we run the transition on the
        // startMethod node to update its state
        transition = (StateTransition_t) funcs->transition->function;
        // @Transition on the state of startMethod
        satisfied = (*transition)(startMethod, m);
        if (!satisfied) { // Error in evaluating @Transition
            model_print("Transition returns false. Problematic method"
                " is as follow: \n");
            m->print(true, true);
            printOneHistory(history, "Failed History");
            if (verbose) {
                if (historyIndex > 0)
                    model_print("---- Check history #%d END ----\n\n",
                        historyIndex);
                else
                    model_print("---- Check history END ----\n\n");
            }
            break;
        }

        if (verbose) {
            model_print("\t@Transition on itself\n");
            if (printStateFunc) {
                model_print("\t**********  State Info  **********\n");
                (*printStateFunc)(startMethod);
            }
        }
        
        // @PostCondition of Mehtod m
        CheckState_t postCondition = (CheckState_t)
            funcs->postCondition->function;
        if (postCondition) {
            satisfied = (*postCondition)(startMethod, m);

            if (!satisfied) {
                model_print("PostCondition is not satisfied. Problematic method"
                    " is as follow: \n");
                m->print(true, true);
                printOneHistory(history, "Failed History");
                if (verbose) {
                    if (historyIndex > 0)
                        model_print("---- Check history #%d END ----\n\n",
                            historyIndex);
                    else
                        model_print("---- Check history END ----\n\n");
                }
                break;
            }
        }
    }

    // Clear out the states created when checking
    (*clearFunc)(startMethod);

    if (satisfied && verbose) {
        printOneHistory(history, "Passed History");
        // Print the history in verbose mode
        if (historyIndex > 0)
            model_print("---- Check history #%d END ----\n\n", historyIndex);
        else
            model_print("---- Check history END ----\n\n");
    }
    
    if (verbose) {
        if (historyIndex > 0)
            model_print("---- Start to check justifying subhistory #%d ----\n", historyIndex);
        else
            model_print("---- Start to check justifying subhistory ----\n");

    }
    for (MethodList::iterator it = history->begin(); it != history->end();
        it++) {
        Method m = *it;
        if (isFakeMethod(m))
            continue;
        // Check justifying subhistory
        if (!m->justified) {
            funcs = funcMap->get(m->name);
            CheckState_t justifyingPrecondition = (CheckState_t)
                funcs->justifyingPrecondition->function;
            CheckState_t justifyingPostcondition = (CheckState_t)
                funcs->justifyingPostcondition->function;
            if (!justifyingPrecondition && !justifyingPostcondition ) {
                // No need to check justifying conditions
                m->justified = true;
                if (verbose) {
                    model_print("\tMethod call  ");
                    m->print(false, false);
                    model_print(": automatically justified.\n");
                }
            } else {
                bool justified = checkJustifyingSubhistory(history, m, verbose, historyIndex);
                if (justified) {
                    // Set the "justified" flag --- no need to check again for cur
                    m->justified = true;
                    if (verbose) {
                        model_print("\tMethod call  ");
                        m->print(false, false);
                        model_print(": is justified\n");
                    }
                } else {
                    if (verbose) {
                        model_print("\tMethod call  ");
                        m->print(false, false);
                        model_print(": has NOT been justified yet\n");
                    }
                }
            }
        } else {
            if (verbose) {
                model_print("\tMethod call  ");
                m->print(false, false);
                model_print(": is justified\n");
            }
        }
    }
    if (verbose) {
        if (historyIndex > 0)
            model_print("---- Start to check justifying subhistory #%d END ----\n\n", historyIndex);
        else
            model_print("---- Start to check justifying subhistory # END ----\n\n");

    }

    return satisfied;
}

bool ExecutionGraph::checkJustifyingSubhistory(MethodList *history, Method
    cur, bool verbose, int historyIndex) {
    if (verbose) {
        model_print("\tMethod call  ");
        cur->print(false, false);
        model_print(": is being justified\n");
    }

    // @Transition can also return a boolean. For example when a steal() and
    // take() in the deque both gets the last element, then we can see a bug in
    // the evaluating the list of @Transitions for a following operation.
    bool satisfied = true;

    // @Initial state (with a fake starting node)
    Method startMethod = getStartMethod();
    UpdateState_t initialFunc = (UpdateState_t) initial->function;
    UpdateState_t printStateFunc = (UpdateState_t) printState->function;
    //UpdateState_t printVauleFunc = NULL;
    UpdateState_t clearFunc = (UpdateState_t) clear->function;

    // We execute the equivalent sequential data structure with the state of the
    // current method call
    (*initialFunc)(cur);
    if (verbose) {
        startMethod->print(false, true);
        model_print("\t@Initial on ");
        cur->print(false, true);
        if (printStateFunc) { // If user has defined the print-out function
            model_print("\t**********  State Info  **********\n");
            (*printStateFunc)(cur);
        }
    }
    
    StateFunctions *funcs = NULL;
    StateTransition_t transition = NULL;
    /** Execute each method call in the justifying subhistory till cur */
    for (MethodList::iterator it = history->begin(); it != history->end(); it++) {
        Method m = *it;
        if (m == cur)
            break;
        if (isFakeMethod(m))
            continue;
        
        // Ignore method calls that are not in my justifying subhistory
        if (!MethodCall::belong(cur->allPrev, m))
            continue;

        
        if (verbose) {
            m->print(false, true);
            funcs = funcMap->get(m->name);
            ASSERT (funcs);
            UpdateState_t printValue = (UpdateState_t) funcs->print->function;
            if (printValue) {
                model_print("\t**********  Value Info  **********\n");
                (*printValue)(m);
            }
        }

        funcs = funcMap->get(m->name);
        ASSERT (funcs);
        transition = (StateTransition_t)
            funcs->transition->function;

        // In checking justifying behaviors, we don't check precondition &
        // postcondition for other method calls
        
        // @Transition on the state of the "cur" method call 
        satisfied = (*transition)(cur, m);
        if (!satisfied) { // Error in evaluating @Transition
            if (verbose) {
                model_print("\tFailed @Transition before\n");
            }
            // Clear out the states created when checking
            (*clearFunc)(startMethod);
            return false;
        } else {
            if (verbose) {
                model_print("\t@Transition on itself\n");
                if (printStateFunc) {
                    model_print("\t**********  State Info  **********\n");
                    (*printStateFunc)(startMethod);
                }
            }
        }
    }

    // For justifying subhistory, we only check the @JustifyingPrecondition &
    // @JustifyingPostcondition for the last method call (the one we are
    // checking)

    // First check the @JustifyingPrecondition
    funcs = funcMap->get(cur->name);
    if (satisfied) {
        // Check the justifying preondition on cur
        CheckState_t justifyingPrecondition = (CheckState_t)
            funcs->justifyingPrecondition->function;
        if (justifyingPrecondition) {
            // @JustifyingPrecondition of Mehtod cur
            satisfied = (*justifyingPrecondition)(cur, cur);
        }
        if (!satisfied) {
            if (verbose) {
                model_print("\tFailed @JustifyingPrecondition\n");
            }
            // Clear out the states created when checking
            (*clearFunc)(startMethod);
            return false;
        }
    }

    // Then execute the @Transition
    transition = (CheckState_t) funcs->transition->function;
    // @Transition on the state of the "cur" method call 
    satisfied = (*transition)(cur, cur);
    if (!satisfied) { // Error in evaluating @Transition
        if (verbose) {
            model_print("\tFailed @Transition on itself\n");
        }
        // Clear out the states created when checking
        (*clearFunc)(startMethod);
        return false;
    }
    if (verbose) {
        model_print("\t@Transition on itself\n");
        if (printStateFunc) {
            model_print("\t**********  State Info  **********\n");
            (*printStateFunc)(startMethod);
        }
    }

    // Finally check the @JustifyingPostcondition
    if (satisfied) {
        // Check the justifying preondition on cur
        funcs = funcMap->get(cur->name);
        CheckState_t justifyingPostcondition = (CheckState_t)
            funcs->justifyingPostcondition->function;
        if (justifyingPostcondition) {
            // @JustifyingPostcondition of Mehtod cur
            satisfied = (*justifyingPostcondition)(cur, cur);
        }
    }

    // Clear out the states created when checking
    (*clearFunc)(startMethod);
    return satisfied;
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

void ExecutionGraph::printOneSubhistory(MethodList *history, Method cur, CSTR header) {
    model_print("-------------    %s (exec #%d)   -------------\n", header,
        execution->get_execution_number());
    int idx = 1;
    for (MethodList::iterator it = history->begin(); it != history->end(); it++) {
        Method m = *it;
        if (!MethodCall::belong(cur->allPrev, m))
            continue;
        model_print("%d. ", idx++);
        m->print(false);
    }
    model_print("-------------    %s (exec #%d) (end)    -------------\n",
        header, execution->get_execution_number());
}

