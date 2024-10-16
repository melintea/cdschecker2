/**
 * @file context.h
 * @brief ucontext header, since Mac OSX swapcontext() is broken
 */

#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include "common.h"
#include <ucontext.h>

#ifdef MAC

int model_swapcontext(ucontext_t *oucp, ucontext_t *ucp);

#else /* !MAC */

static inline int model_swapcontext(ucontext_t *oucp, ucontext_t *ucp)
{
    DEBUG("swapcontext %p => %p\n", oucp, ucp);
    return swapcontext(oucp, ucp);
}

#endif /* !MAC */

#endif /* __CONTEXT_H__ */
