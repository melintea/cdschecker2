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
typedef int call_id_t;

#define DEFAULT_CALL_ID 0

/* End of call ID */

/* Sequential List */
typedef struct IntegerList {
	void *ptr;
} IntegerList;

IntegerList* createIntegerList();

void destroyIntegerList(IntegerList *list);

int front(IntegerList *list);

int back(IntegerList *list);

void push_back(IntegerList *list, int elem);

void push_front(IntegerList *list, int elem);

int size(IntegerList *list);

bool pop_back(IntegerList *list);

bool pop_front(IntegerList *list);

int elem_at_index(IntegerList *list, int idx);

bool has_elem(IntegerList *list, int elem);
/* End of sequential list */


/* Sequential hashtable */
typedef struct IntegerMap {
	void *ptr;
} IntegerMap;

IntegerMap* createIntegerMap();

void destroyIntegerMap(IntegerMap *m);

int getIntegerMap(IntegerMap *m, int key);

int putIntegerMap(IntegerMap *m, int key, int value);
	
/* End of hashtable */

#ifdef __cplusplus
} /* extern "C" */
#endif 

#endif
