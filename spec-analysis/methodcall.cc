#include <algorithm>
#include "common.h"
#include "threads-model.h"
#include "methodcall.h"
#include "spec_common.h"

CSTR GRAPH_START = "START_NODE";
CSTR GRAPH_FINISH = "FINISH_NODE";

const unsigned int METHOD_ID_MAX = 0xffffffff;
const unsigned int METHOD_ID_MIN = 0;

MethodCall::MethodCall(CSTR name, void *value, ModelAction *begin) :
    name(name), value(value), prev(new SnapSet<Method>), next(new
    SnapSet<Method>), concurrent(new SnapSet<Method>), justifiedMethod(NULL),
    justified(false), end(NULL), orderingPoints(new action_list_t), allPrev(new
    SnapSet<Method>), allNext(new SnapSet<Method>), exist(true) {
    if (name == GRAPH_START) {
        this->begin = NULL;
        id = METHOD_ID_MIN;
        tid = 1; // Considered to be the main thread
    } else if (name == GRAPH_FINISH) {
        this->begin = NULL;
        id = METHOD_ID_MAX;
        tid = 1; // Considered to be the main thread
    } else {
        this->begin = begin;
        ASSERT (begin);
        id = begin->get_seq_number();
        tid = id_to_int(begin->get_tid());
    }
}
    
void MethodCall::addPrev(Method m) { prev->insert(m); }

void MethodCall::addNext(Method m) { next->insert(m); }
    
void MethodCall::addConcurrent(Method m) { concurrent->insert(m); }

void MethodCall::addOrderingPoint(ModelAction *act) {
    bool hasIt = orderingPoints->end() != std::find(orderingPoints->begin(),
        orderingPoints->end(), act);
    if (!hasIt)
        orderingPoints->push_back(act);
}


bool MethodCall::before(Method another) {
    return belong(allNext, another);
}

bool MethodCall::belong(MethodSet s, Method m) {
    return s->end() != std::find(s->begin(), s->end(), m);
}

bool MethodCall::identical(MethodSet s1, MethodSet s2) {
    if (s1->size() != s2->size())
        return false;
    SnapSet<Method>::iterator it;
    for (it = s1->begin(); it != s1->end(); it++) {
        Method m1 = *it;
        if (belong(s2, m1))
            return false;
    }
    return true;
}

/**
    Put the union of src and dest to dest.
*/
void MethodCall::Union(MethodSet dest, MethodSet src) {
    SnapSet<Method>::iterator it;
    for (it = src->begin(); it != src->end(); it++) {
        Method m = *it;
        dest->insert(m);
    }
}

MethodSet MethodCall::intersection(MethodSet s1, MethodSet s2) {
    MethodSet res = new SnapSet<Method>;
    SnapSet<Method>::iterator it;
    for (it = s1->begin(); it != s1->end(); it++) {
        Method m = *it;
        if (belong(s2, m))
            res->insert(m);
    }
    return res;
}

bool MethodCall::disjoint(MethodSet s1, MethodSet s2) {
    SnapSet<Method>::iterator it;
    for (it = s1->begin(); it != s1->end(); it++) {
        Method m = *it;
        if (belong(s2, m))
            return false;
    }
    return true;
}

void MethodCall::print(bool printOP, bool breakAtEnd) {
    if (name == GRAPH_START) {
        model_print("%s", GRAPH_START);
        if (breakAtEnd)
            model_print("\n");
        return;
    }
    if (name == GRAPH_FINISH) {
        model_print("%s", GRAPH_FINISH);
        if (breakAtEnd)
            model_print("\n");
        return;
    }
    model_print("%u.%s(T%d)", id, name, id_to_int(begin->get_tid()));
    if (breakAtEnd)
        model_print("\n");
    if (!printOP)
        return;
    int i = 1;
    for (action_list_t::iterator it = orderingPoints->begin(); it !=
        orderingPoints->end(); it++) {
        ModelAction *op = *it;
        model_print("\t-> %d. ", i++);
        op->print();
    }
}
