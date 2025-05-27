#include <uriscv/liburiscv.h>
#include <uriscv/const.h>
#include "headers/initial.h"
#include "headers/scheduler.h"

#define MUSEC_TO_TICKS(T) ((T) * (*((cpu_t *)TIMESCALEADDR)))

#define MAXPLT 0xFFFFFFFFUL

//settaggio TPR
void setTPR(unsigned int value) {
    *((volatile unsigned int *)TPR) = value;
}

volatile cpu_t sliceStart;

void updateProcessCPUTime() {
    cpu_t now;
    STCK(now);
    (CURRENT_PROCESS)->p_time += (now - sliceStart);
}

_Noreturn void scheduler() {
    while (1) {
        pcb_t *nextProc = NULL;
        MUTEX_GLOBAL(
                nextProc = removeProcQ(&readyQueue);
        )
        if (nextProc != NULL) {
            CURRENT_PROCESS = nextProc;
            setTIMER(MUSEC_TO_TICKS(TIMESLICE));
            STCK(sliceStart);
            LDST(&(nextProc->p_s));
        }

        if (processCount == 0) {
            HALT();  // Tutti i processi sono morti
        }
        // CPU in idle
        setTPR(1);
        setMIE(MIE_ALL & ~MIE_MTIE_MASK);
        unsigned int status = getSTATUS();
        status |= MSTATUS_MIE_MASK;
        setSTATUS(status);
        setTIMER(MUSEC_TO_TICKS(MAXPLT));
        WAIT();     // Quando arriva un interrupt, riprova
        setTPR(0);
    }
}


