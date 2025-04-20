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
#define EXC_RETURN(excState, x) {excState->reg_a0 = x; break;}
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
    if (curr_p->p_supportStruct != NULL) {
        // Pass Up
        state_t* excState = GET_EXCEPTION_STATE_PTR(getPRID());
        curr_p->p_supportStruct->sup_exceptState[excIndex] = *excState;
        passUpContext = curr_p->p_supportStruct->sup_exceptContext[excIndex];
    } else {
        // Die
        removePcb(curr_p);
        return; // non esegue LDCXT
    })
    LDCXT(passUpContext.stackPtr, passUpContext.status, passUpContext.pc);
}

void interruptExcHandler() { dispatchInterrupt(getCAUSE(), GET_EXCEPTION_STATE_PTR(getPRID())); }

void programTrapExcHandler() { passUpOrDie(GENERALEXCEPT); }

void syscallExcHandler() {
    state_t* excState = GET_EXCEPTION_STATE_PTR(getPRID());
    if (excState->reg_a0 <= 0) {
        // negative SYSCALL
        if (EXCEPTION_IN_KERNEL_MODE(excState)) {
            switch (excState->reg_a0) {
                case CREATEPROCESS: {
                    MUTEX_GLOBAL(
                            pcb_t *curr_p = *current_process();
                            pcb_t *new_p = allocPcb();
                            if (new_p == NULL) EXC_RETURN(excState, -1)
                            // aggiunge il nuovo processo all'albero dei processi
                            insertProcQ(&curr_p->p_list, new_p);
                            insertChild(curr_p, new_p);
                            // aggiunge il processo alla ready queue
                            insertProcQ(ready_queue(), new_p);
                            (*process_count())++;
                    )
                    new_p->p_s = *((state_t *) excState->reg_a1);
                    new_p->p_supportStruct = (support_t *) excState->reg_a3;
                    EXC_RETURN(excState, new_p->p_pid)
                }
                case TERMPROCESS:
                MUTEX_GLOBAL(
                        freePcb(*current_process());
                        (*process_count())--;
                )
                    break;
                case PASSEREN: {
                    semd_t *sem = ((semd_t *) excState->reg_a1);
                    unsigned int callScheduler = 0;
                    MUTEX_GLOBAL(if (*sem->s_key > 0) {
                        (*sem->s_key)--;
                    } else {
                        blockPcb(sem->s_key, *current_process(), excState);
                        callScheduler = 1;
                    })
                    if (callScheduler) scheduler();
                    break;
                }
                case VERHOGEN: {
                    semd_t *sem = ((semd_t *) excState->reg_a1);
                    unsigned int callScheduler = 0;
                    MUTEX_GLOBAL(if (*sem->s_key < 1) {
                        (*sem->s_key)++;
                    } else {
                        blockPcb(sem->s_key, *current_process(), excState);
                        callScheduler = 1;
                    })
                    if (callScheduler) scheduler();
                    break;
                }
                case DOIO:
                    //blocco il processo sul semaforo a cui fa riferimento commandAddr
                MUTEX_GLOBAL(
                        pcb_t *curr_p = *current_process();
                        curr_p->p_semAdd = (int *) excState->reg_a1;
                        blockPcb((int *) excState->reg_a1, curr_p, excState);
                )
                    //Scrivo il comando nel Registro
                    excState->reg_a1 = excState->reg_a2;
                    scheduler();
                    break;
                case GETTIME:
                MUTEX_GLOBAL(
                        EXC_RETURN(excState, (*current_process())->p_time))
                case CLOCKWAIT: {
                    MUTEX_GLOBAL(
                            blockPcb(&clock_sem, *current_process(), excState);
                    )
                    scheduler();
                    break;
                }
                case GETSUPPORTPTR: EXC_RETURN(excState, (unsigned int) (*current_process())->p_supportStruct)
                case GETPROCESSID:
                MUTEX_GLOBAL(
                        pcb_t *curr_pr = *current_process();
                        if (excState->reg_a1 == 0) {
                            EXC_RETURN(excState, curr_pr->p_pid)
                        } else {
                            EXC_RETURN(excState, (curr_pr->p_parent != NULL) ? curr_pr->p_parent->p_pid : 0)
                        }
                )
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
