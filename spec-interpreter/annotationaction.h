#ifndef _ANNOTATIONACTION_H
#define _ANNOTATIONACTION_H

typedef struct cond_args {
	int arg_num;
	void* arg_ptrs[];
} cond_args_t;

/**
	This class abstracts the execution of the condition check for the
	happpens-before relationship. For example, PutIfMatch(__RET__ != NULL) ->
	Get. The "__RET__ != NULL" here is the HB condition.
*/
class HBConditionExecutor {
	public:
	HBConditionExecutor(void *_func_ptr, void **_args) :
		func_ptr(_func_ptr),
		args(_args)
	{
	}

	virtual bool execute() = 0;

	private:
	void *func_ptr;
	cond_args_t args;
};

// A function pointer that abstracts the checks and actions to be done by the
// model checker internally
typedef (void*) (*annotation_action_t)();

class InterfaceAction {
	public:
	annotation_action_t getIDAction();
	annotation_action_t getCheckAction();
	annotation_action_t getPostCheckAction();

	private:

}

#endif
