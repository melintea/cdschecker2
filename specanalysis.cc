#include "specanalysis.h"
#include "action.h"
#include "threads-model.h"
#include "clockvector.h"
#include "execution.h"
#include <sys/time.h>


SPECAnalysis::SPECAnalysis()
{
}

SPECAnalysis::~SPECAnalysis() {
}

void SPECAnalysis::setExecution(ModelExecution * execution) {
	this->execution=execution;
}

const char * SPECAnalysis::name() {
	const char * name = "SPEC";
	return name;
}

void SPECAnalysis::finish() {
}

bool SPECAnalysis::option(char * opt) {
}

void SPECAnalysis::analyze(action_list_t *actions) {
}

