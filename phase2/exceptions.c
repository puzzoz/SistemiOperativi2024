#ifndef MULTIPANDOS_EXCEPTIONS_H
#define MULTIPANDOS_EXCEPTIONS_H

#include "headers/exceptions.h"
#include "headers/interrupts.h"
#include "headers/initial.h"
#include "headers/scheduler.h"
#include "../phase1/headers/asl.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>

#define EXCEPTION_IN_KERNEL_MODE(excStatus) (excStatus->status & MSTATUS_MPP_MASK)
#define CAUSE_GET_EXCCODE(x) ((x)&CAUSE_EXCCODE_MASK)


void blockPcb(int *sem_key, pcb_t *pcb, state_t *excState) {
    if (!insertBlocked(sem_key, pcb)) {
        (*process_count())--;
        softBlockCount++;
        //Aggiorno il tempo Utilizzato
        cpu_t now;
        STCK(now);
        pcb->p_time += now - sliceStart;
        //Salvo il contesto nel pcb e incremento il PC
        pcb->p_s = *excState;
        excState->pc_epc += 4;
    }
}

/*
 * Rimuove un pcb e tutti i processi figli dalla ready queue
 * e da qualunque processo o semaforo con un riferimento ad esso.
 *
 * Da chiamare in un contesto di mutua esclusione
*/
void removePcb(pcb_t* pcb) { // NOLINT(*-no-recursion)
    outChild(pcb);
    (*process_count())--;
    if (pcb->p_semAdd != NULL && outBlocked(pcb) != NULL) {
        softBlockCount--;
    }
    outProcQ(ready_queue(), pcb);
    pcb_t *child;
    while ((child = removeChild(pcb)) != NULL) {
        removePcb(child);
    }
    freePcb(pcb);
}

void passUpOrDie(unsigned int excIndex) { MUTEX_GLOBAL(
    pcb_t *curr_p = *current_process();
    context_t passUpContext;
    unsigned int dead = 0;
    if (curr_p->p_supportStruct != NULL) {
        // Pass Up
        state_t* excState = GET_EXCEPTION_STATE_PTR(getPRID());
        curr_p->p_supportStruct->sup_exceptState[excIndex] = *excState;
        passUpContext = curr_p->p_supportStruct->sup_exceptContext[excIndex];
    } else {
        // Die
        removePcb(curr_p);
        dead = 1; // non esegue LDCXT
    })
    if (!dead) LDCXT(passUpContext.stackPtr, passUpContext.status, passUpContext.pc);
}

void interruptExcHandler() { dispatchInterrupt(getCAUSE(), GET_EXCEPTION_STATE_PTR(getPRID())); }

void programTrapExcHandler() { passUpOrDie(GENERALEXCEPT); }

void createProcess(state_t *excState) {
    MUTEX_GLOBAL(
        pcb_t *curr_p = *current_process();
        pcb_t *new_p = allocPcb();
        if (new_p == NULL) {
            excState->reg_a0 = -1;
        } else {
            // aggiunge il nuovo processo all'albero dei processi
            insertProcQ(&curr_p->p_list, new_p);
            insertChild(curr_p, new_p);
            // aggiunge il processo alla ready queue
            insertProcQ(ready_queue(), new_p);
            (*process_count())++;
        }
    )
    new_p->p_s = *((state_t *) excState->reg_a1);
    new_p->p_supportStruct = (support_t *) excState->reg_a3;
    excState->reg_a0 = new_p->p_pid;
}
void termProcess() {
    MUTEX_GLOBAL(
        freePcb(*current_process());
        (*process_count())--;
    )
}
unsigned int passeren(int *sem, state_t *excState) {
    unsigned int blocked = 0;
    MUTEX_GLOBAL(
        if (*sem <= 0) {
            blockPcb(sem, *current_process(), excState);
            blocked = 1;
        } else if (headBlocked(sem) != NULL) {
            // il semaforo blocca un processo
            insertProcQ(ready_queue(), removeBlocked(sem));
        } else {
            (*sem)--;
        }
    )
    return blocked;
}
unsigned int verhogen(int *sem, state_t *excState) {
    unsigned int blocked = 0;
    MUTEX_GLOBAL(
        if (*sem >= 1) {
            blockPcb(sem, *current_process(), excState);
            blocked = 1;
        } else if (headBlocked(sem) != NULL) {
            // il semaforo blocca un processo
            insertProcQ(ready_queue(), removeBlocked(sem));
        } else {
            (*sem)++;
        }
    )
    return blocked;
}
void doio(state_t *excState) {
    devreg_t *dev = (devreg_t *) excState->reg_a1;
    unsigned int blocked = passeren(device_semaphores(DEV_NO_BY_DEV_ADDR(dev)), excState);
    dev->dtp.command = excState->reg_a2;
    if (blocked) scheduler();
    excState->reg_a0 = dev->dtp.status;
}
void getTime(state_t *excState) {
    MUTEX_GLOBAL(
        excState->reg_a0 = (*current_process())->p_time
    )
}
void clockWait(state_t *excState) {
    MUTEX_GLOBAL(
        blockPcb(&clock_sem, *current_process(), excState);
    )
    scheduler();
}
void getSupportPointer(state_t *excState) {
    MUTEX_GLOBAL(
        excState->reg_a0 = (unsigned int) (*current_process())->p_supportStruct
    )
}
void getProcessId(state_t *excState) {
    MUTEX_GLOBAL(
        pcb_t *curr_pr = *current_process();
        excState->reg_a0 = (excState->reg_a1 == 0) ?
            curr_pr->p_pid
            : (curr_pr->p_parent != NULL) ? curr_pr->p_parent->p_pid : 0
    )
}

void syscallExcHandler() {
    state_t* excState = GET_EXCEPTION_STATE_PTR(getPRID());
    int syscall = excState->reg_a0; // non salvare il valore del registro in una variabile causa comportamenti inaspettati
    if (syscall <= 0) {
        // negative SYSCALL
        if (EXCEPTION_IN_KERNEL_MODE(excState)) {
            switch (syscall) {
                case CREATEPROCESS:
                    createProcess(excState); break;
                case TERMPROCESS:
                    termProcess(); break;
                case PASSEREN:
                    if (passeren(((int *) excState->reg_a1), excState)) scheduler();
                    break;
                case VERHOGEN:
                    if (verhogen(((int *) excState->reg_a1), excState)) scheduler();
                    break;
                case DOIO:
                    doio(excState); break;
                case GETTIME:
                    getTime(excState); break;
                case CLOCKWAIT:
                    clockWait(excState); break;
                case GETSUPPORTPTR:
                    getSupportPointer(excState); break;
                case GETPROCESSID:
                    getProcessId(excState); break;
                default:
                    break;
            }
        } else {
            // attempting negative SYSCALL in user mode
            excState->cause = PRIVINSTR;
            programTrapExcHandler();
        }
    } else {
        // positive SYSCALL, standard Pass Up or Die
        passUpOrDie(GENERALEXCEPT);
    }
}

void tlbExcHandler() { passUpOrDie(PGFAULTEXCEPT); }

void exceptionHandler() {
    unsigned int cause = getCAUSE();
    if (CAUSE_IS_INT(cause)) interruptExcHandler(); // interrupt exception handling
    else {
        switch (CAUSE_GET_EXCCODE(cause)) {
            case EXC_ECU:
            case EXC_ECM:
                // SYSCALL exception handling
                syscallExcHandler();
                break;
            case EXC_IAM ... EXC_SAF:
            case EXC_ECS ... (EXC_ECM-1):
            case EXC_IPF ... (EXC_MOD-1):
                // Program Trap exception handling
                programTrapExcHandler();
                break;
            case EXC_MOD ... EXC_UTLBS:
                // TLB exception handling
                tlbExcHandler();
                break;
        }
    }
}

#endif //MULTIPANDOS_EXCEPTIONS_H
