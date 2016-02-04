#ifndef _SPECANNOTATION_H
#define _SPECANNOTATION_H

#include <unordered_map>
#include <utility>
#include "modeltypes.h"
#include "model-assert.h"
#include "methodcall.h"
#include "action.h"
#include "cdsannotate.h"

#define SPEC_ANALYSIS 1

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

	SpecAnnotation(SpecAnnoType type, void *anno) : type(type), annotation(anno) {
		
	}

	/** 
		A static function for others to use. To extract the actual annotation
		pointer and return the actual annotation if this is an annotation action;
		otherwise return NULL.
	*/
	static SpecAnnotation* getAnnotation(ModelAction *act) {
		if (act->get_type() != ATOMIC_ANNOTATION)
			return NULL;
		if (act->get_value() != SPEC_ANALYSIS)
			return NULL;
		SpecAnnotation *anno = (SpecAnnotation*) act->get_location();
		MODEL_ASSERT (anno);
		return anno;
	}

	SNAPSHOTALLOC
	
} SpecAnnotation ;

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
	string method1;
	string method2;

	/** The plain text of the rule (debugging purpose) */
	string rule;
	CheckCommutativity_t condition;

	CommutativityRule(string method1, string method2, string rule,
		CheckCommutativity_t condition) : method1(method1),
		method2(method2), rule(rule), condition(condition) {}

	CommutativityRule() {}
	
	bool isRightRule(Method m1, Method m2) {
		return (m1->interfaceName == method1 && m2->interfaceName == method2) ||
			(m1->interfaceName == method2 && m2->interfaceName == method1);
	}
	
	bool checkCondition(Method m1, Method m2) {
		if (m1->interfaceName == method1 && m2->interfaceName == method2)
			return (*condition)(m1, m2);
		else if (m1->interfaceName == method2 && m2->interfaceName == method1)
			return (*condition)(m2, m1);
		else // The checking should only be called on the right rule
			MODEL_ASSERT(false);
			return false;
	}

	SNAPSHOTALLOC
} CommutativityRule;

typedef
struct StateFunctions {
	string name;
	StateTransition_t transition;
	UpdateState_t evaluateState;
	CheckState_t preCondition;
	UpdateState_t sideEffect;
	CheckState_t postCondition;

	StateFunctions(string name, StateTransition_t transition, UpdateState_t
		evaluateState, CheckState_t preCondition, UpdateState_t sideEffect,
		CheckState_t postCondition) : name(name), transition(transition),
		evaluateState(evaluateState), preCondition(preCondition),
		sideEffect(sideEffect), postCondition(postCondition) {}

	SNAPSHOTALLOC
} StateFunctions;

template<typename Key, typename T>
class SnapMap : public std::unordered_map<Key, T, hash<Key>, equal_to<Key>,
	SnapshotAlloc< pair<const Key, T> > >
{
	public:
	typedef std::unordered_map< Key, T, hash<Key>, equal_to<Key>,
		SnapshotAlloc<pair<const Key, T> > > map;

	SnapMap() : map() { }

	SNAPSHOTALLOC
};

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
	SnapMap<string, StateFunctions*> *funcMap;
	
	/** For commutativity rules */
	CommutativityRule *commuteRules;
	int commuteRuleNum;

	AnnoInit(UpdateState_t initial, CheckState_t final, CopyState_t copy,
		CommutativityRule *commuteRules, int ruleNum)
		: initial(initial), final(final), copy(copy),
		commuteRules(commuteRules), commuteRuleNum(ruleNum) {
		funcMap = new SnapMap<string, StateFunctions*>;
	}
	
	AnnoInit(UpdateState_t initial, CopyState_t copy, CommutativityRule
		*commuteRules, int ruleNum) :
		initial(initial), final(NULL), copy(copy), commuteRules(commuteRules),
		commuteRuleNum(ruleNum) {
		funcMap = new SnapMap<string, StateFunctions*>;
	}

	void addInterfaceFunctions(string name, StateFunctions *funcs) {
		funcMap->insert(make_pair(name, funcs));
	}

	SNAPSHOTALLOC
} AnnoInit;

typedef
struct AnnoPotentialOP {
	string label;

	AnnoPotentialOP(string label) : label(label) {}

	SNAPSHOTALLOC

} AnnoPotentialOP;

typedef
struct AnnoOPCheck {
	string label;

	AnnoOPCheck(string label) : label(label) {}

	SNAPSHOTALLOC
} AnnoOPCheck;


/**********    Universal functions for rewriting the program    **********/

inline Method _createInterfaceBeginAnnotation(string name) {
	Method cur = new MethodCall(name);
	// Create and instrument with the INTERFACE_BEGIN annotation
	cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(INTERFACE_BEGIN, cur));
	return cur;
}

inline void _createOPDefineAnnotation() {
	cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(OP_DEFINE, NULL));
}

inline void _createPotentialOPAnnotation(string label) {
	cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(POTENTIAL_OP, new
	AnnoPotentialOP(label)));
}

inline void _createOPCheckAnnotation(string label) {
	cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(OP_CHECK, new
	AnnoOPCheck(label)));
}

inline void _createOPClearAnnotation() {
	cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(OP_CLEAR, NULL));
}

inline void _createOPClearDefineAnnotation() {
	cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(OP_CLEAR_DEFINE, NULL));
}

#endif
