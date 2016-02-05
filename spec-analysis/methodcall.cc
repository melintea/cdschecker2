#include <algorithm>
#include "common.h"
#include "methodcall.h"

MethodCall::MethodCall(CSTR name) {
	name = name;
	prev = new SnapSet<Method>;
	next = new SnapSet<Method>;
	concurrent = new SnapSet<Method>;
	orderingPoints = new action_list_t;
	allPrev = new SnapSet<Method>;
	allNext  = new SnapSet<Method>;
}
	
void MethodCall::addPrev(Method m) { prev->insert(m); }

void MethodCall::addNext(Method m) { next->insert(m); }
	
void MethodCall::addConcurrent(Method m) { concurrent->insert(m); }

void MethodCall::addOrderingPoint(ModelAction *act) {
	bool hasIt = orderingPoints->end() != std::find(orderingPoints->begin(),
		orderingPoints->end(), act);
	if (!hasIt)
		orderingPoints->push_back(act);
}

bool MethodCall::belong(MethodSet s, Method m) {
	return s->end() != std::find(s->begin(), s->end(), m);
}

MethodSet MethodCall::intersection(MethodSet s1, MethodSet s2) {
	MethodSet res = new SnapSet<Method>;
	SnapSet<Method>::iterator it;
	for (it = s1->begin(); it != s1->end(); it++) {
		Method m = *it;
		if (belong(s2, m))
			res->insert(m);
	}
	return res;
}

bool MethodCall::disjoint(MethodSet s1, MethodSet s2) {
	SnapSet<Method>::iterator it;
	for (it = s1->begin(); it != s1->end(); it++) {
		Method m = *it;
		if (belong(s2, m))
			return false;
	}
	return true;
}

void MethodCall::print() {
	model_print("Method Call %s (Seq #%d)\n", name,
		begin->get_seq_number());
}
