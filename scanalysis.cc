#include "scanalysis.h"
#include "action.h"
#include "threads-model.h"
#include "clockvector.h"
#include "execution.h"

#include <utility>

#include <sys/time.h>


SCAnalysis::SCAnalysis() :
    cvmap(),
    cyclic(false),
    badrfset(),
    lastwrmap(),
    threadlists(1),
    execution(NULL),
    fastVersion(true),
    allowNonSC(false),
    print_always(false),
    print_buggy(true),
    print_nonsc(false),
    time(false),
    stats((struct sc_statistics *)model_calloc(1, sizeof(struct sc_statistics)))
{
    updateSet = new const_actions_t;
    updateSetSize = 0;
}

SCAnalysis::~SCAnalysis() {
    delete(stats);
}

void SCAnalysis::setExecution(ModelExecution * execution) {
    this->execution=execution;
}

const char * SCAnalysis::name() {
    const char * name = "SC";
    return name;
}

void SCAnalysis::finish() {
    if (time)
        model_print("Elapsed time in usec %llu\n", stats->elapsedtime);
    model_print("SC count: %u\n", stats->sccount);
    model_print("Non-SC count: %u\n", stats->nonsccount);
    model_print("Total actions: %llu\n", stats->actions);
    unsigned long long execCount = stats->sccount + stats->nonsccount;
    unsigned long long actionperexec=(stats->actions) / execCount;

    model_print("Elapsed time in buildVector %llu\n", stats->buildVectorTime);
    model_print("Elapsed time in computeCV %llu\n", stats->computeCVTime);
    model_print("Elapsed time in computeCVOther %llu\n", stats->computeCVOtherTime);
    model_print("Elapsed time in processRead %llu\n", stats->processReadTime);
    model_print("Elapsed time in passChange %llu\n", stats->passChangeTime);

    model_print("Actions per execution: %llu\n", actionperexec);

    model_print("Read actions per execution: %llu\n", stats->reads / execCount);
    model_print("Write actions per execution: %llu\n", stats->writes / execCount);
    model_print("Push per execution: %llu\n", stats->pushCount / execCount);
    model_print("Merge per execution: %llu\n", stats->mergeCount / execCount);
    model_print("Processed read actions per execution: %llu\n", stats->processedReads / execCount);
    model_print("Processed writes calculated per processed read: %llu\n", stats->processedWrites / stats->processedReads);
    model_print("Length of write lists per processed read: %llu\n", stats->writeListsLength / stats->processedReads);
    model_print("Maximum length of write lists: %llu\n", stats->writeListsMaxLength);
}

bool SCAnalysis::option(char * opt) {
    if (strcmp(opt, "verbose")==0) {
        print_always=true;
        return false;
    } else if (strcmp(opt, "buggy")==0) {
        return false;
    } else if (strcmp(opt, "quiet")==0) {
        print_buggy=false;
        return false;
    } else if (strcmp(opt, "nonsc")==0) {
        print_nonsc=true;
        return false;
    } else if (strcmp(opt, "time")==0) {
        time=true;
        return false;
    } else if (strcmp(opt, "help") != 0) {
        model_print("Unrecognized option: %s\n", opt);
    }

    model_print("SC Analysis options\n");
    model_print("verbose -- print all feasible executions\n");
    model_print("buggy -- print only buggy executions (default)\n");
    model_print("nonsc -- print non-sc execution\n");
    model_print("quiet -- print nothing\n");
    model_print("time -- time execution of scanalysis\n");
    model_print("\n");
    
    return true;
}

void SCAnalysis::print_list(action_list_t *list) {
    model_print("---------------------------------------------------------------------\n");
    if (cyclic)
        model_print("Not SC\n");
    unsigned int hash = 0;

    for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
        const ModelAction *act = *it;
        if (act->get_seq_number() > 0) {
            if (badrfset.contains(act))
                model_print("BRF ");
            act->print();
            cvmap.get(act)->print();
            if (badrfset.contains(act)) {
                model_print("Desired Rf: %u \n", badrfset.get(act)->get_seq_number());
            }
        }
        hash = hash ^ (hash << 3) ^ ((*it)->hash());
    }
    model_print("HASH %u\n", hash);
    model_print("---------------------------------------------------------------------\n");
}

void SCAnalysis::analyze(action_list_t *actions) {
    struct timeval start;
    struct timeval finish;
    if (time)
        gettimeofday(&start, NULL);
    /* Run as the fast version at first */
    fastVersion = true;
    action_list_t *list = generateSC(actions);
    if (cyclic) {
        //print_list(list);
        
        reset(actions);
        if (list)
            delete list;
        // Now we found that this is not SC, we run the slow version
        fastVersion = false;
        list = generateSC(actions);
    }
    check_rf(list);
    if (print_always || (print_buggy && execution->have_bug_reports())|| (print_nonsc && cyclic))
        print_list(list);
    if (time) {
        gettimeofday(&finish, NULL);
        stats->elapsedtime+=((finish.tv_sec*1000000+finish.tv_usec)-(start.tv_sec*1000000+start.tv_usec));
    }
    update_stats();
}

void SCAnalysis::update_stats() {
    if (cyclic) {
        stats->nonsccount++;
    } else {
        stats->sccount++;
    }
}

void SCAnalysis::check_rf(action_list_t *list) {
    bool hasBadRF = false;
    for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
        const ModelAction *act = *it;
        if (act->is_read()) {
            if (act->get_reads_from() != lastwrmap.get(act->get_location())) {
                badrfset.put(act, lastwrmap.get(act->get_location()));
                hasBadRF = true;
            }
        }
        if (act->is_write())
            lastwrmap.put(act->get_location(), act);
    }
    if (hasBadRF != cyclic) {
        model_print("cyclic=%d\n", cyclic);
        print_list(list);
        ASSERT (false);
    } else {
        //model_print("SC\n");
        //print_list(list);
    }
}


bool SCAnalysis::merge(ClockVector *cv, const ModelAction *act, const ModelAction *act2) {
    // Record the number merge called
    stats->mergeCount++;

    bool status;
    ClockVector *cv2 = cvmap.get(act2);
    if (cv2 == NULL)
        return true;
    if (cv2->getClock(act->get_tid()) >= act->get_seq_number() && act->get_seq_number() != 0) {
        cyclic = true;
        //refuse to introduce cycles into clock vectors
        return false;
    }
    if (fastVersion) {
        status = cv->merge(cv2);
        return status;
    } else {
        bool merged;
        if (allowNonSC) {
            /* Now we can add any edges */
            merged = cv->merge(cv2);
            if (merged) {
                allowNonSC = false;
                cvChanged = true;
            }
            return merged;
        } else {
            /* We are still only allowed to add SC and hb edges */
            if (act2->happens_before(act) ||
                (act->is_seqcst() && act2->is_seqcst() && *act2 < *act)) {
                return cv->merge(cv2);
            } else {
                /* Don't add non-SC or non-hb edges yet */
                return false;
            }
        }
    }
}

int SCAnalysis::getNextActions(ModelAction ** array) {
    int count=0;

    for (int t = 0; t <= maxthreads; t++) {
        action_list_t *tlt = &threadlists[t];
        if (tlt->empty())
            continue;
        ModelAction *act = tlt->front();
        ClockVector *cv = cvmap.get(act);
        
        /* Find the earliest in SC ordering */
        for (int i = 0; i <= maxthreads; i++) {
            if ( i == t )
                continue;
            action_list_t *threadlist = &threadlists[i];
            if (threadlist->empty())
                continue;
            ModelAction *first = threadlist->front();
            if (cv->synchronized_since(first)) {
                act = NULL;
                break;
            }
        }
        if (act != NULL) {
            array[count++]=act;
        }
    }
    if (count != 0)
        return count;
    for (int t = 0; t <= maxthreads; t++) {
        action_list_t *tlt = &threadlists[t];
        if (tlt->empty())
            continue;
        ModelAction *act = tlt->front();
        ClockVector *cv = act->get_cv();
        
        /* Find the earliest in SC ordering */
        for (int i = 0; i <= maxthreads; i++) {
            if ( i == t )
                continue;
            action_list_t *threadlist = &threadlists[i];
            if (threadlist->empty())
                continue;
            ModelAction *first = threadlist->front();
            if (cv->synchronized_since(first)) {
                act = NULL;
                break;
            }
        }
        if (act != NULL) {
            array[count++]=act;
        }
    }

    ASSERT(count==0 || cyclic);

    return count;
}

ModelAction * SCAnalysis::pruneArray(ModelAction **array,int count) {
    /* No choice */
    if (count == 1)
        return array[0];

    /* Choose first non-write action */
    ModelAction *nonwrite=NULL;
    for(int i=0;i<count;i++) {
        if (!array[i]->is_write())
            if (nonwrite==NULL || nonwrite->get_seq_number() > array[i]->get_seq_number())
                nonwrite = array[i];
    }
    if (nonwrite != NULL)
        return nonwrite;
    
    /* Look for non-conflicting action */
    ModelAction *nonconflict=NULL;
    for(int a=0;a<count;a++) {
        ModelAction *act=array[a];
        for (int i = 0; i <= maxthreads && act != NULL; i++) {
            thread_id_t tid = int_to_id(i);
            if (tid == act->get_tid())
                continue;
            
            action_list_t *list = &threadlists[id_to_int(tid)];
            for (action_list_t::iterator rit = list->begin(); rit != list->end(); rit++) {
                ModelAction *write = *rit;
                if (!write->is_write())
                    continue;
                ClockVector *writecv = cvmap.get(write);
                if (writecv->synchronized_since(act))
                    break;
                if (write->get_location() == act->get_location()) {
                    //write is sc after act
                    act = NULL;
                    break;
                }
            }
        }
        if (act != NULL) {
            if (nonconflict == NULL || nonconflict->get_seq_number() > act->get_seq_number())
                nonconflict=act;
        }
    }
    return nonconflict;
}

action_list_t * SCAnalysis::generateSC(action_list_t *list) {
    struct timeval start;
    struct timeval finish;

    gettimeofday(&start, NULL);
    int numactions=buildVectors(list);
    gettimeofday(&finish, NULL);
    stats->buildVectorTime += ((finish.tv_sec*1000000+finish.tv_usec)-(start.tv_sec*1000000+start.tv_usec));

    //gettimeofday(&start, NULL);
    computeCV(list);
    //gettimeofday(&finish, NULL);
    //stats->computeCVTime += ((finish.tv_sec*1000000+finish.tv_usec)-(start.tv_sec*1000000+start.tv_usec));

    if (fastVersion) {
        stats->actions+=numactions;
        /* A quick return for fast non-SC detection */
        if (cyclic)
            return NULL;
    }

    action_list_t *sclist = new action_list_t();
    ModelAction **array = (ModelAction **)model_calloc(1, (maxthreads + 1) * sizeof(ModelAction *));
    int * choices = (int *) model_calloc(1, sizeof(int)*numactions);
    int endchoice = 0;
    int currchoice = 0;
    int lastchoice = -1;
    while (true) {
        int numActions = getNextActions(array);
        if (numActions == 0)
            break;
        ModelAction * act=pruneArray(array, numActions);
        if (act == NULL) {
            if (currchoice < endchoice) {
                act = array[choices[currchoice]];
                //check whether there is still another option
                if ((choices[currchoice]+1)<numActions)
                    lastchoice=currchoice;
                currchoice++;
            } else {
                act = array[0];
                choices[currchoice]=0;
                if (numActions>1)
                    lastchoice=currchoice;
                currchoice++;
            }
        }
        thread_id_t tid = act->get_tid();
        //remove action
        threadlists[id_to_int(tid)].pop_front();
        //add ordering constraints from this choice
        if (updateConstraints(act)) {
            //propagate changes if we have them
            bool prevc=cyclic;
            computeCV(list);
            if (!prevc && cyclic) {
                model_print("ROLLBACK in SC\n");
                //check whether we have another choice
                if (lastchoice != -1) {
                    //have to reset everything
                    choices[lastchoice]++;
                    endchoice=lastchoice+1;
                    currchoice=0;
                    lastchoice=-1;
                    reset(list);
                    buildVectors(list);
                    computeCV(list);
                    sclist->clear();
                    continue;
                } else { // We found that it is non-SC
                    fastVersion = false;
                    return sclist;
                }
            }
        }
        //add action to end
        sclist->push_back(act);
    }
    model_free(array);
    return sclist;
}

int SCAnalysis::buildVectors(action_list_t *list) {
    if (fastVersion)
        return buildVectorsFast(list);
    else
        return buildVectorsSlow(list);

}

void SCAnalysis::pushChange(const ModelAction *act) {
    /* To record the number of overall push to the update set */
    stats->pushCount++;
    updateSet->push_back(act);
    updateSetSize++;
}


void SCAnalysis::popUpdateSet() {
    updateSet->pop_front();
    updateSetSize--;
}

int SCAnalysis::buildVectorsFast(action_list_t *list) {
    maxthreads = 0;
    int numactions = 0;
    for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
        ModelAction *act = *it;

        ClockVector *cv = cvmap.get(act);
        if (cv == NULL) {
            cv = new ClockVector(NULL, act);
            cvmap.put(act, cv);
        }

        numactions++;
        int threadid = id_to_int(act->get_tid());
        if (threadid > maxthreads) {
            threadlists.resize(threadid + 1);
            maxthreads = threadid;
        }
    
        void *loc = act->get_location();
        SnapVector<SnapVector<ModelAction*>*> *writeLists = writeMap.get(loc);
        if (!writeLists) {
            writeLists = new SnapVector<SnapVector<ModelAction*>*>;
            writeMap.put(loc, writeLists);
        }
        if (std::cmp_less(writeLists->size(), threadid)) {  // writeLists->size() <= threadid)
            writeLists->resize(threadid + 1);
        }
        SnapVector<ModelAction*> *writeList = (*writeLists)[threadid];
        if (!writeList) {
            writeList = new SnapVector<ModelAction*>;
            (*writeLists)[threadid] = writeList;
        }

        /* Building the write lists */
        if (act->is_write()) {
            /* To record the number of write actions in the executions */
            stats->writes++;

            writeList->push_back(act);
            //model_print("writelists size=%d\n", writeLists->size());
            //model_print("maxthreads=%d\n", maxthreads);
            //act->print();
        }

        ModelAction *lastAct = threadlists[threadid].back();
        /* Add the sb edge */
        if (lastAct != NULL) {
            action_node *lastNode = nodeMap.get(lastAct);
            if (lastNode == NULL) {
                lastNode = new action_node;
                nodeMap.put(lastAct, lastNode);
            }
            lastNode->sb = act;
        }
        /* Add the rf edge */
        if (act->is_read()) {
            /* To record the number of read actions in the executions */
            stats->reads++;

            const ModelAction *write = act->get_reads_from();
            if (write->get_seq_number() != 0) {
                /* All the read actions that read from a different thread */
                ClockVector *writeCV = cvmap.get(write);
                if (writeCV == NULL) {
                    writeCV = new ClockVector(NULL, write);
                    cvmap.put(write, writeCV);
                }
                merge(cv, act, write);
                pushChange(act);

                action_node *writeNode = nodeMap.get(write);
                if (writeNode == NULL) {
                    writeNode = new action_node;
                    nodeMap.put(write, writeNode);
                }
                writeNode->specialEdge = act;
            }
        }
        /* thrd_create->thrd_start */
        if (act->is_thread_start()) {
            ModelAction *create = execution->get_thread(act)->get_creation();
            if (create != NULL) { // Not the very first main thread
                action_node *createNode = nodeMap.get(create);
                if (createNode == NULL) {
                    createNode = new action_node;
                    nodeMap.put(create, createNode);
                }
                createNode->specialEdge = act;
                pushChange(create);
            }
        }
        /* thrd_finish->thrd_join */
        if (act->is_thread_join()) {
            Thread *joinedthr = act->get_thread_operand();
            ModelAction *finish = execution->get_last_action(joinedthr->get_id());
            action_node *finishNode = nodeMap.get(finish);
            if (finishNode == NULL) {
                finishNode = new action_node;
                nodeMap.put(finish, finishNode);
            }
            finishNode->specialEdge = act;
            pushChange(finish);
        }
        threadlists[threadid].push_back(act);
    }
    return numactions;
}

int SCAnalysis::buildVectorsSlow(action_list_t *list) {
    maxthreads = 0;
    int numactions = 0;
    for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
        ModelAction *act = *it;

        ClockVector *cv = cvmap.get(act);
        if (cv == NULL) {
            cv = new ClockVector(NULL, act);
            cvmap.put(act, cv);
        }

        numactions++;
        int threadid = id_to_int(act->get_tid());
        if (threadid > maxthreads) {
            threadlists.resize(threadid + 1);
            maxthreads = threadid;
        }
        threadlists[threadid].push_back(act);
    }
    return numactions;
}

void SCAnalysis::reset(action_list_t *list) {
    for (int t = 0; t <= maxthreads; t++) {
        action_list_t *tlt = &threadlists[t];
        tlt->clear();
    }
    for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
        ModelAction *act = *it;
        delete cvmap.get(act);
        cvmap.put(act, NULL);
    }
    updateSet->clear();

    cyclic=false;   
}

bool SCAnalysis::updateConstraints(ModelAction *act) {
    bool changed = false;
    for (int i = 0; i <= maxthreads; i++) {
        thread_id_t tid = int_to_id(i);
        if (tid == act->get_tid())
            continue;

        action_list_t *list = &threadlists[id_to_int(tid)];
        for (action_list_t::iterator rit = list->begin(); rit != list->end(); rit++) {
            ModelAction *write = *rit;
            if (!write->is_write())
                continue;
            ClockVector *writecv = cvmap.get(write);
            if (writecv->synchronized_since(act))
                break;
            if (write->get_location() == act->get_location()) {
                //write is sc after act
                bool status = merge(writecv, write, act);
                if (status) {
                    passChange(write);
                }
                changed = true;
                break;
            }
        }
    }
    return changed;
}

bool SCAnalysis::processReadFast(const ModelAction *read, ClockVector *cv) {
    bool changed = false;
    bool status = false;

    /* Merge in the clock vector from the write */
    const ModelAction *write = read->get_reads_from();
    ClockVector *writecv = cvmap.get(write);

    /* To record the number of read actions that has been processed */
    stats->processedReads++;

    int writeAccessCount = 0;
    SnapVector<SnapVector<ModelAction*>*> *writeLists = writeMap.get(read->get_location());
    for (int i = 0; i <= maxthreads; i++) {
        thread_id_t tid = int_to_id(i);
        if (tid == read->get_tid())
            continue;
        if (std::cmp_equal(writeLists->size(), i)) { // writeLists->size() == i
            break;
        }
        SnapVector<ModelAction*> *writeList = (*writeLists)[i];
        if (!writeList || writeList->size() == 0)
            continue;
    
        /* To record the number of write actions in the writeList */
        stats->writeListsLength += writeList->size();

        /* To record the maximum number of write actions in the writeList */
        if (stats->writeListsMaxLength < writeList->size())
            stats->writeListsMaxLength = writeList->size();

        // Use binary search to reduce searching time
        int low = 0, high = writeList->size() - 1, mid;
        int earliestPos = -1;
        ClockVector *write2cv;
        // Find the earliest write2 (write2 -> read) in the list
        ModelAction *write2;
        
        if (tid == write->get_tid()) {
            // Update the read->write2 atomatically
            for (int i = writeList->size() - 1; i >= 0; i--) {
                //if (write == (*writeList)[i] && (i + 1) != writeList->size()) {
                if (write == (*writeList)[i] && std::cmp_not_equal((i + 1), writeList->size()) ) {
                    write2 = (*writeList)[i + 1];
                    //model_print("corner case\n");
                    //write->print();
                    //read->print();
                    //write2->print();
                    write2cv = cvmap.get(write2);
                    if (write2cv) {
                        status = merge(write2cv, write2, read);
                        if (status)
                            passChange(write);
                        changed |= status;
                    }
                    break;
                }
            }
            continue;
        }

        while (low + 1 < high) {
            // The number of processed write actions
            stats->processedWrites++;
            writeAccessCount++;

            mid = (low + high) / 2;
            write2 = (*writeList)[mid];
            write2cv = cvmap.get(write2);
            if (write2cv && cv->synchronized_since(write2)) {
                low = mid;
            } else {
                high = mid - 1;
            }
        }
        write2 = (*writeList)[high];
        write2cv = cvmap.get(write2);

        // The number of processed write actions
        stats->processedWrites++;
        writeAccessCount++;
        if (write2cv && cv->synchronized_since(write2)) { // Found it
            status = merge(writecv, write, write2);
            if (status)
                passChange(write);
            changed |= status;
            earliestPos = high;
        } else if (low != high) {
            // The number of processed write actions
            stats->processedWrites++;
            writeAccessCount++;

            write2 = (*writeList)[low];
            write2cv = cvmap.get(write2);
            if (write2cv && cv->synchronized_since(write2)) {
                status = merge(writecv, write, write2);
                if (status)
                    passChange(write);
                changed |= status;      
                earliestPos = low;
            }
        }
        
        // Find the latest write2 (write -> write2) in the list
        low = earliestPos + 1;
        //if (low == writeList->size()) {
        if (std::cmp_equal(low, writeList->size())) {
            // In case where only 1 action is in the list
            low = 0;
        }
        high = writeList->size() - 1;
        while (low + 1 < high) {
            // The number of processed write actions
            stats->processedWrites++;
            writeAccessCount++;

            mid = (low + high) / 2;
            write2 = (*writeList)[mid];
            write2cv = cvmap.get(write2);
            if (write2cv && write2cv->synchronized_since(write)) {
                high = mid;
            } else {
                low = mid + 1;
            }
        }
        write2 = (*writeList)[low];
        write2cv = cvmap.get(write2);

        // The number of processed write actions
        stats->processedWrites++;
        writeAccessCount++;
        if (write2cv && write2cv->synchronized_since(write)) { // Found it
            status = merge(write2cv, write2, read);
            if (status)
                passChange(write2);
            changed |= status;
        } else if (high != low) {
            // The number of processed write actions
            stats->processedWrites++;
            writeAccessCount++;

            write2 = (*writeList)[high];
            write2cv = cvmap.get(write2);
            if (write2cv && write2cv->synchronized_since(write)) {
                status = merge(write2cv, write2, read);
                if (status)
                    passChange(write2);
                changed |= status;      
            }
        }

        /*
        for (SnapVector<ModelAction*>::reverse_iterator rit = writeList->rbegin(); rit !=
            writeList->rend(); rit++) {
            ModelAction *write2 = *rit;
            if (!write2->is_write())
                continue;

            ClockVector *write2cv = cvmap.get(write2);
            if (write2cv == NULL)
                continue;

            // To record the number of write actions that have been calculated
            stats->processedWrites++;

            // write -sc-> write2 &&
            //   write -rf-> R =>
            //   R -sc-> write2 
            if (write2cv->synchronized_since(write)) {
                status = merge(write2cv, write2, read);
                if (status)
                    passChange(write2);
                changed |= status;
            }

            //looking for earliest write2 in iteration to satisfy this
            // write2 -sc-> R &&
            //   write -rf-> R =>
            //   write2 -sc-> write
            if (cv->synchronized_since(write2)) {
                status = merge(writecv, write, write2);
                if (status)
                    passChange(write);
                changed |= status;
                break;
            }
        }
        */
        //model_print("writeListLength: %d\n", writeList->size());
        //model_print("writeCount: %d\n", writeAccessCount);
    }
    return changed;
}

void SCAnalysis::getWriteActions(const_actions_t *list) {
    if (list == NULL) {
        list = new const_actions_t;
    }

    for (const_actions_t::iterator it = list->begin(); it != list->end(); it++) {
        const ModelAction *act = *it;
        if (act->is_read()) {
            const ModelAction *write = act->get_reads_from();
            list->push_back(write);
        }
    }
}


bool SCAnalysis::processReadSlow(const ModelAction *read, ClockVector *cv, bool *updateFuture) {
    bool changed = false;
    
    /* Merge in the clock vector from the write */
    const ModelAction *write = read->get_reads_from();
    ClockVector *writecv = cvmap.get(write);
    if ((*write < *read) || ! *updateFuture) {
        bool status = merge(cv, read, write) && (*read < *write);
        changed |= status;
        *updateFuture = status;
    }

    for (int i = 0; i <= maxthreads; i++) {
        thread_id_t tid = int_to_id(i);
        if (tid == read->get_tid())
            continue;
        //if (tid == write->get_tid())
        //  continue;
        action_list_t *list = execution->get_actions_on_obj(read->get_location(), tid);
        if (list == NULL)
            continue;
        for (action_list_t::reverse_iterator rit = list->rbegin(); rit != list->rend(); rit++) {
            ModelAction *write2 = *rit;
            if (!write2->is_write())
                continue;
            if (write2 == write)
                continue;

            ClockVector *write2cv = cvmap.get(write2);
            if (write2cv == NULL)
                continue;

            /* write -sc-> write2 &&
                 write -rf-> R =>
                 R -sc-> write2 */
            if (write2cv->synchronized_since(write)) {
                if ((*read < *write2) || ! *updateFuture) {
                    bool status = merge(write2cv, write2, read);
                    changed |= status;
                    *updateFuture |= status && (*write2 < *read);
                }
            }

            //looking for earliest write2 in iteration to satisfy this
            /* write2 -sc-> R &&
                 write -rf-> R =>
                 write2 -sc-> write */
            if (cv->synchronized_since(write2)) {
                if ((*write2 < *write) || ! *updateFuture) {
                    bool status = writecv == NULL || merge(writecv, write, write2);
                    changed |= status;
                    *updateFuture |= status && (*write < *write2);
                }
                break;
            }
        }
    }
    return changed;
}

void SCAnalysis::passChange(const ModelAction *act) {
    //struct timeval start;
    //struct timeval finish;
    /* Update the CV of the to node */
    action_node *node = nodeMap.get(act);

    if (node == NULL)
        return;
    const ModelAction *nextAct = node->sb;
    //model_print("pass change act:\n");
    //act->print();
    //cvmap.get(act)->print();
    if (nextAct) {
        //model_print("sb next:\n");
        //nextAct->print();
        ClockVector *nextCV = cvmap.get(nextAct);
        if (merge(nextCV, nextAct, act))
            pushChange(nextAct);
    }
    nextAct = node->specialEdge;
    if (nextAct) {
        ClockVector *nextCV = cvmap.get(nextAct);
        if (merge(nextCV, nextAct, act))
            pushChange(nextAct);
        //model_print("others next:\n");
        //nextAct->print();
    }
}


void SCAnalysis::computeCVFast(action_list_t *list) {
    struct timeval start;
    struct timeval finish;

    /* A BFS-like approach */
    while (updateSetSize > 0) {
        gettimeofday(&start, NULL);
        const ModelAction *act = updateSet->front();
        //updateSet->pop_front();
        popUpdateSet();
        gettimeofday(&finish, NULL);
        stats->computeCVOtherTime += ((finish.tv_sec*1000000+finish.tv_usec)-(start.tv_sec*1000000+start.tv_usec));

        /* Update the CV of the to node */
        gettimeofday(&start, NULL);
        passChange(act);
        gettimeofday(&finish, NULL);
        stats->passChangeTime += ((finish.tv_sec*1000000+finish.tv_usec)-(start.tv_sec*1000000+start.tv_usec));

        if (act->is_read()) {
            gettimeofday(&start, NULL);
            processReadFast(act, cvmap.get(act));
            gettimeofday(&finish, NULL);
            stats->processReadTime += ((finish.tv_sec*1000000+finish.tv_usec)-(start.tv_sec*1000000+start.tv_usec));
        }
    }
}

void SCAnalysis::computeCV(action_list_t *list) {
    struct timeval start;
    struct timeval finish;

    gettimeofday(&start, NULL);
    if (fastVersion)
        computeCVFast(list);
    else
        computeCVSlow(list);
    gettimeofday(&finish, NULL);
    stats->computeCVTime += ((finish.tv_sec*1000000+finish.tv_usec)-(start.tv_sec*1000000+start.tv_usec));
}

void SCAnalysis::computeCVSlow(action_list_t *list) {
    bool changed = true;
    bool firsttime = true;
    ModelAction **last_act = (ModelAction **)model_calloc(1, (maxthreads + 1) * sizeof(ModelAction *));

    while (changed) {
        changed = changed&firsttime;
        firsttime = false;
        bool updateFuture = false;

        for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
            ModelAction *act = *it;
            ModelAction *lastact = last_act[id_to_int(act->get_tid())];
            if (act->is_thread_start())
                lastact = execution->get_thread(act)->get_creation();
            last_act[id_to_int(act->get_tid())] = act;
            ClockVector *cv = cvmap.get(act);
            if (cv == NULL) {
                cv = new ClockVector(act->get_cv(), act);
                cvmap.put(act, cv);
            }
            
            if (lastact != NULL) {
                merge(cv, act, lastact);
            }
            if (act->is_thread_join()) {
                Thread *joinedthr = act->get_thread_operand();
                ModelAction *finish = execution->get_last_action(joinedthr->get_id());
                changed |= merge(cv, act, finish);
            }
            if (act->is_read()) {
                /*
                if (fastVersion) {
                    changed |= processReadFast(act, cv);
                } else if (annotatedReadSet.contains(act)) {
                    changed |= processAnnotatedReadSlow(act, cv, &updateFuture);
                } else {
                    changed |= processReadSlow(act, cv, &updateFuture);
                }
                */
                changed |= processReadSlow(act, cv, &updateFuture);
            }
        }
        /* Reset the last action array */
        if (changed) {
            bzero(last_act, (maxthreads + 1) * sizeof(ModelAction *));
        } else {
            if (!allowNonSC) {
                allowNonSC = true;
                changed = true;
            } else {
                break;
            }
        }
    }
    model_free(last_act);
}
