#ifndef _INTERPRETER_H
#define _INTERPRETER_H

#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <utility>

/**
	SPEC constructs:
	Each construct should be embraced by /DOUBLE_STAR ... STAR/ annotation.
	Within there, any line beginning with a "#" is a comment of the annotation.
	Each constrcut should begin with @Begin and end with @End. Otherwise, the
	annotation would be considered as normal comments of the source.
	
	a) Global construct
	@Begin
	@Global_define:
		...
	@Interface_cluster:
		...
	@Happens-before:
		...
	@End
	
	b) Interface construct
	@Begin
	@Interface: ...
	@Commit_point_set:
		...
	@Condition: ... (Optional)
	@ID: ... (Optional, use default ID)
	@Check: (Optional)
		...
	@Action: (Optional)
		...
	@Post_action: (Optional)
	@Post_check: (Optional)
	@End

	c) Potential commit construct
	@Begin
	@Potential_commit_point_define: ...
	@Label: ...
	@End

	d) Commit point define construct
	@Begin
	@Commit_point_define_check: ...
	@Label: ...
	@End
	
		OR

	@Begin
	@Commit_point_define: ...
	@Potential_commit_point_label: ...
	@Label: ...
	@End
*/

/**
	Key notes for interpreting the spec into the model checking process:
	1. In the "include/cdsannotate.h" file, it declares a "void
	cdsannotate(uinit64_t analysistype, void *annotation)" function to register
	for an atomic annotation for the purpose trace analysis.

	2. All the @Check, @Action, @Post_action, @Post_check can be wrapped into an
	annotation of the model checker, and it has registered for an
	AnnotationAction which does the internal checks and actions in the trace
	analysis.
*/

using std::map;
using std::string;
using std::vector;

// Forward declaration
class FunctionDeclaration;
class SpecInterpreter;

// A function pointer that abstracts the checks and actions to be done by the
// model checker internally
typedef (void*) (*annotation_action_t)();

class 

class FunctionDeclaration {
	/**
		The following is an example to illustrate how to use this class.

		ReturnType functionName(ArgType1 arg1, ArgType2 arg2, ... ArgTypeN argN)
		{
			...
		}
	*/
	public:
	FunctionDeclaration();
	// Will get "ReturnType" exactly
	string getReturnType();
	// Will get "functionName(arg1, arg2, ... argN)
	string getFunctionCallStatement();
	// Will get N
	int getArgumentNum();
	// argIndex ranges from 0 -- (N - 1). if argIndex == 1, you will get
	// "ArgType2"
	string getNthArgType(int argIndex);
	// argIndex ranges from 0 -- (N - 1). if argIndex == 1, you will get
	// "arg2"
	string getNthArg(int argIndex);
	private:
	// "ReturnType functionName(ArgType1 arg1, ArgType2 arg2, ... ArgTypeN
	// argN)"
	string originalFunctionDefinition();
};

struct 

}

class SpecInterpreter {
	public:
	SpecInterpreter();
	SpecInterpreter(const char* dirname);
	void scanFiles();
	void interpretSpec();
	
	private:
	// Private fields necessary to interpret the spec
	map<string, FunctionDeclaration> _interface2Decl;
	vector<pair<

	void generateFiles();
};

#endif
