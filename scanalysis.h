#ifndef SCANALYSIS_H
#define SCANALYSIS_H
#include "traceanalysis.h"
#include "hashtable.h"

struct sc_statistics {
	unsigned long long elapsedtime;
	unsigned int sccount;
	unsigned int nonsccount;
	unsigned long long actions;
};

struct action_node;

typedef SnapList<action_node*> act_node_list_t;

typedef struct action_node {
	ModelAction *sb;
	ModelAction *fromReads;
	act_node_list_t *otherActs;

	action_node() {
		sb = NULL;
		fromReads = NULL;
		otherActs = NULL;
	}

	SNAPSHOTALLOC
} action_node;

class SCAnalysis : public TraceAnalysis {
 public:
	SCAnalysis();
	~SCAnalysis();
	virtual void setExecution(ModelExecution * execution);
	virtual void analyze(action_list_t *);
	virtual const char * name();
	virtual bool option(char *);
	virtual void finish();


	SNAPSHOTALLOC
 private:
	void update_stats();
	void print_list(action_list_t *list);
	int buildVectors(action_list_t *);
	bool updateConstraints(ModelAction *act);
	void computeCV(action_list_t *);
	void changeBasedComputeCV(action_list_t *);
	action_list_t * generateSC(action_list_t *);
	bool processReadFast(ModelAction *read, ClockVector *cv);
	bool processReadSlow(ModelAction *read, ClockVector *cv, bool * updatefuture);
	int getNextActions(ModelAction **array);
	bool merge(ClockVector *cv, const ModelAction *act, const ModelAction *act2);
	void check_rf(action_list_t *list);
	void reset(action_list_t *list);
	ModelAction* pruneArray(ModelAction**, int);

	int maxthreads;
	HashTable<const ModelAction *, ClockVector *, uintptr_t, 4 > cvmap;
	bool cyclic;
	HashTable<const ModelAction *, const ModelAction *, uintptr_t, 4 > badrfset;
	HashTable<void *, const ModelAction *, uintptr_t, 4 > lastwrmap;
	SnapVector<action_list_t> threadlists;
	ModelExecution *execution;
	/** fastVersion -> at first, we don't care whether we prioritize the SC or hb
	 *  edges and just randomly add any edges that we can.
	    !fastVersion -> when we later find that it's not SC, we destroy
	    everything and run the slower version, where we prioritize SC and hb.
	*/
	bool fastVersion;
	/** In the slow version mode, we first add those SC and hb edges (with
	 * allowNonSC to be false). When no more SC and hb edges are left, we set
	 * allowNonSC to be true to let it add any edges that we can add.
	*/
	bool allowNonSC;

	HashTable<const ModelAction *, action_node*, uintptr_t, 4 > nodeMap;

	bool print_always;
	bool print_buggy;
	bool print_nonsc;
	bool time;
	struct sc_statistics *stats;
};
#endif
