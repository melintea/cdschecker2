#ifndef SPECANALYSIS_H
#define SPECANALYSIS_H
#include "traceanalysis.h"
#include "hashtable.h"

class SPECAnalysis : public TraceAnalysis {
 public:
	SPECAnalysis();
	~SPECAnalysis();
	virtual void setExecution(ModelExecution * execution);
	virtual void analyze(action_list_t *);
	virtual const char * name();
	virtual bool option(char *);
	virtual void finish();


	SNAPSHOTALLOC
 private:

};
#endif
