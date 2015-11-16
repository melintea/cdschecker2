#include "specanalysis.h"
#include "action.h"
#include "cyclegraph.h"
#include "threads-model.h"
#include "clockvector.h"
#include "execution.h"
#include <sys/time.h>
#include <assert.h>
#include "modeltypes.h"


SPECAnalysis::SPECAnalysis()
{
	execution = NULL;
}

SPECAnalysis::~SPECAnalysis() {
}

void SPECAnalysis::setExecution(ModelExecution * execution) {
	this->execution = execution;
}

const char * SPECAnalysis::name() {
	const char * name = "SPEC";
	return name;
}

void SPECAnalysis::finish() {
	model_print("SPECAnalysis finished!\n");
}

bool SPECAnalysis::option(char * opt) {
	return true;
}

void SPECAnalysis::analyze(action_list_t *actions) {
	CPGraph *cpGraph = new CPGraph(execution);
	cpGraph->build(actions);
	cpGraph->printGraph();
	if (!cpGraph->checkAdmissibility()) {
		model_print("This execution is not admissible\n");
	}
	
	CPNodeListVector *sortings = cpGraph->generateAllSortings();
	//cpGraph->printAllSortings(sortings);

	CPNodeList *list= cpGraph->generateOneSorting();
	cpGraph->printOneSorting(list);
	if (!cpGraph->checkOne(list)) {
		model_print("This execution is NOT consistent with the spec.\n");
	}
}
