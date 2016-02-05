#ifndef _SPECANNOTATION_H
#define _SPECANNOTATION_H

#include "modeltypes.h"
#include "model-assert.h"
#include "methodcall.h"
#include "action.h"
#include "spec_common.h"

using namespace std;

/** 
	We can only pass a void* pointer from the real program execution to the
	checking engine, so we need to wrap the key information into that pointer.
	We use the SpecAnntotation struct to represent that info. Different types
	here mean different annotations.

	FIXME: Currently we actually do not need to have the INTERFACE_END types. We
	basically wrap the MethodCall* pointer of the method call in the
	INTERFACE_BEGIN type of annotation.
*/
typedef enum SpecAnnoType {
	INIT, POTENTIAL_OP, OP_DEFINE, OP_CHECK, OP_CLEAR, OP_CLEAR_DEFINE,
	INTERFACE_BEGIN, INTERFACE_END
} SpecAnnoType; 

typedef
struct SpecAnnotation {
	SpecAnnoType type;
	void *annotation;

	SpecAnnotation(SpecAnnoType type, void *anno);

} SpecAnnotation;

typedef bool (*CheckCommutativity_t)(Method, Method);
/**
	The first method is the target (to update its state), and the second method
	is the method that should be executed (to access its method call info (ret
	& args)
*/
typedef void (*StateTransition_t)(Method, Method);
typedef bool (*CheckState_t)(Method);
typedef void (*UpdateState_t)(Method);
// Copy the second state to the first state
typedef void (*CopyState_t)(Method, Method);

/**
	This struct contains a commutativity rule: two method calls represented by
	two unique integers and a function that takes two "info" pointers (with the
	return value and the arguments) and returns a boolean to represent whether
	the two method calls are commutable.
*/
typedef
struct CommutativityRule {
	CSTR method1;
	CSTR method2;

	/** The plain text of the rule (debugging purpose) */
	CSTR rule;
	CheckCommutativity_t condition;

	CommutativityRule(CSTR method1, CSTR method2, CSTR rule,
		CheckCommutativity_t condition);

	bool isRightRule(Method m1, Method m2);
	
	bool checkCondition(Method m1, Method m2);

} CommutativityRule;

typedef
struct StateFunctions {
	CSTR name;
	StateTransition_t transition;
	UpdateState_t evaluateState;
	CheckState_t preCondition;
	UpdateState_t sideEffect;
	CheckState_t postCondition;

	StateFunctions(CSTR name, StateTransition_t transition, UpdateState_t
		evaluateState, CheckState_t preCondition, UpdateState_t sideEffect,
		CheckState_t postCondition);

} StateFunctions;


typedef
struct AnnoInit {
	/**
		For the initialization of a state; We actually assume there are two
		special nodes --- INITIAL & FINAL. They actually have a special
		MethodCall class representing these two nodes, and their interface name
		would be INITIAL and FINAL. For the initial function, we actually also
		use it as the state copy function when evaluating the state of other
		method calls
	*/
	UpdateState_t initial;

	/**
		TODO: Currently we just have this "final" field, which was supposed to
		be a final checking function after all method call nodes have been
		executed on the graph. However, before we think it through, this might
		potentially be a violation of composability
	*/
	CheckState_t final;
	
	/**
		For copying out an existing state from a previous method call
	*/
	CopyState_t copy;

	/** For the state functions. We can conveniently access to the set of state
	 *  functions with a hashmap */
	Map<CSTR, StateFunctions*> *funcMap;
	
	/** For commutativity rules */
	CommutativityRule *commuteRules;
	int commuteRuleNum;

	AnnoInit(UpdateState_t initial, CheckState_t final, CopyState_t copy,
		CommutativityRule *commuteRules, int ruleNum);
			
	void addInterfaceFunctions(CSTR name, StateFunctions *funcs);

} AnnoInit;

typedef
struct AnnoInterfaceInfo {
	CSTR name;
	void *value;

	AnnoInterfaceInfo(CSTR name); 
} AnnoInterfaceInfo;

/**********    Universal functions for rewriting the program    **********/

AnnoInterfaceInfo* _createInterfaceBeginAnnotation(CSTR name);

void _createOPDefineAnnotation();

void _createPotentialOPAnnotation(CSTR label);

void _createOPCheckAnnotation(CSTR label);

void _createOPClearAnnotation();

void _createOPClearDefineAnnotation();

#endif
