#ifndef MULTIPANDOS_EXCEPTIONS_H
#define MULTIPANDOS_EXCEPTIONS_H

#include "headers/exceptions.h"
#include "headers/interrupts.h"
#include "headers/initial.h"
#include "headers/scheduler.h"
#include "../phase1/headers/pcb.h"
#include "../phase1/headers/asl.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include "../headers/listx.h"
#include "../headers/types.h"


#define EXCEPTION_IN_KERNEL_MODE(excStatus) (excStatus->status & MSTATUS_MPP_MASK)
#define EXC_RETURN(excState, x) {excState->reg_a0 = x; break;}
#define CAUSE_GET_EXCCODE(x) ((x)&CAUSE_EXCCODE_MASK)


void interruptExcHandler() { dispatchInterrupt(getCAUSE(), GET_EXCEPTION_STATE_PTR(getPRID())); }

void syscallExcHandler() {
    state_t* excState = GET_EXCEPTION_STATE_PTR(getPRID());
    if (EXCEPTION_IN_KERNEL_MODE(excState) && excState->reg_a0 < 0) {
        MUTEX_GLOBAL(pcb_t* curr_p = current_process(getPRID()))
        switch (excState->reg_a0) {
            case CREATEPROCESS: {
                MUTEX_GLOBAL(
                    pcb_t* new_p = allocPcb();
                    if (new_p == NULL) EXC_RETURN(excState, -1)
                    // aggiunge il nuovo processo all'albero dei processi
                    insertProcQ(&curr_p->p_list, new_p);
                    insertChild(curr_p, new_p);
                    // IL PROCESSO DEVE ESSERE AGGIUNTO ALLA READY QUEUE
                    (*process_count())++;
                )
                new_p->p_s = *((state_t*)excState->reg_a1);
                new_p->p_supportStruct = (support_t *)excState->reg_a3;
                EXC_RETURN(excState, new_p->p_pid)
            }
            case TERMPROCESS:
                MUTEX_GLOBAL(freePcb(curr_p); (*process_count())--)
                break;
            case PASSEREN: {
                semd_t *sem = ((semd_t *) excState->reg_a1);
                MUTEX_GLOBAL(if (*sem->s_key > 0) {
                    (*sem->s_key)--;
                } else {
                    insertBlocked(sem->s_key, curr_p);
                    (*process_count())--;
                    softBlockCount++;
                    scheduler();
                })
                break;
            }
            case VERHOGEN: {
                semd_t *sem = ((semd_t *) excState->reg_a1);
                MUTEX_GLOBAL(if (*sem->s_key < 1) {
                    (*sem->s_key)++;
                } else {
                    insertBlocked(sem->s_key, curr_p);
                    // INSERIRE CHIAMATA ALLO SCHEDULER
                })
                break;
            }
            case DOIO://Punto 12
                //blocco il processo sul semaforo a cui fa riferimento commandAddr
                MUTEX_GLOBAL(
                        curr_p->p_semAdd = (int *)excState->reg_a1;
                        insertBlocked((int *)excState->reg_a1, curr_p);
                        (*process_count())--;
                        softBlockCount++;
                );
                //Aggiorno il tempo Utilizzato
                cpu_t now;
                STCK(now);
                curr_p->p_time += now - sliceStart;
                //Salvo il contesto nel pcb e incremento il PC
                curr_p->p_s = *excState;
                excState->pc_epc += 4;
                //Scrivo il comando nel Registro
                *((int *)excState->reg_a1) = excState->reg_a2;
                scheduler();
                break;
                //fine punto 12
            case GETTIME: EXC_RETURN(excState, curr_p->p_time)
            case CLOCKWAIT: {
                MUTEX_GLOBAL(
                        insertBlocked(&clock_sem, curr_p);
                        (*process_count())--;
                        softBlockCount++;
                        scheduler();
                )
                break;
            }
            case GETSUPPORTPTR: EXC_RETURN(excState, (unsigned int) curr_p->p_supportStruct)
            case GETPROCESSID: {
                if (excState->reg_a1 == 0) {
                    EXC_RETURN(excState, curr_p->p_pid)
                } else {
                    EXC_RETURN(excState, (curr_p->p_parent != NULL) ? curr_p->p_parent->p_pid : 0)
                }
            }
        }
    }
}

void programTrapExcHandler() {}

void tlbExcHandler() {}

void exceptionHandler() {
    unsigned int cause = getCAUSE();
    if (CAUSE_IS_INT(cause)) interruptExcHandler(); // interrupt exception handling
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

void terminte_process_tree(pcb_t* p){
    //se il processo ha un genitore, lo perde e viene rimosso dalla lista dei figli
    if (p->p_parent != NULL)
        outChild(p);

    //termino ricorsivamente tutti i figli di p
    while (!emptyChild(p)) {
        pcb_t *child = removeChild(p);
        terminte_process_tree(child);
    }

    (*process_count())--;

    freePcb(p);
}

#endif //MULTIPANDOS_EXCEPTIONS_H
