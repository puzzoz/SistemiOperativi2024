#include "headers/interrupts.h"
#include <uriscv/const.h>
#include "../headers/types.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include "headers/initial.h"
#include "headers/scheduler.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/asl.h"

#define DEVREGADDR(line, no) (RAMBASEADDR + 0x54 + ((line - 3) * DEVREGSIZE * DEVPERINT) + (no * DEVREGSIZE))


// Interrupt del clock di sistema (ogni 100ms): sblocca i processi in wait clock
static void handlePseudoClockInterrupt(state_t *exception_state) {
    LDIT(PSECOND);
    MUTEX_GLOBAL(
        int *pseudoClockSem = device_semaphores(PSEUDO_CLOCK_SEM);
        pcb_t *unblocked;
        while ((unblocked = removeBlocked(pseudoClockSem)) != NULL) {
            insertProcQ(ready_queue(), unblocked);
            softBlockCount--;
        }
        unblocked = *current_process();
    )
    if (unblocked != NULL) {
        LDST(exception_state);
    } else {
        scheduler();
    }
}


// Interrupt del PLT: fine time slice per il processo corrente
static void handlePLTInterrupt(state_t *exception_state) {
    setTIMER(-1); // ACK del PLT
    MUTEX_GLOBAL(
            updateProcessCPUTime();
            // Copia lo stato processore dal contesto salvato al PCB
            (*current_process())->p_s = *exception_state;
            insertProcQ(ready_queue(), *current_process()); // rimettilo in ready
            )
    scheduler();
}

// Calcola l'indice del semaforo dato numero linea e device
static int getDeviceIndex(int line, int dev_no) {
    return ((line - 3) * 8) + dev_no;
}

// Interrupt da device: trova chi ha fatto interrupt, fa ACK e sblocca il processo
void handleDeviceInterrupt(int excCode, state_t *exception_state) {
    int line = -1;
    switch (excCode) {
        case IL_DISK:     line = 3; break;
        case IL_FLASH:    line = 4; break;
        case IL_ETHERNET: line = 5; break;
        case IL_PRINTER:  line = 6; break;
        case IL_TERMINAL: line = 7; break;
        default: return; // caso di errore di sicurezza
    }

    // Legge il bitmap
    unsigned int bitmap = *((unsigned int *)(0x10000040 + (line - 3) * 4));

    // Trova quale device ha fatto interrupt
    int dev_no = -1;
    for (int i = 0; i < 8; i++) {
        if (bitmap & (1u << i)) {
            dev_no = i;
            break;
        }
    }
    if (dev_no < 0) {
        return; // Nessun device pending
    }

    int sem_index = getDeviceIndex(line, dev_no);
    unsigned int dev_status = 0;

    // Gestione terminali
    if (excCode == IL_TERMINAL) {
        termreg_t *term = (termreg_t *)DEVREGADDR(line, dev_no);
        int transm_sem = sem_index;
        int recv_sem = sem_index + 1;
        int hasTransmBlocked, hasRecvBlocked;
        MUTEX_GLOBAL(
                hasTransmBlocked = (headBlocked(device_semaphores(transm_sem)) != NULL);
                hasRecvBlocked = (headBlocked(device_semaphores(recv_sem)) != NULL);
        )

        if (hasTransmBlocked) {
            dev_status = term->transm_status;
            term->transm_command = ACK;
            sem_index = transm_sem;
        } else if (hasRecvBlocked) {
            dev_status = term->recv_status;
            term->recv_command = ACK;
            sem_index = recv_sem;
        } else {
            if ((term->transm_status & 0xFF) == BUSY || (term->transm_status & 0xFF) == READY) {
                dev_status = term->transm_status;
                term->transm_command = ACK;
                sem_index = transm_sem;
            } else {
                dev_status = term->recv_status;
                term->recv_command = ACK;
                sem_index = recv_sem;
            }
        }
    } else {
        // Device normale
        devreg_t *dev = (devreg_t *)DEVREGADDR(line, dev_no);
        dev_status = ((unsigned int *)dev)[0];     // status
        ((unsigned int *)dev)[1] = ACK;             // command
    }

    MUTEX_GLOBAL(
            int* sem = device_semaphores(sem_index);
            pcb_t *unblocked = removeBlocked(sem);
            if (unblocked != NULL) {
                insertProcQ(ready_queue(), unblocked);
                softBlockCount--;
                *sem = 0;
            }
    )

    if (unblocked != NULL) {
        unblocked->p_s.reg_a0 = dev_status;
        LDST(exception_state);
    } else {
        scheduler();
    }
}


// Smista gli interrupt alla funzione giusta in base alla linea
void dispatchInterrupt(unsigned int cause, state_t *exception_state) {
    unsigned int excCode = cause & CAUSE_EXCCODE_MASK;
    switch (excCode) {
        case IL_CPUTIMER:
            handlePLTInterrupt(exception_state);
            break;
        case IL_TIMER:
            handlePseudoClockInterrupt(exception_state);
            break;
        case IL_DISK:
        case IL_FLASH:
        case IL_ETHERNET:
        case IL_PRINTER:
        case IL_TERMINAL:
            handleDeviceInterrupt(excCode, exception_state);
            break;
        default:

            break;
    }

}