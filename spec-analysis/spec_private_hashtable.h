/** @file spec_hashtable.h
 *  @brief Hashtable.  Standard chained bucket variety. Stealed from Brian
 *  Norris's model checker.
 */

#ifndef _SPEC_PRIVATE_HASHTABLE_H__
#define _SPEC_PRIVATE_HASHTABLE_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <modeltypes.h>

/**
 * @brief HashTable node
 */
typedef struct hashlistnode {
	thread_t key;
	uint64_t val;
} hashlistnode;

/**
 * @brief A simple, custom hash table
 *
 */
typedef struct spec_private_hashtable {
	hashlistnode *table;
	unsigned int capacity;
	unsigned int size;
	unsigned int capacitymask;
	unsigned int threshold;
	double loadfactor;
} spec_private_hashtable;


/**
 * @brief Hash table constructor
 * @param initialcapacity Sets the initial capacity of the hash table.
 * Default size 1024.
 * @param factor Sets the percentage full before the hashtable is
 * resized. Default ratio 0.5.
 */
void init_table(spec_private_hashtable *table, unsigned int initialcapacity = 1024, double factor = 0.5) {
	// Allocate space for the hash table
	table = (struct hashlistnode<_Key, _Val> *)_calloc(initialcapacity, sizeof(struct hashlistnode<_Key, _Val>));
	loadfactor = factor;
	capacity = initialcapacity;
	capacitymask = initialcapacity - 1;

	threshold = (unsigned int)(initialcapacity * loadfactor);
	size = 0; // Initial number of elements in the hash
}


/** @brief Reset the table to its initial state. */
void reset(spec_private_hashtable *table) {
	memset(table, 0, capacity * sizeof(struct hashlistnode<_Key, _Val>));
	size = 0;
}


/**
 * @brief Put a key/value pair into the table
 * @param key The key for the new value; must not be 0 or NULL
 * @param val The value to store in the table
 */
void put(spec_private_hashtable *table, thread_id_t key, uint64_t val) {
	/* HashTable cannot handle 0 as a key */
	ASSERT(key);

	if (size > threshold)
		resize(capacity << 1);

	struct hashlistnode<_Key, _Val> *search;

	unsigned int index = ((_KeyInt)key) >> _Shift;
	do {
		index &= capacitymask;
		search = &table[index];
		if (search->key == key) {
			search->val = val;
			return;
		}
		index++;
	} while (search->key);

	search->key = key;
	search->val = val;
	size++;
}


/**
 * @brief Lookup the corresponding value for the given key
 * @param key The key for finding the value; must not be 0 or NULL
 * @return The value in the table, if the key is found; otherwise 0
 */
uint64_t get(spec_private_hashtable *table, thread_id_t key) {
	struct hashlistnode<_Key, _Val> *search;

	/* HashTable cannot handle 0 as a key */
	ASSERT(key);

	unsigned int index = ((_KeyInt)key) >> _Shift;
	do {
		index &= capacitymask;
		search = &table[index];
		if (search->key == key)
			return search->val;
		index++;
	} while (search->key);
	return (_Val)0;
}

/**
 * @brief Check whether the table contains a value for the given key
 * @param key The key for finding the value; must not be 0 or NULL
 * @return True, if the key is found; false otherwise
 */
bool contains(spec_private_hashtable *table, thread_id_t key) {
	struct hashlistnode<_Key, _Val> *search;

	/* HashTable cannot handle 0 as a key */
	ASSERT(key);

	unsigned int index = ((_KeyInt)key) >> _Shift;
	do {
		index &= capacitymask;
		search = &table[index];
		if (search->key == key)
			return true;
		index++;
	} while (search->key);
	return false;
}

/**
 * @brief Resize the table
 * @param newsize The new size of the table
 */
void resize(spec_private_hashtable *table, unsigned int newsize) {
	struct hashlistnode<_Key, _Val> *oldtable = table;
	struct hashlistnode<_Key, _Val> *newtable;
	unsigned int oldcapacity = capacity;

	if ((newtable = (struct hashlistnode<_Key, _Val> *)_calloc(newsize, sizeof(struct hashlistnode<_Key, _Val>))) == NULL) {
		model_print("calloc error %s %d\n", __FILE__, __LINE__);
		exit(EXIT_FAILURE);
	}

	table = newtable;          // Update the global hashtable upon resize()
	capacity = newsize;
	capacitymask = newsize - 1;

	threshold = (unsigned int)(newsize * loadfactor);

	struct hashlistnode<_Key, _Val> *bin = &oldtable[0];
	struct hashlistnode<_Key, _Val> *lastbin = &oldtable[oldcapacity];
	for (; bin < lastbin; bin++) {
		_Key key = bin->key;

		struct hashlistnode<_Key, _Val> *search;

		unsigned int index = ((_KeyInt)key) >> _Shift;
		do {
			index &= capacitymask;
			search = &table[index];
			index++;
		} while (search->key);

		search->key = key;
		search->val = bin->val;
	}

	_free(oldtable);            // Free the memory of the old hash table
}

#endif
