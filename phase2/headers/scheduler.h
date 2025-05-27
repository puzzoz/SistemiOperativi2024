#ifndef MULTIPANDOS_SCHEDULER_H
#define MULTIPANDOS_SCHEDULER_H

#include <uriscv/liburiscv.h>
#include <uriscv/const.h>
#include "./exceptions.h"
#include "../../headers/types.h"

//funzioni
void setTPR(unsigned int value);

_Noreturn void scheduler(void);

void updateProcessCPUTime();

//variabile globale
extern volatile cpu_t sliceStart;

#endif // MULTIPANDOS_SCHEDULER_H