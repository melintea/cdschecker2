#include "scanalysis.h"
#include "action.h"
#include "threads-model.h"
#include "clockvector.h"
#include "execution.h"
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
	unsigned long long actionperexec=(stats->actions)/(stats->sccount+stats->nonsccount);
	model_print("Actions per execution: %llu\n", actionperexec);
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
		reset(actions);
		delete list;
		/* Now we found that this is not SC, we run the slow version */
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
	for (action_list_t::iterator it = list->begin(); it != list->end(); it++) {
		const ModelAction *act = *it;
		if (act->is_read()) {
			if (act->get_reads_from() != lastwrmap.get(act->get_location()))
				badrfset.put(act, lastwrmap.get(act->get_location()));
		}
		if (act->is_write())
			lastwrmap.put(act->get_location(), act);
	}
}


bool SCAnalysis::merge(ClockVector *cv, const ModelAction *act, const ModelAction *act2) {
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
			if (merged)
				allowNonSC = false;
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
 	int numactions=buildVectors(list);
	if (fastVersion)
		stats->actions+=numactions;

	computeCV(list);

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
			const ModelAction *write = act->get_reads_from();
			if (write->get_seq_number() != 0) {
				/* All the read actions that read from a different thread */
				ClockVector *writeCV = cvmap.get(write);
				if (writeCV == NULL) {
					writeCV = new ClockVector(NULL, write);
					cvmap.put(write, writeCV);
				}
				merge(cv, act, write);
				updateSet->push_back(act);

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

	for (int i = 0; i <= maxthreads; i++) {
		thread_id_t tid = int_to_id(i);
		if (tid == read->get_tid())
			continue;
		if (tid == write->get_tid())
			continue;
		action_list_t *list = execution->get_actions_on_obj(read->get_location(), tid);
		if (list == NULL)
			continue;
		for (action_list_t::reverse_iterator rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *write2 = *rit;
			if (!write2->is_write())
				continue;

			ClockVector *write2cv = cvmap.get(write2);
			if (write2cv == NULL)
				continue;

			/* write -sc-> write2 &&
				 write -rf-> R =>
				 R -sc-> write2 */
			if (write2cv->synchronized_since(write)) {
				status = merge(write2cv, write2, read);
				if (status)
					passChange(write2);
				changed |= status;
			}

			//looking for earliest write2 in iteration to satisfy this
			/* write2 -sc-> R &&
				 write -rf-> R =>
				 write2 -sc-> write */
			if (cv->synchronized_since(write2)) {
				status = merge(writecv, write, write2);
				if (status)
					passChange(write);
				changed |= status;
				break;
			}
		}
	}
	return changed;
}

bool SCAnalysis::processReadSlow(ModelAction *read, ClockVector *cv, bool * updatefuture) {
	bool changed = false;

	/* Merge in the clock vector from the write */
	const ModelAction *write = read->get_reads_from();
	ClockVector *writecv = cvmap.get(write);
	if ((*write < *read) || ! *updatefuture) {
		bool status = merge(cv, read, write) && (*read < *write);
		changed |= status;
		*updatefuture |= status;
	}

	for (int i = 0; i <= maxthreads; i++) {
		thread_id_t tid = int_to_id(i);
		if (tid == read->get_tid())
			continue;
		if (tid == write->get_tid())
			continue;
		action_list_t *list = execution->get_actions_on_obj(read->get_location(), tid);
		if (list == NULL)
			continue;
		for (action_list_t::reverse_iterator rit = list->rbegin(); rit != list->rend(); rit++) {
			ModelAction *write2 = *rit;
			if (!write2->is_write())
				continue;

			ClockVector *write2cv = cvmap.get(write2);
			if (write2cv == NULL)
				continue;

			/* write -sc-> write2 &&
				 write -rf-> R =>
				 R -sc-> write2 */
			if (write2cv->synchronized_since(write)) {
				if ((*read < *write2) || ! *updatefuture) {
					bool status = merge(write2cv, write2, read);
					changed |= status;
					*updatefuture |= status && (*write2 < *read);
				}
			}

			//looking for earliest write2 in iteration to satisfy this
			/* write2 -sc-> R &&
				 write -rf-> R =>
				 write2 -sc-> write */
			if (cv->synchronized_since(write2)) {
				if ((*write2 < *write) || ! *updatefuture) {
					bool status = writecv == NULL || merge(writecv, write, write2);
					changed |= status;
					*updatefuture |= status && (*write < *write2);
				}
				break;
			}
		}
	}
	return changed;
}

void SCAnalysis::passChange(const ModelAction *act) {
	/* Update the CV of the to node */
	action_node *node = nodeMap.get(act);
	if (node == NULL)
		return;
	const ModelAction *nextAct = node->sb;
	if (nextAct)
		if (merge(cvmap.get(nextAct), nextAct, act))
			updateSet->push_back(nextAct);
	nextAct = node->specialEdge;
	if (nextAct)
		if (merge(cvmap.get(nextAct), nextAct, act))
			updateSet->push_back(nextAct);
	/*
	if (node->otherActs) {
		for (const_actions_t::iterator it = node->otherActs->begin(); it !=
			node->otherActs->end(); it++) {
			nextAct = *it;
			if (merge(cvmap.get(nextAct), nextAct, act))
				updateSet->push_back(nextAct);
		}
	}
	*/
}


void SCAnalysis::changeBasedComputeCV(action_list_t *list) {
	/* We now only use this approach for the fast version */

	/* A BFS-like approach */
	while (updateSet->size() > 0) {
		const ModelAction *act = updateSet->front();
		updateSet->pop_front();

		/* Update the CV of the to node */
		passChange(act);
		if (act->is_read()) {
			processReadFast(act, cvmap.get(act));
		}
	}
}

void SCAnalysis::computeCV(action_list_t *list) {
	if (fastVersion)
		changeBasedComputeCV(list);
	else
		normalComputeCV(list);
}

void SCAnalysis::normalComputeCV(action_list_t *list) {
	bool changed = true;
	bool firsttime = true;
	ModelAction **last_act = (ModelAction **)model_calloc(1, (maxthreads + 1) * sizeof(ModelAction *));
	while (changed) {
		changed = changed&firsttime;
		firsttime = false;
		bool updatefuture=false;

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
				if (fastVersion)
					changed |= processReadFast(act, cv);
				else
					changed |= processReadSlow(act, cv, &updatefuture);
			}
		}
		/* Reset the last action array */
		if (changed) {
			bzero(last_act, (maxthreads + 1) * sizeof(ModelAction *));
		} else {
			if (!fastVersion) {
				if (!allowNonSC) {
					allowNonSC = true;
					changed = true;
				} else {
					break;
				}
			}
		}
	}
	model_free(last_act);
}
