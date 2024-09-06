#include "specanalysis.h"
#include "action.h"
#include "cyclegraph.h"
#include "threads-model.h"
#include "clockvector.h"
#include "execution.h"
#include <sys/time.h>
#include <assert.h>
#include "modeltypes.h"
#include "executiongraph.h"


SPECAnalysis::SPECAnalysis()
{
    execution = NULL;
    stats = (struct spec_stats*) model_calloc(1, sizeof(struct spec_stats));
    print_always = false;
    print_inadmissible = true;
    quiet = false;
    checkCyclic = false;
    stopOnFail = false;
    checkRandomNum = 0;
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

    if (stats->cyclicCnt > 0 && checkCyclic) {
        model_print("Warning: You have cycle in your execution graphs.\n");
    }

    if (stats->failedCnt == 0) {
        model_print("Yay! All executions have passed the specification.\n");
        if (stats->brokenCnt > 0)
            model_print("However! You have executions with a broken graph.\n");
        if (stats->cyclicCnt > 0) {
            model_print("Warning: You have cyclic execution graphs!\n");
            if (checkCyclic) {
                model_print("Make sure that's what you expect.\n");
            }
        }
        if (stats->inadmissibilityCnt > 0)
            model_print("However! You have inadmissible executions.\n");
        if (stats->noOrderingPointCnt > 0 && print_always)
            model_print("You have execution graphs that have no ordering points.\n");

    }
}

bool SPECAnalysis::isCheckRandomHistories(char *opt, int &num) {
    char *p = opt;
    bool res = false;
    if (*p == 'c') {
        p++;
        if (*p == 'h') {
            p++;
            if (*p == 'e') {
                p++;
                if (*p == 'c') {
                    p++;
                    if (*p == 'k') {
                        res = true;
                        p++;
                    }
                }
            }
        }
    }
    if (res) {
        // p now should point to '-'
        if (*p == '-') {
            p++;
            num = atoi(p);
            if (num > 0)
                return true;
        }
        return false;
    }
    return res;
}

bool SPECAnalysis::option(char * opt) {
    if (strcmp(opt, "verbose")==0) {
        print_always=true;
        return false;
    } else if (strcmp(opt, "no-admissible")==0) {
        print_inadmissible = false;
        return false;
    } else if (strcmp(opt, "quiet")==0) {
        quiet = true;
        return false;
    } else if (strcmp(opt, "check-cyclic") == 0) {
        checkCyclic = true;
        return false;
    } else if (strcmp(opt, "stop-on-fail")==0) {
        stopOnFail = true;
        return false;
    } else if (isCheckRandomHistories(opt, checkRandomNum)) {
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

    // FIXME: Make checkCyclic false by default
    ExecutionGraph *graph = new ExecutionGraph(execution, checkCyclic);
    graph->buildGraph(actions);
    if (graph->isBroken()) {
        stats->brokenCnt++;
        if (print_always && !quiet) { // By default not printing
            model_print("Execution #%d has a broken graph.\n\n",
                execution->get_execution_number());
        }
        return;
    }

    if (print_always) {
        model_print("------------------  Checking execution #%d"
            "  ------------------\n", execution->get_execution_number());
    }
    
    // Count how many executions that have no-ordering-point method calls
    if (graph->isNoOrderingPoint()) {
        stats->noOrderingPointCnt++;
    }

    if (!graph->checkAdmissibility()) {
        /* One more inadmissible trace */
        stats->inadmissibilityCnt++;
        if (print_inadmissible && !quiet) {
            model_print("Execution #%d is NOT admissible\n",
                execution->get_execution_number());
            graph->print();
            if (print_always)
                graph->printAllMethodInfo(true);
        }
        return;
    }

    bool pass = false;
    if (graph->hasCycle()) {
        /* One more trace with a cycle */
        stats->cyclicCnt++;
        if (!checkCyclic) {
            if (!quiet)
                graph->print();
            if (print_always && !quiet) { // By default not printing
                model_print("Execution #%d has a cyclic graph.\n\n",
                    execution->get_execution_number());
            }
        } else {
            if (print_always && !quiet) {
                model_print("Checking cyclic execution #%d...\n",
                    execution->get_execution_number());
            }
            pass = graph->checkCyclicGraphSpec(print_always && !quiet);
        }
    } else if (checkRandomNum > 0) { // Only a few random histories
        if (print_always && !quiet)
            model_print("Check %d random histories...\n", checkRandomNum);
        pass = graph->checkRandomHistories(checkRandomNum, true, print_always && !quiet);
    } else { // Check all histories 
        if (print_always && !quiet)
            model_print("Check all histories...\n");
        pass = graph->checkAllHistories(true, print_always && !quiet);
    }

    if (!pass) {
        /* One more failed trace */
        stats->failedCnt++;
        model_print("Execution #%d failed.\n\n",
            execution->get_execution_number());
    } else {
        /* One more passing trace */
        stats->passCnt++;

        if (print_always && !quiet) { // By default not printing
            model_print("Execution #%d passed.\n\n",
                execution->get_execution_number());
        }
    }
}
