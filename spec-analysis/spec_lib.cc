#include "spec_lib.h"
#include "common.h"
#include "stl-model.h"
#include "model_memory.h"
#include "modeltypes.h"
#include "hashtable.h"

#include <algorithm> 
#include <iterator> 

typedef SnapList<int64_t> StdIntList;
typedef HashTable<int64_t, int64_t, int64_t, 4 > MyIntTable;

/* Sequential List, wrap with a C++ std list of integer */

IntegerList* createIntegerList() {
	StdIntList *stdList = new StdIntList;
	IntegerList* list = (IntegerList*) MODEL_MALLOC(sizeof(IntegerList));
	list->ptr = stdList;
	return list;
}

void destroyIntegerList(IntegerList *list) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	delete stdList;
	MODEL_FREE(list);
}

int64_t front(IntegerList *list) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	if (stdList->empty())
		return 0;
	return stdList->front();
}

int64_t back(IntegerList *list) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	if (stdList->empty())
		return 0;
	return stdList->back();
}

void push_back(IntegerList *list, int64_t elem) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	stdList->push_back(elem);
}

void push_front(IntegerList *list, int64_t elem) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	stdList->push_front(elem);
}

int64_t size(IntegerList *list) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	return stdList->size();
}

bool pop_back(IntegerList *list) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	if (stdList->empty())
		return false;
	stdList->pop_back();
	return true;
}

bool pop_front(IntegerList *list) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	if (stdList->empty())
		return false;
	stdList->pop_front();
	return true;
}

int64_t elem_at_index(IntegerList *list, int64_t idx) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	StdIntList::iterator it = stdList->begin();
	for (int64_t i = 0; i < idx; i++) {
		it++;
		if (it == stdList->end())
			break;
	}
	if (it == stdList->end())
		return 0;
	else
		return *it;
}


void do_by_value(IntegerList *list, voidFunc_t _func) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	StdIntList::iterator it = stdList->begin();
	for (; it != stdList->end(); it++) {
		int64_t cur = *it;
		(*_func)(cur);
	}
}

bool has_elem_by_value(IntegerList *list, int64_t ptr, equalFunc_t _eq) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	StdIntList::iterator it = stdList->begin();
	for (; it != stdList->end(); it++) {
		int64_t cur = *it;
		if ((*_eq)(cur, ptr))
			return true;
	}
	return false;
}

bool has_elem(IntegerList *list, int64_t elem) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	StdIntList::iterator it = std::find(stdList->begin(), stdList->end(), elem);
	return it != stdList->end();
}
/* End of sequential list */



/* Sequential hashtable */

IntegerMap* createIntegerMap() {
	MyIntTable *table = new MyIntTable;
	IntegerMap* map = (IntegerMap*) MODEL_MALLOC(sizeof(IntegerMap));
	map->ptr = table;
	return map;
}

void destroyIntegerMap(IntegerMap *m) {
	MyIntTable *table = (MyIntTable*) m->ptr;
	delete table;
	MODEL_FREE(m);
}

int64_t getIntegerMap(IntegerMap *m, int64_t key) {
	ASSERT (key);
	MyIntTable *table = (MyIntTable*) m->ptr;
	return table->get(key);
}

int64_t putIntegerMap(IntegerMap *m, int64_t key, int64_t value) {
	ASSERT (key);
	MyIntTable *table = (MyIntTable*) m->ptr;
	int64_t old = table->get(key);
	table->put(key, value);
	return old;
}

/* End of sequential hashtable */
