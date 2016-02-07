#include <algorithm> 
#include "specannotation.h"
#include "cdsannotate.h"

/**********    Annotations    **********/

SpecAnnotation::SpecAnnotation(SpecAnnoType type, void *anno) : type(type),
	annotation(anno) { }
		

CommutativityRule::CommutativityRule(string method1, string method2, string rule,
	CheckCommutativity_t condition) : method1(method1),
	method2(method2), rule(rule), condition(condition) {}

bool CommutativityRule::isRightRule(Method m1, Method m2) {
	return (m1->name == method1 && m2->name == method2) ||
		(m1->name == method2 && m2->name == method1);
}
	
bool CommutativityRule::checkCondition(Method m1, Method m2) {
	if (m1->name == method1 && m2->name == method2)
		return (*condition)(m1, m2);
	else if (m1->name == method2 && m2->name == method1)
		return (*condition)(m2, m1);
	else // The checking should only be called on the right rule
		MODEL_ASSERT(false);
		return false;
}


StateFunctions::StateFunctions(string name, StateTransition_t transition, UpdateState_t
	evaluateState, CheckState_t preCondition, UpdateState_t sideEffect,
	CheckState_t postCondition) : name(name), transition(transition),
	evaluateState(evaluateState), preCondition(preCondition),
	sideEffect(sideEffect), postCondition(postCondition) {}


AnnoInit::AnnoInit(UpdateState_t initial, CheckState_t final, CopyState_t copy,
	CommutativityRule *commuteRules, int ruleNum)
	: initial(initial), final(final), copy(copy),
	commuteRules(commuteRules), commuteRuleNum(ruleNum) {
	funcMap = new Map<string, StateFunctions*>;
}
	

void AnnoInit::addInterfaceFunctions(string name, StateFunctions *funcs) {
	funcMap->insert(make_pair(name, funcs));
}

AnnoInterfaceInfo::AnnoInterfaceInfo(string name) : name(name), value(NULL) { }


/**********    Universal functions for rewriting the program    **********/

AnnoInterfaceInfo* _createInterfaceBeginAnnotation(string name) {
	AnnoInterfaceInfo *info = new AnnoInterfaceInfo(name);
	// Create and instrument with the INTERFACE_BEGIN annotation
	cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(INTERFACE_BEGIN, info));
	return info;
}

void _createOPDefineAnnotation() {
	cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(OP_DEFINE, NULL));
}

void _createPotentialOPAnnotation(string label) {
	cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(POTENTIAL_OP,
		new string(label)));
}

void _createOPCheckAnnotation(string label) {
	cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(OP_CHECK, new string(label)));
}

void _createOPClearAnnotation() {
	cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(OP_CLEAR, NULL));
}

void _createOPClearDefineAnnotation() {
	cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(OP_CLEAR_DEFINE, NULL));
}
