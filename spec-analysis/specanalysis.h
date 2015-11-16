#ifndef _SPECANALYSIS_H
#define _SPECANALYSIS_H

#include <stack>

#include "traceanalysis.h"
#include "hashtable.h"
#include "specannotation.h"
#include "mymemory.h"
#include "modeltypes.h"
#include "action.h"
#include "common.h"
#include "cpgraph.h"

class SPECAnalysis : public TraceAnalysis {
 public:
	SPECAnalysis();
	~SPECAnalysis();

	virtual void setExecution(ModelExecution * execution);
	virtual void analyze(action_list_t *actions);
	virtual const char * name();
	virtual bool option(char *);
	virtual void finish();

	SNAPSHOTALLOC
 private:
 	/** The execution */
	ModelExecution *execution;

	/** The commit point graph */
	CPGraph *graph;
};


#endif
