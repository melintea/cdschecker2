#include <algorithm> 
#include "specannotation.h"
#include "cdsannotate.h"

/**********    Annotations    **********/

SpecAnnotation::SpecAnnotation(SpecAnnoType type, void *anno) : type(type),
	annotation(anno) { }
		

CommutativityRule::CommutativityRule(CSTR method1, CSTR method2, CSTR rule,
	CheckCommutativity_t condition) : method1(method1),
	method2(method2), rule(rule), condition(condition) {}

bool CommutativityRule::isRightRule(Method m1, Method m2) {
	return (m1->interfaceName == method1 && m2->interfaceName == method2) ||
		(m1->interfaceName == method2 && m2->interfaceName == method1);
}
	
bool CommutativityRule::checkCondition(Method m1, Method m2) {
	if (m1->interfaceName == method1 && m2->interfaceName == method2)
		return (*condition)(m1, m2);
	else if (m1->interfaceName == method2 && m2->interfaceName == method1)
		return (*condition)(m2, m1);
	else // The checking should only be called on the right rule
		MODEL_ASSERT(false);
		return false;
}


StateFunctions::StateFunctions(CSTR name, StateTransition_t transition, UpdateState_t
	evaluateState, CheckState_t preCondition, UpdateState_t sideEffect,
	CheckState_t postCondition) : name(name), transition(transition),
	evaluateState(evaluateState), preCondition(preCondition),
	sideEffect(sideEffect), postCondition(postCondition) {}


AnnoInit::AnnoInit(UpdateState_t initial, CheckState_t final, CopyState_t copy,
	CommutativityRule *commuteRules, int ruleNum)
	: initial(initial), final(final), copy(copy),
	commuteRules(commuteRules), commuteRuleNum(ruleNum) {
	funcMap = new Map<CSTR, StateFunctions*>;
}
	
AnnoInit::AnnoInit(UpdateState_t initial, CopyState_t copy, CommutativityRule
	*commuteRules, int ruleNum) :
	initial(initial), final(NULL), copy(copy), commuteRules(commuteRules),
	commuteRuleNum(ruleNum) {
	funcMap = new Map<CSTR, StateFunctions*>;
}

void AnnoInit::addInterfaceFunctions(CSTR name, StateFunctions *funcs) {
	funcMap->insert(make_pair(name, funcs));
}


AnnoPotentialOP::AnnoPotentialOP(CSTR label) : label(label) {}

AnnoOPCheck::AnnoOPCheck(CSTR label) : label(label) {}



/**********    Universal functions for rewriting the program    **********/

Method _createInterfaceBeginAnnotation(CSTR name) {
	Method cur = (MethodCall*) snapshot_malloc(sizeof(MethodCall));
	new(cur)MethodCall(name);
	// Create and instrument with the INTERFACE_BEGIN annotation
	SpecAnnotation *anno = (SpecAnnotation*) snapshot_malloc(sizeof(SpecAnnotation));
	new(anno)SpecAnnotation(INTERFACE_BEGIN, cur);
	cdsannotate(SPEC_ANALYSIS, anno);
	return cur;
}

void _createOPDefineAnnotation() {
	SpecAnnotation *anno = (SpecAnnotation*) snapshot_malloc(sizeof(SpecAnnotation));
	new(anno)SpecAnnotation(OP_DEFINE, NULL);
	cdsannotate(SPEC_ANALYSIS, anno);
}

void _createPotentialOPAnnotation(CSTR label) {
	SpecAnnotation *anno = (SpecAnnotation*) snapshot_malloc(sizeof(SpecAnnotation));
	AnnoPotentialOP *pop= (AnnoPotentialOP*) snapshot_malloc(sizeof(AnnoPotentialOP));
	new(pop)AnnoPotentialOP(label);
	new(anno)SpecAnnotation(POTENTIAL_OP, pop);
	cdsannotate(SPEC_ANALYSIS, anno);
}

void _createOPCheckAnnotation(CSTR label) {
	SpecAnnotation *anno = (SpecAnnotation*) snapshot_malloc(sizeof(SpecAnnotation));
	AnnoOPCheck *opCheck= (AnnoOPCheck*) snapshot_malloc(sizeof(AnnoOPCheck));
	new(opCheck)AnnoOPCheck(label);
	new(anno)SpecAnnotation(OP_CHECK, opCheck);

	cdsannotate(SPEC_ANALYSIS, anno);
}

void _createOPClearAnnotation() {
	SpecAnnotation *anno = (SpecAnnotation*) snapshot_malloc(sizeof(SpecAnnotation));
	new(anno)SpecAnnotation(OP_CLEAR, NULL);
	cdsannotate(SPEC_ANALYSIS, anno);
}

void _createOPClearDefineAnnotation() {
	SpecAnnotation *anno = (SpecAnnotation*) snapshot_malloc(sizeof(SpecAnnotation));
	new(anno)SpecAnnotation(OP_CLEAR_DEFINE, NULL);
	cdsannotate(SPEC_ANALYSIS, anno);
}
