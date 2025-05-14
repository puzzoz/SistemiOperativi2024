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
    MUTEX_GLOBAL(
            *current_process() = removeProcQ(&readyQueue);
            int next = emptyProcQ(&readyQueue);
    )
    if (next) { //se la ready queue e' vuota
        if (processCount == 0) {      //0 processi attivi --> termina
            HALT();
        } else {
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
    setTIMER(MUSEC_TO_TICKS(TIMESLICE)); //PLT per time slice a 5ms
    STCK(sliceStart);
    LDST(&((*current_process())->p_s)); //stato del processore caricato dal pcb corrente
}