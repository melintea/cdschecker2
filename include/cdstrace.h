#ifndef _CDSTRACE_H
#define _CDSTRACE_H
#include <stdint.h>
#include "common.h"
#include "action.h"
#include "model.h"

ModelAction getLatestAction(thread_id_t tid, int index);

#endif
