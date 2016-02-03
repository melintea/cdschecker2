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

	model_print("Total execution checked: %d\n", stats->bugfreeCnt);
	model_print("Detected by CDSChecker: %d\n", stats->buggyCnt);
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
	} else if (strcmp(opt, "inadmissible")==0) {
		print_inadmissible = true;
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
	if (execution->have_bug_reports()) {
		/* Count the number traces */
		stats->buggyCnt++;
		return;
	}

	/* Count the number bug-free traces */
	stats->bugfreeCnt++;

	ExecutionGraph *graph = new ExecutionGraph(execution);
	graph->buildGraph(actions);
	if (graph->isBroken()) {
		stats->brokenCnt++;
		return;
	}

	if (!graph->checkAdmissibility()) {
		/* One more inadmissible trace */
		stats->inadmissibilityCnt++;
		if (print_inadmissible && !quiet) {
			model_print("Execution #%d is NOT admissible\n",
				execution->get_execution_number());
			graph->print();
		}
		return;
	}

	if (graph->hasCycle()) {
		/* One more trace with a cycle */
		stats->cyclicCnt++;
		if (!quiet)
			graph->print();
		return;
	}
	
	bool pass = false;
	MethodList *history = NULL;
	MethodListVector *histories= NULL;
	if (check_one) { // Only check one random sorting
		history = graph->generateOneHistory();
		pass = graph->checkOneHistory(history);
	} else { // Check all sortings
		histories = graph->generateAllHistories();
		pass = graph->checkAllHistories(histories);
	}

	if (!pass) {
		/* One more failed trace */
		stats->failedCnt++;

		if (!quiet) {
			model_print("This execution is NOT consistent with the spec.\n");
			graph->print();
			if (check_one)
				graph->printOneHistory(history);
			else
				graph->printAllHistories(histories);
		}
	} else {
		/* One more passing trace */
		stats->passCnt++;

		if (print_always && !quiet) { // By default not printing
			model_print("This execution is consistent with the spec.\n");
			graph->print();
			if (check_one)
				graph->printOneHistory(history);
			else
				graph->printAllHistories(histories);
		}
	}
}
