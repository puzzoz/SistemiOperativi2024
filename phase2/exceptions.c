#include "headers/exceptions.h"
#include "headers/interrupts.h"
#include "headers/initial.h"
#include "headers/scheduler.h"
#include "../phase1/headers/asl.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#define EXCEPTION_IN_KERNEL_MODE(excStatus) (excStatus->status & MSTATUS_MPP_MASK)
#define CAUSE_GET_EXCCODE(x) ((x)&CAUSE_EXCCODE_MASK)
#define CURR_EXCEPTION_STATE GET_EXCEPTION_STATE_PTR(getPRID())

/*
 * Blocca un pcb nel semaforo con chiave in sem_key, e
 * aggiorna lo stato in excState da caricare con LDST
 * in seguito all'esecuzione dell'exception
 *
 * Da chiamare in un contesto di mutua esclusione
 */
void blockPcb(int *sem_key, pcb_t *pcb) {
    if (!insertBlocked(sem_key, pcb)) {
        outProcQ(&readyQueue, pcb);
        softBlockCount++;
        //Aggiorno il tempo Utilizzato
        updateProcessCPUTime();
        //Salvo il contesto nel pcb e incremento il PC
        state_t *excState = CURR_EXCEPTION_STATE;
        excState->pc_epc += 4;
        pcb->p_s = *excState;
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
    processCount--;
    if (pcb->p_semAdd != NULL && outBlocked(pcb) != NULL) {
        softBlockCount--;
    }
    outProcQ(&readyQueue, pcb);
    pcb_t *child;
    while ((child = removeChild(pcb)) != NULL) {
        removePcb(child);
    }
    freePcb(pcb);
}

void passUpOrDie(unsigned int excIndex) {
    MUTEX_GLOBAL(
        pcb_t *curr_p = *current_process();
        context_t passUpContext;
        unsigned int dead = 0;
        if (curr_p->p_supportStruct != NULL) {
            // Pass Up
            state_t* excState = CURR_EXCEPTION_STATE;
            curr_p->p_supportStruct->sup_exceptState[excIndex] = *excState;
            passUpContext = curr_p->p_supportStruct->sup_exceptContext[excIndex];
        } else {
            // Die
            removePcb(curr_p);
            dead = 1; // non esegue LDCXT
        }
    )
    if (!dead) {
        LDCXT(passUpContext.stackPtr, passUpContext.status, passUpContext.pc);
    } else {
        scheduler();
    }
}

void interruptExcHandler() { dispatchInterrupt(getCAUSE(), GET_EXCEPTION_STATE_PTR(getPRID())); }

void programTrapExcHandler() {
    // rilascia il lock nel caso l'eccezione sia stata lanciata in un contesto di mutua esclusione
    RELEASE_LOCK(&globalLock);
    passUpOrDie(GENERALEXCEPT);
}

void createProcess() {
    state_t *excState = CURR_EXCEPTION_STATE;
    MUTEX_GLOBAL(
        pcb_t *curr_p = *current_process();
        pcb_t *new_p = allocPcb();
        if (new_p == NULL) {
            excState->reg_a0 = -1;
        } else {
            // aggiunge il nuovo processo all'albero dei processi
            insertChild(curr_p, new_p);
            // aggiunge il processo alla ready queue
            insertProcQ(&readyQueue, new_p);
            processCount++;
        }
    )
    new_p->p_s = *((state_t *) excState->reg_a1);
    new_p->p_supportStruct = (support_t *) excState->reg_a3;
    excState->reg_a0 = new_p->p_pid;
}
void termProcess() {
    state_t *excState = CURR_EXCEPTION_STATE;
    MUTEX_GLOBAL(removePcb(excState->reg_a1 != 0 ? (pcb_t *) excState->reg_a1 : *current_process()))
    scheduler();
}
unsigned int passeren(int *sem) {
    unsigned int blocked = 0;
    MUTEX_GLOBAL(
        if (*sem <= 0) {
            blockPcb(sem, *current_process());
            blocked = 1;
        } else if (headBlocked(sem) != NULL) {
            // il semaforo blocca un processo
            insertProcQ(&readyQueue, removeBlocked(sem));
            softBlockCount--;
        } else {
            *sem = 0;
        }
    )
    return blocked;
}
unsigned int verhogen(int *sem) {
    unsigned int blocked = 0;
    MUTEX_GLOBAL(
        if (*sem >= 1) {
            blockPcb(sem, *current_process());
            blocked = 1;
        } else if (headBlocked(sem) != NULL) {
            insertProcQ(&readyQueue, removeBlocked(sem));
            softBlockCount--;
        } else {
            *sem = 1;
        }
    )
    return blocked;
}
void doio() {
    state_t *excState = CURR_EXCEPTION_STATE;
    unsigned int *commandAddr = (unsigned int *) excState->reg_a1;
    unsigned int blocked = passeren(device_semaphores(DEV_NO_BY_DEV_ADDR(commandAddr - offsetof(dtpreg_t, command))));
    *commandAddr = excState->reg_a2;
    if (blocked) scheduler();
}
void getTime() {
    MUTEX_GLOBAL(
            cpu_t now;
            STCK(now);
            CURR_EXCEPTION_STATE->reg_a0 = (*current_process())->p_time + (now - sliceStart);
    )
}
void clockWait() {
    if (passeren(device_semaphores(PSEUDO_CLOCK_SEM))) scheduler();
}
void getSupportPointer() {
    MUTEX_GLOBAL(
        CURR_EXCEPTION_STATE->reg_a0 = (unsigned int) (*current_process())->p_supportStruct
    )
}
void getProcessId() {
    state_t *excState = CURR_EXCEPTION_STATE;
    MUTEX_GLOBAL(
        pcb_t *curr_pr = *current_process();
        excState->reg_a0 = (excState->reg_a1 == 0) ?
            curr_pr->p_pid
            : (curr_pr->p_parent != NULL) ? curr_pr->p_parent->p_pid : 0
    )
}

/*
 * Esegue una SYSCALL in base al valore contenuto nel registro a0
 * dell'exception state salvato dal processo corrente.
 *
 * Se il codice della SYSCALL è positivo viene eseguita una Pass Up
 * Or Die, se invece il codice è negativo ma il processo che sta
 * tentando la SYSCALL è in User Mode viene lanciata una Program Trap.
 */
void syscallExcHandler() {
    state_t* excState = CURR_EXCEPTION_STATE;
    if (((int)excState->reg_a0) <= 0) {
        // negative SYSCALL
        if (EXCEPTION_IN_KERNEL_MODE(excState)) {
            switch ((int) excState->reg_a0) {
                case CREATEPROCESS:
                    createProcess(); break;
                case TERMPROCESS:
                    termProcess(); break;
                case PASSEREN:
                    if (passeren((int *) excState->reg_a1)) scheduler();
                    break;
                case VERHOGEN:
                    if (verhogen((int *) excState->reg_a1)) scheduler();
                    break;
                case DOIO:
                    doio(); break;
                case GETTIME:
                    getTime(); break;
                case CLOCKWAIT:
                    clockWait(); break;
                case GETSUPPORTPTR:
                    getSupportPointer(); break;
                case GETPROCESSID:
                    getProcessId(); break;
                default:
                    programTrapExcHandler(); break;
            }
            // return from NSYS, increase PC and load the state
            excState->pc_epc += 4;
            LDST(excState);
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

void tlbExcHandler() {
    // rilascia il lock nel caso l'eccezione sia stata lanciata in un contesto di mutua esclusione
    RELEASE_LOCK(&globalLock);
    passUpOrDie(PGFAULTEXCEPT);
}

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

// commentato fino a fase 3 per non modificare il file p2test.c
/*void uTLB_RefillHandler() {

    int prid = getPRID();
    setENTRYHI(0x80000000);
    setENTRYLO(0x00000000);
    TLBWR();

    LDST(GET_EXCEPTION_STATE_PTR(prid));
}*/