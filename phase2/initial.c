#ifndef MULTIPANDOS_INITIAL_H
#define MULTIPANDOS_INITIAL_H

#include <uriscv/liburiscv.h>
#include "headers/initial.h"
#include "../headers/types.h"

#define CPUS 4

static unsigned int globalLock;
static int processCount;
static pcb_t* currentProcess[CPUS];

unsigned int* global_lock() {
    return &globalLock;
}
int* process_count() {
    return &processCount;
}
pcb_t* current_process() {
    return currentProcess[getPRID()];
}

#endif //MULTIPANDOS_INITIAL_H
