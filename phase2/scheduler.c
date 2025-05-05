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

//risolvere problema softBlockCount

volatile cpu_t sliceStart;

void scheduler() {
    ACQUIRE_LOCK(global_lock()); //prende il GL prima di accedere alla ready queue

    if (emptyProcQ(ready_queue())) {
        //ready queue vuota --> rilascia il lock e poi prosegue
        RELEASE_LOCK(global_lock());
        if (*process_count() == 0) {
            //0 processi attivi --> termina
            HALT();
        } else {
            if (softBlockCount > 0) {
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
            } else {
                //processi attivi ma nessuno in soft block --> DEADLOCk
                PANIC();
            }
        }
    } else {
        //ready queue non vuota --> primo pcb rimosso e assegnazione a currentProcess --> global lock rilasciato
        *current_process() = removeProcQ(ready_queue());
        RELEASE_LOCK(global_lock());
    }

    setTIMER(MUSEC_TO_TICKS(TIMESLICE)); //PLT per times lice a 5ms

    STCK(sliceStart); //

    LDST(&((*current_process())->p_s)); //stato del processore caricato dal pcb corrente
}