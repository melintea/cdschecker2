#ifndef SCANALYSIS_H
#define SCANALYSIS_H
#include "traceanalysis.h"
#include "hashtable.h"

struct sc_statistics {
    unsigned long long elapsedtime;
    unsigned int sccount;
    unsigned int nonsccount;
    unsigned long long actions;

    /* The time elapsed for each part */
    unsigned long long buildVectorTime;
    unsigned long long computeCVTime;
    unsigned long long computeCVOtherTime;
    unsigned long long passChangeTime;
    unsigned long long processReadTime;
    
    /* The number of read actions */
    unsigned long long reads;
    /* The number of write actions */
    unsigned long long writes;
    /* The number of processed read actions */
    unsigned long long processedReads;
    /* The number of actions in the write lists */
    unsigned long long writeListsLength;
    /* The maximum number of actions in the write lists */
    unsigned long long writeListsMaxLength;
    unsigned long long writeListMaxSearchTime;
    /* The number of processed actions in the write lists */
    unsigned long long processedWrites;
    /* The number of push to the update list */
    unsigned long long pushCount;
    /* The number of merge called */
    unsigned long long mergeCount;
};

typedef ModelList<const ModelAction*> const_actions_t;

struct action_node;

typedef SnapList<action_node*> act_node_list_t;

typedef struct action_node {
    ModelAction *sb;
    /* Can be reads_from, thrd_create->thrd_start & thrd_finish->thrd_join */
    ModelAction *specialEdge; 
    const_actions_t *otherActs;

    action_node() {
        sb = NULL;
        specialEdge = NULL;
        otherActs = NULL;
    }

    bool addOtherAction(const ModelAction *act) {
        if (otherActs == NULL)
            otherActs = new const_actions_t;
        for (const_actions_t::iterator it = otherActs->begin(); it !=
            otherActs->end(); it++) {
            const ModelAction *elem = *it;
            if (elem == act)
                return false;
        }
        otherActs->push_back(act);
        return true;
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
    int buildVectorsSlow(action_list_t *);
    int buildVectorsFast(action_list_t *);
    bool updateConstraints(ModelAction *act);
    void computeCV(action_list_t *);
    void computeCVSlow(action_list_t *);
    void computeCVFast(action_list_t *);
    action_list_t * generateSC(action_list_t *);
    bool processReadFast(const ModelAction *read, ClockVector *cv);
    bool processReadSlow(const ModelAction *read, ClockVector *cv, bool *updateFuture);
    int getNextActions(ModelAction **array);
    bool merge(ClockVector *cv, const ModelAction *act, const ModelAction *act2);
    void check_rf(action_list_t *list);
    void reset(action_list_t *list);
    ModelAction* pruneArray(ModelAction**, int);
    
    /** Extract the set of reads to the list */
    void getWriteActions(const_actions_t *list);
    
    /** Only pass the new change through the edges of sb, rf, create->start and
     *  finish->join edges
     */
    void passChange(const ModelAction *act);

    /** A wrapper for pushing changes to the updateSet for the purpose of
     * collecting statistics */
    void pushChange(const ModelAction *act);
    void popUpdateSet();

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
     *  allowNonSC to be false). When no more SC and hb edges are left, we set
     *  allowNonSC to be true to let it add any edges that we can add.
    */
    bool allowNonSC;
    /** When allowNonSC is true, we only allow updating those future edges for
     * the first time.
    */
    bool updateFuture;
    /** Mark whether clock vectors have changed */
    bool cvChanged;

    HashTable<const ModelAction *, action_node*, uintptr_t, 4 > nodeMap;
    /** The list of write operations per location/thread */
    HashTable<void *, SnapVector<SnapVector<ModelAction*>*>*, uintptr_t, 4 > writeMap;
    const_actions_t *updateSet;
    unsigned int updateSetSize;

    bool print_always;
    bool print_buggy;
    bool print_nonsc;
    bool time;
    struct sc_statistics *stats;
};
#endif
