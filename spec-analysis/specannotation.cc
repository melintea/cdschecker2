#include <algorithm> 
#include "specannotation.h"
#include "cdsannotate.h"

/**********    Annotations    **********/

SpecAnnotation::SpecAnnotation(SpecAnnoType type, const void *anno) : type(type),
    annotation(anno) { }
        

CommutativityRule::CommutativityRule(CSTR method1, CSTR method2, CSTR rule,
    CheckCommutativity_t condition) : method1(method1),
    method2(method2), rule(rule), condition(condition) {}

bool CommutativityRule::isRightRule(Method m1, Method m2) {
    return (m1->name == method1 && m2->name == method2) ||
        (m1->name == method2 && m2->name == method1);
}
    
bool CommutativityRule::checkCondition(Method m1, Method m2) {
    if (m1->name == method1 && m2->name == method2) {
        return (*condition)(m1, m2);
    } else if (m1->name == method2 && m2->name == method1) {
        return (*condition)(m2, m1);
    } else {// The checking should only be called on the right rule
        ASSERT(false);
        //unreacheable
    }
    return false;
}

NamedFunction::NamedFunction(CSTR name, CheckFunctionType type, void *function) : name(name),
    type(type), function(function) { }

StateFunctions::StateFunctions(NamedFunction *transition, NamedFunction
    *preCondition, NamedFunction * justifyingPrecondition,
    NamedFunction *justifyingPostcondition,
    NamedFunction *postCondition, NamedFunction *print) : transition(transition),
    preCondition(preCondition), justifyingPrecondition(justifyingPrecondition),
    justifyingPostcondition(justifyingPostcondition),
    postCondition(postCondition), print(print) { }


AnnoInit::AnnoInit(NamedFunction *initial, NamedFunction *final, NamedFunction
    *copy, NamedFunction *clear, NamedFunction *printState, CommutativityRule
    *commuteRules, int ruleNum) : initial(initial), final(final), copy(copy),
    clear(clear), printState(printState), commuteRules(commuteRules),
    commuteRuleNum(ruleNum)
{
    funcMap = new StateFuncMap;
}
    

void AnnoInit::addInterfaceFunctions(CSTR name, StateFunctions *funcs) {
    funcMap->put(name, funcs);
}

AnnoInterfaceInfo::AnnoInterfaceInfo(CSTR name) : name(name), value(NULL) { }


/**********    Universal functions for rewriting the program    **********/

AnnoInterfaceInfo* _createInterfaceBeginAnnotation(CSTR name) {
    AnnoInterfaceInfo *info = new AnnoInterfaceInfo(name);
    // Create and instrument with the INTERFACE_BEGIN annotation
    cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(INTERFACE_BEGIN, info));
    return info;
}

void _createInterfaceEndAnnotation(CSTR name) {
    // Create and instrument with the INTERFACE_END annotation
    cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(INTERFACE_END, (void*) name));
}


void _setInterfaceBeginAnnotationValue(AnnoInterfaceInfo *info, void *value) {
    info->value = value;
}

void _createOPDefineAnnotation() {
    cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(OP_DEFINE, NULL));
}

void _createPotentialOPAnnotation(CSTR label) {
    cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(POTENTIAL_OP, label));
}

void _createOPCheckAnnotation(CSTR label) {
    cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(OP_CHECK, label));
}

void _createOPClearAnnotation() {
    cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(OP_CLEAR, NULL));
}

void _createOPClearDefineAnnotation() {
    cdsannotate(SPEC_ANALYSIS, new SpecAnnotation(OP_CLEAR_DEFINE, NULL));
}
