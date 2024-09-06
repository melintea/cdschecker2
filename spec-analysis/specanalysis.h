#ifndef _SPECANALYSIS_H
#define _SPECANALYSIS_H

#include <stack>

#include "traceanalysis.h"
#include "mymemory.h"
#include "modeltypes.h"
#include "action.h"

struct spec_stats {
    /** The number of traces that have passed the checking */
    unsigned passCnt;

    /** The number of inadmissible traces */
    unsigned inadmissibilityCnt;
    
    /** The number of all checked traces */
    unsigned traceCnt;

    /** The number of traces with a cyclic graph */
    unsigned cyclicCnt;

    /** The number of traces with broken graphs */
    unsigned brokenCnt;

    /** The number of traces with graphs that has no ordering points */
    unsigned noOrderingPointCnt;

    /** The number of traces that failed */
    unsigned failedCnt;
    
    /** The number of buggy and bug-free traces (by CDSChecker) */
    unsigned buggyCnt;
    unsigned bugfreeCnt;
};

class SPECAnalysis : public TraceAnalysis {
 public:
    SPECAnalysis();
    ~SPECAnalysis();

    virtual void setExecution(ModelExecution * execution);
    virtual void analyze(action_list_t *actions);
    virtual const char * name();
    virtual bool option(char *);
    virtual void finish();

    /** Some stats */
    spec_stats *stats;

    SNAPSHOTALLOC
 private:
    /** The execution */
    ModelExecution *execution;

    /** A few useful options */
    /* Print out the graphs of all executions */
    bool print_always;
    /* Print out the graphs of the inadmissible traces */
    bool print_inadmissible;
    /* Never print out the graphs of any traces */
    bool quiet;
    /* Whether we still want to check cyclic executions */
    bool checkCyclic;
    /* Stop checking when seeing one failed history */
    bool stopOnFail;
    /* The number of random histories to be checked; If 0, we check all possible
     * histories */
    int checkRandomNum;
    
    /** Whether this is a "check-12" like option */
    bool isCheckRandomHistories(char *opt, int &num);
};


#endif
