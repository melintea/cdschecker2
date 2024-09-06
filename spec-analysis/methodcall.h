#ifndef _METHODCALL_H
#define _METHODCALL_H

#include "stl-model.h"
#include "action.h"
#include "spec_common.h"

class MethodCall;

typedef MethodCall *Method;
typedef SnapSet<Method> *MethodSet;
typedef SnapList<ModelAction *> action_list_t;

typedef SnapList<Method> MethodList;
typedef SnapVector<Method> MethodVector;
typedef SnapVector<MethodList*> MethodListVector;

/**
    This is the core class on which the whole checking process will be
    executing. With the original execution (with the raw annotation
    information), we construct an execution graph whose nodes represent method
    calls, and whose edges represent the hb/SC relation between the ordering
    points of method calls. In that graph, the MethodCall class acts as the node
    that contains the core information for checking --- the name of the
    interface, the value (return value and arguments), the state of the current
    method call, and a set of previous, next and concurrent method calls.

    Plus, this class contains extra info about the ordering points, a set of all
    method calls that are hb/SC before me (for evaluating the state), and also
    some other helping internal member variable for generating checking schemes.
*/
class MethodCall {
    public:
    unsigned int id; // The method call id (the seq_num of the begin action)
    int tid; // The thread id
    CSTR name; // The interface label name
    void *value; // The pointer that points to the struct that have the return
                 // value and the arguments
    void *state; // The pointer that points to the struct that represents
                      // the state
    MethodSet prev; // Method calls that are hb right before me
    MethodSet next; // Method calls that are hb right after me
    MethodSet concurrent; // Method calls that are concurrent with me

    Method justifiedMethod; // The method before me and is not concurrent with
                            // any other mehtods in my allPrev set

    /** 
        This indicates if the non-deterministic behaviors of this method call
        has been justified by any of its justifying subhistory. If so, we do not
        need to check on its justifying subhistory. Initially it is false.
    */
    bool justified; 

    MethodCall(CSTR name, void *value = NULL, ModelAction *begin = NULL);
    
    void addPrev(Method m);

    void addNext(Method m);
    
    void addConcurrent(Method m);

    void addOrderingPoint(ModelAction *act);

    bool before(Method another);

    static bool belong(MethodSet s, Method m);

    static bool identical(MethodSet s1, MethodSet s2);

    /** Put the union of src and dest to dest */
    static void Union(MethodSet dest, MethodSet src);

    static MethodSet intersection(MethodSet s1, MethodSet s2);

    static bool disjoint(MethodSet s1, MethodSet s2);

    /**
        Print the method all name with the seq_num of the begin annotation and
        its thread id.
    
        printOP == true -> Add each line with the ordering point operation's
        print()

        breakAtEnd == true -> Add a line break at the end of the print;
        otherwise, the print will be a string without line breaker when printOP
        is false.
    */
    void print(bool printOP = true, bool breakAtEnd = true);
    
    /**
        FIXME: The end action is not really used or necessary here, maybe we
        should clean this
    */
    ModelAction *begin;
    ModelAction *end;
    action_list_t *orderingPoints;
    
    /**
        Keep a set of all methods that are hb/SC before&after me to calculate my
        state
    */
    MethodSet allPrev;
    MethodSet allNext;

    /** Logically exist (for generating all possible topological sortings) */
    bool exist;

    SNAPSHOTALLOC
};



#endif
