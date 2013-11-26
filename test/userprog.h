#ifndef _USERPROG_H
#define _USERPROG_H

#include <stdio.h>
#include <threads.h>
#include <stdatomic.h>

#include "librace.h"


atomic_int x;
atomic_int y;

void a(void *obj)
{
	int r1=atomic_load_explicit(&y, memory_order_relaxed);
	_Z11cdsannotatemPv(0, &r1);
	atomic_store_explicit(&x, r1, memory_order_relaxed);
	_Z11cdsannotatemPv(0, &r1);
	printf("r1=%d\n",r1);
}

void b(void *obj)
{
	//_Z11cdsannotatemPv(0, &a);
	int r2=atomic_load_explicit(&x, memory_order_relaxed);
	//_Z11cdsannotatemPv(0, &r2);
	atomic_store_explicit(&y, 42, memory_order_relaxed);
	//_Z11cdsannotatemPv(0, &r2);
	printf("r2=%d\n",r2);
}

#endif
