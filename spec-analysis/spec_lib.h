#ifndef _SPEC_LIB_H
#define _SPEC_LIB_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

/* This file defines the basic sequential data structures that can be used by
 * the specification
 */
#ifdef __cplusplus
extern "C"{
#endif

/* ID of interface call */
typedef int64_t call_id_t;

#define DEFAULT_CALL_ID 0

/* End of call ID */

/* Sequential List */
typedef struct IntegerList {
	void *ptr;
} IntegerList;

IntegerList* createIntegerList();

void destroyIntegerList(IntegerList *list);

int64_t front(IntegerList *list);

int64_t back(IntegerList *list);

void push_back(IntegerList *list, int64_t elem);

void push_front(IntegerList *list, int64_t elem);

int64_t size(IntegerList *list);

bool pop_back(IntegerList *list);

bool pop_front(IntegerList *list);

int64_t elem_at_index(IntegerList *list, int64_t idx);

typedef bool (*voidFunc_t)(int64_t);
void do_by_value(IntegerList *list, voidFunc_t _func);

bool has_elem(IntegerList *list, int64_t elem);

typedef bool (*equalFunc_t)(int64_t, int64_t);

bool has_elem_by_value(IntegerList *list, int64_t elem, equalFunc_t _eq);

/* End of sequential list */


/* Sequential hashtable */
typedef struct IntegerMap {
	void *ptr;
} IntegerMap;

IntegerMap* createIntegerMap();

void destroyIntegerMap(IntegerMap *m);

int64_t getIntegerMap(IntegerMap *m, int64_t key);

int64_t putIntegerMap(IntegerMap *m, int64_t key, int64_t value);
	
/* End of hashtable */

#ifdef __cplusplus
} /* extern "C" */
#endif 

#endif
