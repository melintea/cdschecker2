#include "spec_lib.h"

spec_list* init_spec_list() {
    spec_list *list = (spec_list*) malloc(sizeof(spec_list));
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    return list;
}

void free_spec_list(spec_list *list) {
    list_node *cur = list->head, *next;
    // Free all the nodes
    while (cur != NULL) {
        next = cur->next;
        free(cur);
        cur = next;
    }
    free(list);
}

void* front(spec_list *list) {
    return list->head == NULL ? NULL : list->head->data;
}

void* back(spec_list *list) {
    return list->tail == NULL ? NULL : list->tail->data;
}

void push_back(spec_list *list, void *elem) {
    list_node *new_node = (list_node*) malloc(sizeof(list_node));
    new_node->data = elem;
    new_node->next = NULL;
    new_node->prev = list->tail; // Might be null here for an empty list
    list->size++;
    if (list->tail != NULL) {
        list->tail->next = new_node;
    } else { // An empty list
        list->head = new_node;
    }
    list->tail = new_node;
}

void push_front(spec_list *list, void *elem) {
    list_node *new_node = (list_node*) malloc(sizeof(list_node));
    new_node->data = elem;
    new_node->prev = NULL;
    new_node->next = list->head; // Might be null here for an empty list
    list->size++;
    if (list->head != NULL) {
        list->head->prev = new_node;
    } else { // An empty list
        list->tail = new_node;
    }
    list->head = new_node;
}

int size(spec_list *list) {
    return list->size;
}

bool pop_back(spec_list *list) {
    if (list->size == 0) {
        return false;
    }
    list->size--; // Decrease size first
    if (list->head == list->tail) { // Only 1 element
        free(list->head);
        list->head = NULL;
        list->tail = NULL;
    } else { // More than 1 element
        list_node *to_delete = list->tail;
        list->tail = list->tail->prev;
        free(to_delete); // Don't forget to free the node
    }
    return true;
}

bool pop_front(spec_list *list) {
    if (list->size == 0) {
        return false;
    }
    list->size--; // Decrease size first
    if (list->head == list->tail) { // Only 1 element
        free(list->head);
        list->head = NULL;
        list->tail = NULL;
    } else { // More than 1 element
        list_node *to_delete = list->head;
        list->head = list->head->next;
        free(to_delete); // Don't forget to free the node
    }
    return true;
}
