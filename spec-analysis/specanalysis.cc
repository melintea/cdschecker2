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
	stats = (struct spec_stats*) model_calloc(1, sizeof(struct spec_stats));
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
	model_print("Among %d executions, %d of them are admissible.\n", stats->traceCnt,
		stats->traceCnt - stats->inadmissibilityCnt);
	if (stats->passCnt == stats->traceCnt) {
		model_print("All executions (%d) have passed the specification.\n",
			stats->traceCnt);	
	} else {
		model_print("%d executions have NOT passed the specification.\n",
			stats->traceCnt - stats->passCnt);	
	}
}

bool SPECAnalysis::option(char * opt) {
	return true;
}

void SPECAnalysis::analyze(action_list_t *actions) {
	stats->traceCnt++;
	CPGraph *cpGraph = new CPGraph(execution);
	cpGraph->build(actions);
	cpGraph->printGraph();
	if (!cpGraph->checkAdmissibility()) {
		model_print("This execution is not admissible\n");
		stats->inadmissibilityCnt++;
		return;
	}
	
	CPNodeListVector *sortings = cpGraph->generateAllSortings();
	//cpGraph->printAllSortings(sortings);

	CPNodeList *list= cpGraph->generateOneSorting();
	cpGraph->printOneSorting(list);
	if (!cpGraph->checkOne(list)) {
		model_print("This execution is NOT consistent with the spec.\n");
	} else {
		stats->passCnt++;
	}

}
