#ifndef MULTIPANDOS_SCHEDULER_H
#define MULTIPANDOS_SCHEDULER_H

//togliere/sistemare gli include se necessario (alcune volte non mi riconosceva delle variabili)
#include "../uriscv/src/include/support/liburiscv/liburiscv.h"  
#include "../uriscv/src/include/support/liburiscv/const.h"      
#include "headers/initial.h"
#include "headers/exceptions.h"
#include "headers/interrupts.h"
#include "../phase1/headers/pcb.h"

//settaggio TPR
void setTPR(unsigned int value) {
    *((volatile unsigned int *)TPR) = value;
}

volatile cpu_t sliceStart;

void schedule() {
    ACQUIRE_LOCK(global_lock()); //prende il GL prima di accedere alla ready queue

    if (emptyProcQ(&readyQueue)) {
        //ready queue vuota --> rilascia il lock e poi prosegue
        RELEASE_LOCK(global_lock());

        if (*process_count() == 0) {
            //0 processi attivi --> termina
            HALT();
        }

        if (*process_count() > 0 && softBlockCount > 0) {
            //ci sono processi bloccati --> attesa di interrupt
            //CPU in WAIT --> TPR a 1
            setTPR(1);

            //attivazione interrupt e disattivazione PLT
            setMIE(MIE_ALL & ~MIE_MTIE_MASK);
            unsigned int prevStatus = getSTATUS();
            prevStatus |= MSTATUS_MIE_MASK;
            setSTATUS(prevStatus);

            //valore timer al max per non causare un interrupt
            setTIMER(MUSEC_TO_TICKS(MAXPLT));

            WAIT();

            //ripristina status e TPR a 0
            setSTATUS(prevStatus);
            setTPR(0);
        }

        if (*process_count() > 0 && softBlockCount == 0) {
            //processi attivi ma nessuno in soft block --> DEADLOCk
            PANIC();
        }
    } else {
        //ready queue non vuota --> primo pcb rimosso e assegnazione a currentProcess --> global lock rilasciato
        pcb_t* newProc = removeProcQ(&readyQueue);
        *current_process() = newProc;
        RELEASE_LOCK(global_lock());
    }

    setTIMER(MUSEC_TO_TICKS(TIMESLICE)); //PLT per times lice a 5ms

    STCK(sliceStart); //

    LDST(&((*current_process())->p_s)); //stato del processore caricato dal pcb corrente
}

#endif //MULTIPANDOS_SCHEDULER_H
