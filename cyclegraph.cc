#include "cyclegraph.h"
#include "action.h"
#include "common.h"
#include "promise.h"
#include "threads-model.h"

/** Initializes a CycleGraph object. */
CycleGraph::CycleGraph() :
    discovered(new HashTable<const CycleNode *, const CycleNode *, uintptr_t, 4, model_malloc, model_calloc, model_free>(16)),
    queue(new ModelVector<const CycleNode *>()),
    hasCycles(false),
    oldCycles(false)
{
}

/** CycleGraph destructor */
CycleGraph::~CycleGraph()
{
    delete queue;
    delete discovered;
}

/**
 * Add a CycleNode to the graph, corresponding to a store ModelAction
 * @param act The write action that should be added
 * @param node The CycleNode that corresponds to the store
 */
void CycleGraph::putNode(const ModelAction *act, CycleNode *node)
{
    actionToNode.put(act, node);
#if SUPPORT_MOD_ORDER_DUMP
    nodeList.push_back(node);
#endif
}

/**
 * Add a CycleNode to the graph, corresponding to a Promise
 * @param promise The Promise that should be added
 * @param node The CycleNode that corresponds to the Promise
 */
void CycleGraph::putNode(const Promise *promise, CycleNode *node)
{
    promiseToNode.put(promise, node);
#if SUPPORT_MOD_ORDER_DUMP
    nodeList.push_back(node);
#endif
}

/**
 * @brief Remove the Promise node from the graph
 * @param promise The promise to remove from the graph
 */
void CycleGraph::erasePromiseNode(const Promise *promise)
{
    promiseToNode.put(promise, NULL);
#if SUPPORT_MOD_ORDER_DUMP
    /* Remove the promise node from nodeList */
    CycleNode *node = getNode_noCreate(promise);
    for (unsigned int i = 0; i < nodeList.size(); )
        if (nodeList[i] == node)
            nodeList.erase(nodeList.begin() + i);
        else
            i++;
#endif
}

/** @return The corresponding CycleNode, if exists; otherwise NULL */
CycleNode * CycleGraph::getNode_noCreate(const ModelAction *act) const
{
    return actionToNode.get(act);
}

/** @return The corresponding CycleNode, if exists; otherwise NULL */
CycleNode * CycleGraph::getNode_noCreate(const Promise *promise) const
{
    return promiseToNode.get(promise);
}

/**
 * @brief Returns the CycleNode corresponding to a given ModelAction
 *
 * Gets (or creates, if none exist) a CycleNode corresponding to a ModelAction
 *
 * @param action The ModelAction to find a node for
 * @return The CycleNode paired with this action
 */
CycleNode * CycleGraph::getNode(const ModelAction *action)
{
    CycleNode *node = getNode_noCreate(action);
    if (node == NULL) {
        node = new CycleNode(action);
        putNode(action, node);
    }
    return node;
}

/**
 * @brief Returns a CycleNode corresponding to a promise
 *
 * Gets (or creates, if none exist) a CycleNode corresponding to a promised
 * value.
 *
 * @param promise The Promise generated by a reader
 * @return The CycleNode corresponding to the Promise
 */
CycleNode * CycleGraph::getNode(const Promise *promise)
{
    CycleNode *node = getNode_noCreate(promise);
    if (node == NULL) {
        node = new CycleNode(promise);
        putNode(promise, node);
    }
    return node;
}

/**
 * Resolve/satisfy a Promise with a particular store ModelAction, taking care
 * of the CycleGraph cleanups, including merging any necessary CycleNodes.
 *
 * @param promise The Promise to resolve
 * @param writer The store that will resolve this Promise
 * @return false if the resolution results in a cycle (or fails in some other
 * way); true otherwise
 */
bool CycleGraph::resolvePromise(const Promise *promise, ModelAction *writer)
{
    CycleNode *promise_node = promiseToNode.get(promise);
    CycleNode *w_node = actionToNode.get(writer);
    ASSERT(promise_node);

    if (w_node)
        return mergeNodes(w_node, promise_node);
    /* No existing write-node; just convert the promise-node */
    promise_node->resolvePromise(writer);
    erasePromiseNode(promise_node->getPromise());
    putNode(writer, promise_node);
    return true;
}

/**
 * @brief Merge two CycleNodes that represent the same write
 *
 * Note that this operation cannot be rolled back.
 *
 * @param w_node The write ModelAction node with which to merge
 * @param p_node The Promise node to merge. Will be destroyed after this
 * function.
 *
 * @return false if the merge cannot succeed; true otherwise
 */
bool CycleGraph::mergeNodes(CycleNode *w_node, CycleNode *p_node)
{
    ASSERT(!w_node->is_promise());
    ASSERT(p_node->is_promise());

    const Promise *promise = p_node->getPromise();
    if (!promise->is_compatible(w_node->getAction()) ||
            !promise->same_value(w_node->getAction()))
        return false;

    /* Transfer the RMW */
    CycleNode *promise_rmw = p_node->getRMW();
    if (promise_rmw && promise_rmw != w_node->getRMW() && w_node->setRMW(promise_rmw))
        return false;

    /* Transfer back edges to w_node */
    while (p_node->getNumBackEdges() > 0) {
        CycleNode *back = p_node->removeBackEdge();
        if (back == w_node)
            continue;
        addNodeEdge(back, w_node);
        if (hasCycles)
            return false;
    }

    /* Transfer forward edges to w_node */
    while (p_node->getNumEdges() > 0) {
        CycleNode *forward = p_node->removeEdge();
        if (forward == w_node)
            continue;
        addNodeEdge(w_node, forward);
        if (hasCycles)
            return false;
    }

    erasePromiseNode(promise);
    /* Not deleting p_node, to maintain consistency if mergeNodes() fails */

    return !hasCycles;
}

/**
 * Adds an edge between two CycleNodes.
 * @param fromnode The edge comes from this CycleNode
 * @param tonode The edge points to this CycleNode
 * @return True, if new edge(s) are added; otherwise false
 */
bool CycleGraph::addNodeEdge(CycleNode *fromnode, CycleNode *tonode)
{
    if (fromnode->addEdge(tonode)) {
        rollbackvector.push_back(fromnode);
        if (!hasCycles)
            hasCycles = checkReachable(tonode, fromnode);
    } else
        return false; /* No new edge */

    /*
     * If the fromnode has a rmwnode that is not the tonode, we should
     * follow its RMW chain to add an edge at the end, unless we encounter
     * tonode along the way
     */
    CycleNode *rmwnode = fromnode->getRMW();
    if (rmwnode) {
        while (rmwnode != tonode && rmwnode->getRMW())
            rmwnode = rmwnode->getRMW();

        if (rmwnode != tonode) {
            if (rmwnode->addEdge(tonode)) {
                if (!hasCycles)
                    hasCycles = checkReachable(tonode, rmwnode);

                rollbackvector.push_back(rmwnode);
            }
        }
    }
    return true;
}

/**
 * @brief Add an edge between a write and the RMW which reads from it
 *
 * Handles special case of a RMW action, where the ModelAction rmw reads from
 * the ModelAction/Promise from. The key differences are:
 *  -# No write can occur in between the @a rmw and @a from actions.
 *  -# Only one RMW action can read from a given write.
 *
 * @param from The edge comes from this ModelAction/Promise
 * @param rmw The edge points to this ModelAction; this action must read from
 * the ModelAction/Promise from
 */
template <typename T>
void CycleGraph::addRMWEdge(const T *from, const ModelAction *rmw)
{
    ASSERT(from);
    ASSERT(rmw);

    CycleNode *fromnode = getNode(from);
    CycleNode *rmwnode = getNode(rmw);

    /* We assume that this RMW has no RMW reading from it yet */
    ASSERT(!rmwnode->getRMW());

    /* Two RMW actions cannot read from the same write. */
    if (fromnode->setRMW(rmwnode))
        hasCycles = true;
    else
        rmwrollbackvector.push_back(fromnode);

    /* Transfer all outgoing edges from the from node to the rmw node */
    /* This process should not add a cycle because either:
     * (1) The rmw should not have any incoming edges yet if it is the
     * new node or
     * (2) the fromnode is the new node and therefore it should not
     * have any outgoing edges.
     */
    for (unsigned int i = 0; i < fromnode->getNumEdges(); i++) {
        CycleNode *tonode = fromnode->getEdge(i);
        if (tonode != rmwnode) {
            if (rmwnode->addEdge(tonode))
                rollbackvector.push_back(rmwnode);
        }
    }

    addNodeEdge(fromnode, rmwnode);
}
/* Instantiate two forms of CycleGraph::addRMWEdge */
template void CycleGraph::addRMWEdge(const ModelAction *from, const ModelAction *rmw);
template void CycleGraph::addRMWEdge(const Promise *from, const ModelAction *rmw);

/**
 * @brief Adds an edge between objects
 *
 * This function will add an edge between any two objects which can be
 * associated with a CycleNode. That is, if they have a CycleGraph::getNode
 * implementation.
 *
 * The object to is ordered after the object from.
 *
 * @param to The edge points to this object, of type T
 * @param from The edge comes from this object, of type U
 * @return True, if new edge(s) are added; otherwise false
 */
template <typename T, typename U>
bool CycleGraph::addEdge(const T *from, const U *to)
{
    ASSERT(from);
    ASSERT(to);

    CycleNode *fromnode = getNode(from);
    CycleNode *tonode = getNode(to);

    return addNodeEdge(fromnode, tonode);
}
/* Instantiate four forms of CycleGraph::addEdge */
template bool CycleGraph::addEdge(const ModelAction *from, const ModelAction *to);
template bool CycleGraph::addEdge(const ModelAction *from, const Promise *to);
template bool CycleGraph::addEdge(const Promise *from, const ModelAction *to);
template bool CycleGraph::addEdge(const Promise *from, const Promise *to);

#if SUPPORT_MOD_ORDER_DUMP

static void print_node(FILE *file, const CycleNode *node, int label)
{
    if (node->is_promise()) {
        const Promise *promise = node->getPromise();
        int idx = promise->get_index();
        fprintf(file, "P%u", idx);
        if (label) {
            int first = 1;
            fprintf(file, " [label=\"P%d, T", idx);
            for (unsigned int i = 0 ; i < promise->max_available_thread_idx(); i++)
                if (promise->thread_is_available(int_to_id(i))) {
                    fprintf(file, "%s%u", first ? "": ",", i);
                    first = 0;
                }
            fprintf(file, "\"]");
        }
    } else {
        const ModelAction *act = node->getAction();
        modelclock_t idx = act->get_seq_number();
        fprintf(file, "N%u", idx);
        if (label)
            fprintf(file, " [label=\"N%u, T%u\"]", idx, act->get_tid());
    }
}

static void print_edge(FILE *file, const CycleNode *from, const CycleNode *to, const char *prop)
{
    print_node(file, from, 0);
    fprintf(file, " -> ");
    print_node(file, to, 0);
    if (prop && strlen(prop))
        fprintf(file, " [%s]", prop);
    fprintf(file, ";\n");
}

void CycleGraph::dot_print_node(FILE *file, const ModelAction *act)
{
    print_node(file, getNode(act), 1);
}

template <typename T, typename U>
void CycleGraph::dot_print_edge(FILE *file, const T *from, const U *to, const char *prop)
{
    CycleNode *fromnode = getNode(from);
    CycleNode *tonode = getNode(to);

    print_edge(file, fromnode, tonode, prop);
}
/* Instantiate two forms of CycleGraph::dot_print_edge */
template void CycleGraph::dot_print_edge(FILE *file, const Promise *from, const ModelAction *to, const char *prop);
template void CycleGraph::dot_print_edge(FILE *file, const ModelAction *from, const ModelAction *to, const char *prop);

void CycleGraph::dumpNodes(FILE *file) const
{
    for (unsigned int i = 0; i < nodeList.size(); i++) {
        CycleNode *n = nodeList[i];
        print_node(file, n, 1);
        fprintf(file, ";\n");
        if (n->getRMW())
            print_edge(file, n, n->getRMW(), "style=dotted");
        for (unsigned int j = 0; j < n->getNumEdges(); j++)
            print_edge(file, n, n->getEdge(j), NULL);
    }
}

void CycleGraph::dumpGraphToFile(const char *filename) const
{
    char buffer[200];
    sprintf(buffer, "%s.dot", filename);
    FILE *file = fopen(buffer, "w");
    fprintf(file, "digraph %s {\n", filename);
    dumpNodes(file);
    fprintf(file, "}\n");
    fclose(file);
}
#endif

/**
 * Checks whether one CycleNode can reach another.
 * @param from The CycleNode from which to begin exploration
 * @param to The CycleNode to reach
 * @return True, @a from can reach @a to; otherwise, false
 */
bool CycleGraph::checkReachable(const CycleNode *from, const CycleNode *to) const
{
    discovered->reset();
    queue->clear();
    queue->push_back(from);
    discovered->put(from, from);
    while (!queue->empty()) {
        const CycleNode *node = queue->back();
        queue->pop_back();
        if (node == to)
            return true;
        for (unsigned int i = 0; i < node->getNumEdges(); i++) {
            CycleNode *next = node->getEdge(i);
            if (!discovered->contains(next)) {
                discovered->put(next, next);
                queue->push_back(next);
            }
        }
    }
    return false;
}

/**
 * Checks whether one ModelAction/Promise can reach another ModelAction/Promise
 * @param from The ModelAction or Promise from which to begin exploration
 * @param to The ModelAction or Promise to reach
 * @return True, @a from can reach @a to; otherwise, false
 */
template <typename T, typename U>
bool CycleGraph::checkReachable(const T *from, const U *to) const
{
    CycleNode *fromnode = getNode_noCreate(from);
    CycleNode *tonode = getNode_noCreate(to);

    if (!fromnode || !tonode)
        return false;

    return checkReachable(fromnode, tonode);
}
/* Instantiate four forms of CycleGraph::checkReachable */
template bool CycleGraph::checkReachable(const ModelAction *from,
        const ModelAction *to) const;
template bool CycleGraph::checkReachable(const ModelAction *from,
        const Promise *to) const;
template bool CycleGraph::checkReachable(const Promise *from,
        const ModelAction *to) const;
template bool CycleGraph::checkReachable(const Promise *from,
        const Promise *to) const;

/** @return True, if the promise has failed; false otherwise */
bool CycleGraph::checkPromise(const ModelAction *fromact, Promise *promise) const
{
    discovered->reset();
    queue->clear();
    CycleNode *from = actionToNode.get(fromact);

    queue->push_back(from);
    discovered->put(from, from);
    while (!queue->empty()) {
        const CycleNode *node = queue->back();
        queue->pop_back();

        if (node->getPromise() == promise)
            return true;
        if (!node->is_promise())
            promise->eliminate_thread(node->getAction()->get_tid());

        for (unsigned int i = 0; i < node->getNumEdges(); i++) {
            CycleNode *next = node->getEdge(i);
            if (!discovered->contains(next)) {
                discovered->put(next, next);
                queue->push_back(next);
            }
        }
    }
    return false;
}

/** @brief Begin a new sequence of graph additions which can be rolled back */
void CycleGraph::startChanges()
{
    ASSERT(rollbackvector.empty());
    ASSERT(rmwrollbackvector.empty());
    ASSERT(oldCycles == hasCycles);
}

/** Commit changes to the cyclegraph. */
void CycleGraph::commitChanges()
{
    rollbackvector.clear();
    rmwrollbackvector.clear();
    oldCycles = hasCycles;
}

/** Rollback changes to the previous commit. */
void CycleGraph::rollbackChanges()
{
    for (unsigned int i = 0; i < rollbackvector.size(); i++)
        rollbackvector[i]->removeEdge();

    for (unsigned int i = 0; i < rmwrollbackvector.size(); i++)
        rmwrollbackvector[i]->clearRMW();

    hasCycles = oldCycles;
    rollbackvector.clear();
    rmwrollbackvector.clear();
}

/** @returns whether a CycleGraph contains cycles. */
bool CycleGraph::checkForCycles() const
{
    return hasCycles;
}

/**
 * @brief Constructor for a CycleNode
 * @param act The ModelAction for this node
 */
CycleNode::CycleNode(const ModelAction *act) :
    action(act),
    promise(NULL),
    hasRMW(NULL)
{
}

/**
 * @brief Constructor for a Promise CycleNode
 * @param promise The Promise which was generated
 */
CycleNode::CycleNode(const Promise *promise) :
    action(NULL),
    promise(promise),
    hasRMW(NULL)
{
}

/**
 * @param i The index of the edge to return
 * @returns The CycleNode edge indexed by i
 */
CycleNode * CycleNode::getEdge(unsigned int i) const
{
    return edges[i];
}

/** @returns The number of edges leaving this CycleNode */
unsigned int CycleNode::getNumEdges() const
{
    return edges.size();
}

/**
 * @param i The index of the back edge to return
 * @returns The CycleNode back-edge indexed by i
 */
CycleNode * CycleNode::getBackEdge(unsigned int i) const
{
    return back_edges[i];
}

/** @returns The number of edges entering this CycleNode */
unsigned int CycleNode::getNumBackEdges() const
{
    return back_edges.size();
}

/**
 * @brief Remove an element from a vector
 * @param v The vector
 * @param n The element to remove
 * @return True if the element was found; false otherwise
 */
template <typename T>
static bool vector_remove_node(SnapVector<T>& v, const T n)
{
    for (unsigned int i = 0; i < v.size(); i++) {
        if (v[i] == n) {
            v.erase(v.begin() + i);
            return true;
        }
    }
    return false;
}

/**
 * @brief Remove a (forward) edge from this CycleNode
 * @return The CycleNode which was popped, if one exists; otherwise NULL
 */
CycleNode * CycleNode::removeEdge()
{
    if (edges.empty())
        return NULL;

    CycleNode *ret = edges.back();
    edges.pop_back();
    vector_remove_node(ret->back_edges, this);
    return ret;
}

/**
 * @brief Remove a (back) edge from this CycleNode
 * @return The CycleNode which was popped, if one exists; otherwise NULL
 */
CycleNode * CycleNode::removeBackEdge()
{
    if (back_edges.empty())
        return NULL;

    CycleNode *ret = back_edges.back();
    back_edges.pop_back();
    vector_remove_node(ret->edges, this);
    return ret;
}

/**
 * Adds an edge from this CycleNode to another CycleNode.
 * @param node The node to which we add a directed edge
 * @return True if this edge is a new edge; false otherwise
 */
bool CycleNode::addEdge(CycleNode *node)
{
    for (unsigned int i = 0; i < edges.size(); i++)
        if (edges[i] == node)
            return false;
    edges.push_back(node);
    node->back_edges.push_back(this);
    return true;
}

/** @returns the RMW CycleNode that reads from the current CycleNode */
CycleNode * CycleNode::getRMW() const
{
    return hasRMW;
}

/**
 * Set a RMW action node that reads from the current CycleNode.
 * @param node The RMW that reads from the current node
 * @return True, if this node already was read by another RMW; false otherwise
 * @see CycleGraph::addRMWEdge
 */
bool CycleNode::setRMW(CycleNode *node)
{
    if (hasRMW != NULL)
        return true;
    hasRMW = node;
    return false;
}

/**
 * Convert a Promise CycleNode into a concrete-valued CycleNode. Should only be
 * used when there's no existing ModelAction CycleNode for this write.
 *
 * @param writer The ModelAction which wrote the future value represented by
 * this CycleNode
 */
void CycleNode::resolvePromise(const ModelAction *writer)
{
    ASSERT(is_promise());
    ASSERT(promise->is_compatible(writer));
    action = writer;
    promise = NULL;
    ASSERT(!is_promise());
}
