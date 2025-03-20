#ifndef MULTIPANDOS_INITIAL_H
#define MULTIPANDOS_INITIAL_H

#include "../../headers/types.h"

#define MUTEX_GLOBAL(expr) ACQUIRE_LOCK(global_lock()); expr; RELEASE_LOCK(global_lock());

unsigned int* global_lock();

int* process_count();

pcb_t* current_process();

#endif