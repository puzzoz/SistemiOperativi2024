#ifndef MULTIPANDOS_SCHEDULER_H
#define MULTIPANDOS_SCHEDULER_H

#include "../uriscv/src/include/support/liburiscv/liburiscv.h"
#include "../uriscv/src/include/support/liburiscv/const.h"
#include "headers/initial.h"
#include "headers/exceptions.h"
#include "headers/interrupts.h"
#include "../phase1/headers/pcb.h"

//funzioni
void setTPR(unsigned int value);
void schedule(void);

//variabile globale
extern volatile cpu_t sliceStart;

#endif // MULTIPANDOS_SCHEDULER_H
