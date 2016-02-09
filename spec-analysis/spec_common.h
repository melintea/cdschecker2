#ifndef _SPEC_COMMON_H
#define _SPEC_COMMON_H

#include <set>
#include "mymemory.h"

#define SPEC_ANALYSIS 1

#define NEW_SIZE(type, size) (type*) malloc(size)
#define NEW(type) NEW_SIZE(type, sizeof(type))

typedef const char *CSTR;

extern CSTR GRAPH_START;
extern CSTR GRAPH_FINISH;

/** Define a snapshotting set for CDSChecker backend analysis */
template<typename _Tp>
class SnapSet : public std::set<_Tp, std::less<_Tp>, SnapshotAlloc<_Tp> >
{   
	public:
	typedef std::set<_Tp, std::less<_Tp>, SnapshotAlloc<_Tp> > set;
	 
	SnapSet() : set() { }

	SNAPSHOTALLOC
};

extern SnapshotAlloc<char> snapshotAlloc;

#endif
