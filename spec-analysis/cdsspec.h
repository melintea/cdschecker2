#ifndef _CDSSPEC_H
#define _CDSSPEC_H

#include <vector>
#include <list>
#include <string>
#include <iterator>
#include <algorithm>
#include <set>
#include <unordered_map>

#include <functional>

#include <stdarg.h>

#include "mymemory.h"
//#include "common.h"
#include "methodcall.h"

using namespace std;

/** Macro for output (stole from common.h) */
extern int model_out;
#define PRINT(fmt, ...) do { dprintf(model_out, fmt, ##__VA_ARGS__); } while (0)

/**
    A special kind of integer that has been embedded with a universal tag (ID)
*/
typedef struct TagInt {
    unsigned int tag;
    int val;

    TagInt(unsigned int tag, int val) : tag(tag), val(val) { }
    
    TagInt(int val) : tag(0), val(val) { }
}TagInt;

typedef SnapVector<int> IntVector;
typedef SnapList<int> IntList;
typedef SnapSet<int> IntSet;

template<typename Key, typename Value>
class Map: public unordered_map<Key, Value> {
    public:
    typedef unordered_map<Key, Value> map;

    Map() : map() { }

    Value get(Key key) {
        return (*this)[key];
    }

    void put(Key key, Value value) {
        (*this)[key] = value;
    }

    SNAPSHOTALLOC
};

class IntMap : public unordered_map<int, int> {
    public:
    typedef unordered_map<int, int> map;

    IntMap() : map() { }

    int get(int key) {
        return (*this)[key];
    }

    void put(int key, int value) {
        (*this)[key] = value;
    }

    SNAPSHOTALLOC
};

typedef SnapVector<double> DoubleVector;
typedef SnapList<double> DoubleList;
typedef SnapSet<double> DoubleSet;

/********** Debugging functions **********/
template<class Container>
inline void printContainer(Container *container) {
    if (!container || container->size() == 0)
        PRINT("EMPTY");
    for (auto it = container->begin(); it != container->end(); it++) {
        int item = *it;
        PRINT("%d ", item);
    }
}

inline void printMap(IntMap *container) {
    if (!container || container->size() == 0)
        PRINT("EMPTY");
    for (auto it = container->begin(); it != container->end(); it++) {
        pair<int, int> item = *it;
        PRINT("(%d, %d) ", item.first, item.second);
    }
}

/********** More general specification-related types and operations **********/

#define NewMethodSet new SnapSet<Method>

#define CAT(a, b) CAT_HELPER(a, b) /* Concatenate two symbols for macros! */
#define CAT_HELPER(a, b) a ## b
#define X(name) CAT(__##name, __LINE__) /* unique variable */

/**
    This is a generic ForEach primitive for all the containers that support
    using iterator to iterate.
*/
#define ForEach(item, container) \
    auto X(_container) = (container); \
    auto X(iter) = X(_container)->begin(); \
    for (auto item = *X(iter); X(iter) != X(_container)->end(); item = ((++X(iter)) != \
        X(_container)->end()) ? *X(iter) : 0)

/**
    This is a common macro that is used as a constant for the name of specific
    variables. We basically have two usage scenario:
    1. In Subset operation, we allow users to specify a condition to extract a
    subset. In that condition expression, we provide NAME, RET(type), ARG(type,
    field) and STATE(field) to access each item's (method call) information.
    2. In general specification (in pre- & post- conditions and side effects),
    we would automatically generate an assignment that assign the current
    MethodCall* pointer to a variable namedd _M. With this, when we access the
    state of the current method, we use STATE(field) (this is a reference
    for read/write).
*/
#define ITEM _M
#define _M ME

#define S_RET __value->S_RET

/**
    This operation is specifically for Method set. For example, when we want to
    construct an integer set from the state field "x" (which is an
    integer) of the previous set of method calls (PREV), and the new set goes to
    "readSet", we would call "MakeSet(int, PREV, readSet, STATE(x));". Then users
    can use the "readSet" as an integer set (set<int>)
*/
#define MakeSet(type, oldset, newset, field) \
    auto newset = new SnapSet<type>; \
    ForEach (_M, oldset) \
        newset->insert(field); \

/**
    We provide a more general subset operation that takes a plain boolean
    expression on each item (access by the name "ITEM") of the set, and put it
    into a new set if the boolean expression (Guard) on that item holds. This is
    used as the second arguments of the Subset operation. An example to extract
    a subset of positive elements on an IntSet "s" would be "Subset(s,
    GeneralGuard(int, ITEM > 0))"
*/
#define GeneralGuard(type, expression)  std::function<bool(type)> \
    ([&](type ITEM) -> bool { return (expression);}) \

/**
    This is a more specific guard designed for the Method (MethodCall*). It
    basically wrap around the GeneralGuard with the type Method. An example to
    extract the subset of method calls in the PREV set whose state "x" is
    equal to "val" would be "Subset(PREV, Guard(STATE(x) == val))"
*/
#define Guard(expression) GeneralGuard(Method, expression)

#define MostRecent(guard) mostRecent(_M, guard)

/**
    A general subset operation that takes a condition and returns all the item
    for which the boolean guard holds.
*/
inline SnapSet<Method>* mostRecent(Method method, std::function<bool(MethodCall*)> condition) {
    SnapSet<Method> *res = new SnapSet<Method>;
    // A list of potential nodes that are most recent
    MethodList *toCheckList = new MethodList;
    MethodSet prev = method->prev;
    if (prev->empty()) // method is START node
        return res;
    else {
        ForEach (_M, prev)
            toCheckList->push_back(_M);
    }
    // Searching loop
    while (!toCheckList->empty()) {
        Method _M = toCheckList->front();
        toCheckList->pop_front();
        
        if (condition(_M)) {
            bool recencyFlag = true;
            ForEach (m, res) {
                if (MethodCall::belong(_M->allNext, m)) {
                    recencyFlag = false;
                    break;
                }
            }
            if (recencyFlag)
                res->insert(_M);
        }
        else { // Not what we want, keep going up the graph 
            prev = _M->prev;
            if (!prev->empty()) {
                ForEach (_M, prev)
                    toCheckList->push_back(_M);
            }
        }
    }
    return res;
}


/**
    A general subset operation that takes a condition and returns all the item
    for which the boolean guard holds.
*/
template <class T>
inline SnapSet<T>* Subset(SnapSet<T> *original, std::function<bool(T)> condition) {
    SnapSet<T> *res = new SnapSet<T>;
    ForEach (_M, original) {
        if (condition(_M))
            res->insert(_M);
    }
    return res;
}

/**
    A general set operation that takes a condition and returns if there exists
    any item for which the boolean guard holds.
*/
template <class T>
inline bool HasItem(SnapSet<T> *original, std::function<bool(T)> condition) {
    ForEach (_M, original) {
        if (condition(_M))
            return true;
    }
    return false;
}



/**
    A general sublist operation that takes a condition and returns all the item
    for which the boolean guard holds in the same order as in the old list.
*/
template <class T>
inline list<T>* Sublist(list<T> *original, std::function<bool(T)> condition) {
    list<T> *res = new list<T>;
    ForEach (_M, original) {
        if (condition(_M))
            res->push_back(_M);
    }
    return res;
}

/**
    A general subvector operation that takes a condition and returns all the item
    for which the boolean guard holds in the same order as in the old vector.
*/
template <class T>
inline vector<T>* Subvector(vector<T> *original, std::function<bool(T)> condition) {
    vector<T> *res = new vector<T>;
    ForEach (_M, original) {
        if (condition(_M))
            res->push_back(_M);
    }
    return res;
}

/** General for set, list & vector */
#define Size(container) ((container)->size())

#define _BelongHelper(type) \
    template<class T> \
    inline bool Belong(type<T> *container, T item) { \
        return std::find(container->begin(), \
            container->end(), item) != container->end(); \
    }

_BelongHelper(SnapSet)
_BelongHelper(SnapVector)
_BelongHelper(SnapList)

/** General set operations */
template<class T>
inline SnapSet<T>* Intersect(SnapSet<T> *set1, SnapSet<T> *set2) {
    SnapSet<T> *res = new SnapSet<T>;
    ForEach (item, set1) {
        if (Belong(set2, item))
            res->insert(item);
    }
    return res;
}

template<class T>
inline SnapSet<T>* Union(SnapSet<T> *s1, SnapSet<T> *s2) {
    SnapSet<T> *res = new SnapSet<T>(*s1);
    ForEach (item, s2)
        res->insert(item);
    return res;
}

template<class T>
inline SnapSet<T>* Subtract(SnapSet<T> *set1, SnapSet<T> *set2) {
    SnapSet<T> *res = new SnapSet<T>;
    ForEach (item, set1) {
        if (!Belong(set2, item))
            res->insert(item);
    }
    return res;
}

template<class T>
inline void Insert(SnapSet<T> *s, T item) { s->insert(item); }

template<class T>
inline void Insert(SnapSet<T> *s, SnapSet<T> *others) {
    ForEach (item, others)
        s->insert(item);
}

/*
inline MethodSet MakeSet(int count, ...) {
    va_list ap;
    MethodSet res;

    va_start (ap, count);
    res = NewMethodSet;
    for (int i = 0; i < count; i++) {
        Method item = va_arg (ap, Method);
        res->insert(item);
    }
    va_end (ap);
    return res;
}
*/

/********** Method call related operations **********/
#define Id(method) method->id
#define ID Id(_M)

#define Name(method) method->name
#define NAME Name(_M)

#define StructName(type) __struct_ ## type ## __
#define Value(method, type, field) ((StructName(type)*) method->value)->field
#define Ret(method, type) Value(method, type, RET)
#define CRet(method, type) Value(method, type, C_RET)
#define Arg(method, type, arg) Value(method, type, arg)

#define VALUE(type, field) Value(_M, type, field)
#define RET(type) VALUE(type, RET)
#define C_RET(type) VALUE(type, C_RET)
#define ARG(type, arg) VALUE(type, arg)

#define State(method, field) ((StateStruct*) method->state)->field
#define STATE(field) State(_M, field)

#define Prev(method) method->prev
#define PREV _M->prev

#define Next(method) method->next
#define NEXT _M->next

#define Concurrent(method) method->concurrent
#define CONCURRENT  _M->concurrent

#define AllPrev(method) method->allPrev
#define ALLPREV _M->allPrev

#define AllNext(method) method->allNext
#define ALLNEXT _M->allNext 

/***************************************************************************/
/***************************************************************************/

#endif
