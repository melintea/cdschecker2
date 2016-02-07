#ifndef _METHODCALL_H
#define _METHODCALL_H

#include <string> 
#include "execution.h"
#include "action.h"
#include "spec_common.h"

using std::string;

class MethodCall;

typedef MethodCall *Method;
typedef SnapSet<Method> *MethodSet;

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
	string name; // The interface label name
	void *value; // The pointer that points to the struct that have the return
				 // value and the arguments
	void *state; // The pointer that points to the struct that represents
					  // the state
	MethodSet prev; // Method calls that are hb right before me
	MethodSet next; // Method calls that are hb right after me
	MethodSet concurrent; // Method calls that are concurrent with me

	MethodCall(string name, void *value = NULL, ModelAction *begin = NULL);
	
	void addPrev(Method m);

	void addNext(Method m);
	
	void addConcurrent(Method m);

	void addOrderingPoint(ModelAction *act);

	static bool belong(MethodSet s, Method m);

	static MethodSet intersection(MethodSet s1, MethodSet s2);

	static bool disjoint(MethodSet s1, MethodSet s2);

	void print();
	
	SNAPSHOTALLOC

	/**
		FIXME: The end action is not really used or necessary here, maybe we
		should clean this
	*/
	ModelAction *begin, *end;
	action_list_t *orderingPoints;
	
	/**
		Keep a set of all methods that are hb/SC before&after me to calculate my
		state
	*/
	MethodSet allPrev, allNext;

	/** Logically exist (for generating all possible topological sortings) */
	bool exist;
};



#endif
