#include "spec_lib.h"
#include "common.h"
#include "stl-model.h"
#include "model_memory.h"
#include "modeltypes.h"
#include "hashtable.h"

#include <algorithm> 
#include <iterator> 

typedef SnapList<int> StdIntList;
typedef HashTable<int, int, int, 4 > MyIntTable;

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

int front(IntegerList *list) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	if (stdList->empty())
		return 0;
	return stdList->front();
}

int back(IntegerList *list) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	if (stdList->empty())
		return 0;
	return stdList->back();
}

void push_back(IntegerList *list, int elem) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	stdList->push_back(elem);
}

void push_front(IntegerList *list, int elem) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	stdList->push_front(elem);
}

int size(IntegerList *list) {
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

int elem_at_index(IntegerList *list, int idx) {
	StdIntList *stdList = (StdIntList*) list->ptr;
	StdIntList::iterator it = stdList->begin();
	for (int i = 0; i < idx; i++) {
		it++;
		if (it == stdList->end())
			break;
	}
	if (it == stdList->end())
		return 0;
	else
		return *it;
}

bool has_elem(IntegerList *list, int elem) {
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

int getIntegerMap(IntegerMap *m, int key) {
	ASSERT (key);
	MyIntTable *table = (MyIntTable*) m->ptr;
	return table->get(key);
}

int putIntegerMap(IntegerMap *m, int key, int value) {
	ASSERT (key);
	MyIntTable *table = (MyIntTable*) m->ptr;
	int old = table->get(key);
	table->put(key, value);
	return old;
}

/* End of sequential hashtable */
