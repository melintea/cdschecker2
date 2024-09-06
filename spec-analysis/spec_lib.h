#ifndef _SPEC_LIB_H
#define _SPEC_LIB_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

/* This file defines the basic sequential data structures that can be used by
 * the specification
 */

typedef uint64_t call_id_t;

/* Sequential List */
struct spec_list;
struct list_node;

typedef struct list_node {
    struct list_node *prev;
    struct list_node *next;
    void *data;
} list_node;

typedef struct spec_list {
    list_node *head;
    list_node *tail;
    int size;
} spec_list;


spec_list* init_spec_list();

void free_spec_list(spec_list *list);

void* front(spec_list *list);

void* back(spec_list *list);

void push_back(spec_list *list, void *elem);

void push_front(spec_list *list, void *elem);

int size(spec_list *list);

bool pop_back(spec_list *list);

bool pop_front(spec_list *list);

/* Hash Table */

#endif
