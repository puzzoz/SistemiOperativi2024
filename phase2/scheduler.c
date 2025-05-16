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
    (*current_process())->p_time += (now - sliceStart);
}

void scheduler() {
    ACQUIRE_LOCK(&globalLock);
    if (!emptyProcQ(&readyQueue)) {
        *current_process() = removeProcQ(&readyQueue);
        setTPR(0);
        setTIMER(MUSEC_TO_TICKS(TIMESLICE)); //PLT per time slice a 5ms
        STCK(sliceStart);
        updateProcessCPUTime();
        RELEASE_LOCK(&globalLock);
        LDST(&((*current_process())->p_s)); //stato del processore caricato dal pcb corrente
    } else {
        if (processCount == 0) {
            RELEASE_LOCK(&globalLock);
            //0 processi attivi --> termina
            unsigned int *irt_entry = (unsigned int *) IRT_START;
            for (int i = 0; i < IRT_NUM_ENTRY; i++) {
                *irt_entry = getPRID();
                irt_entry++;
            }
            HALT();
        } else {
            RELEASE_LOCK(&globalLock);
            //ci sono processi bloccati --> attesa di interrupt
            //CPU in WAIT --> TPR a 1
            setTPR(1);
            //attivazione interrupt e disattivazione PLT
            setMIE(MIE_ALL & ~MIE_MTIE_MASK);
            unsigned int status = getSTATUS();
            status |= MSTATUS_MIE_MASK;
            setSTATUS(status);
            //valore timer al max per non causare un interrupt
            setTIMER(MUSEC_TO_TICKS(MAXPLT));
            WAIT();
            //ripristina status e TPR a 0
            setTPR(0);
        }
    }
}