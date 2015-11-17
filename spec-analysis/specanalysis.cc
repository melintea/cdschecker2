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
	print_always = false;
	print_inadmissible = true;
	quiet = false;
	check_one = false;
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
	model_print("\n");
	model_print(">>>>>>>> SPECAnalysis finished <<<<<<<<\n");

	model_print("Total execution checked: %d\n", stats->traceCnt);
	model_print("Broken graph: %d\n", stats->brokenCnt);
	model_print("Cyclic graph: %d\n", stats->cyclicCnt);
	model_print("Inadmissible executions: %d\n", stats->inadmissibilityCnt);
	model_print("Failed executions: %d\n", stats->failedCnt);
	if (stats->failedCnt == 0) {
		model_print("Yay! All executions have passed the specification.\n");
		if (stats->brokenCnt > 0)
			model_print("However! You have executions with a broken graph.\n");
		if (stats->cyclicCnt > 0)
			model_print("However! You have executions with a cyclic graph.\n");
		if (stats->inadmissibilityCnt > 0)
			model_print("However! You have inadmissible executions.\n");
	}
}

bool SPECAnalysis::option(char * opt) {
	if (strcmp(opt, "verbose")==0) {
		print_always=true;
		return false;
	} else if (strcmp(opt, "inadmissible-quiet")==0) {
		print_inadmissible = false;
		return false;
	}  else if (strcmp(opt, "quiet")==0) {
		quiet = true;
		return false;
	} else if (strcmp(opt, "check-one")==0) {
		check_one = true;
		return false;
	} else if (strcmp(opt, "help") != 0) {
		model_print("Unrecognized option: %s\n", opt);
	}

	model_print("SPEC Analysis options\n"
		"By default SPEC outputs the graphs of those failed execution\n"
		"verbose -- print graphs of any traces\n"
		"quiet -- do not print out graphs\n"
		"inadmissible-quiet -- print the inadmissible\n"
		"check-one -- only check one random possible topological"
			"sortings (check all possible by default)\n"
	);
	model_print("\n");
	
	return true;
}

void SPECAnalysis::analyze(action_list_t *actions) {
	/* Count the number traces */
	stats->traceCnt++;

	CPGraph *cpGraph = new CPGraph(execution);
	cpGraph->build(actions);
	if (cpGraph->getBroken()) {
		stats->brokenCnt++;
		return;
	}

	if (!cpGraph->checkAdmissibility()) {
		/* One more inadmissible trace */
		stats->inadmissibilityCnt++;
		if (print_inadmissible && !quiet) {
			model_print("Execution #%d is NOT admissible\n",
				execution->get_execution_number());
			cpGraph->printGraph();
		}
		return;
	}

	if (cpGraph->hasCycle()) {
		/* One more trace with a cycle */
		stats->cyclicCnt++;
		if (!quiet)
			cpGraph->printGraph();
		return;
	}
	
	bool pass = false;
	CPNodeList *list;
	CPNodeListVector *sortings;
	if (check_one) { // Only check one random sorting
		list = cpGraph->generateOneSorting();
		pass = cpGraph->checkOne(list);
	} else { // Check all sortings
		sortings = cpGraph->generateAllSortings();
		pass = cpGraph->checkAll(sortings);
	}

	if (!pass) {
		/* One more failed trace */
		stats->failedCnt++;

		if (!quiet) {
			model_print("This execution is NOT consistent with the spec.\n");
			cpGraph->printGraph();
			if (check_one)
				cpGraph->printOneSorting(list);
			else
				cpGraph->printAllSortings(sortings);
		}
	} else {
		/* One more passing trace */
		stats->passCnt++;

		if (print_always && !quiet) { // By default not printing
			model_print("This execution is consistent with the spec.\n");
			cpGraph->printGraph();
			if (check_one)
				cpGraph->printOneSorting(list);
			else
				cpGraph->printAllSortings(sortings);
		}
	}
}
