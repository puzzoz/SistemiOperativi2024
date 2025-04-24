#ifndef MULTIPANDOS_INTERRUPTS_H
#define MULTIPANDOS_INTERRUPTS_H

#include "headers/interrupts.h"
#include <uriscv/const.h>
#include "../headers/types.h"
#include <uriscv/liburiscv.h>
#include "headers/initial.h"
#include "headers/scheduler.h"
#include "../phase1/headers/asl.h"

#define PSEUDO_CLOCK_SEM 48
#define DEVREGADDR(line, no) (RAMBASEADDR + 0x54 + ((line - 3) * DEVREGSIZE * DEVPERINT) + (no * DEVREGSIZE))

// Aggiorna il tempo CPU del processo attuale
void updateCPUtime(pcb_t* p) {
    cpu_t now;
    STCK(now);
    p->p_time += now - sliceStart;
}

// Copia lo stato processore dal contesto salvato al PCB
void saveState(state_t *dest, state_t *src) {
    *dest = *src;
}

// Interrupt del clock di sistema (ogni 100ms): sblocca i processi in wait clock
static void handlePseudoClockInterrupt(state_t *exception_state) {
    LDIT(PSECOND * (*((cpu_t *)TIMESCALEADDR)));  // resetta il clock di sistema
    MUTEX_GLOBAL(
        pcb_t *unblocked;
        int *pseudoClockSem = device_semaphores(PSEUDO_CLOCK_SEM);
        // Sblocca tutti i processi che aspettavano il clock
        while ((unblocked = removeBlocked(pseudoClockSem)) != NULL) {
            insertProcQ(ready_queue(), unblocked);
            softBlockCount--;
        }
        *pseudoClockSem = 0;
        unblocked = *current_process();
    )

    // Se c'è un processo in esecuzione, lo riprende
    if (unblocked != NULL)
        LDST(&unblocked->p_s);
    else
        scheduler();
}

// Interrupt del PLT: fine time slice per il processo corrente
static void handlePLTInterrupt(state_t *exception_state) {
    setTIMER(-1); // ACK del PLT
    updateCPUtime(*current_process());
    saveState(&((*current_process())->p_s), exception_state);

    ACQUIRE_LOCK(global_lock());
    insertProcQ(ready_queue(), *current_process()); // rimettilo in ready
    RELEASE_LOCK(global_lock());

    scheduler();
}

// Calcola l'indice del semaforo dato numero linea e device
static int getDeviceIndex(int line, int dev_no) {
    return ((line - 3) * 8) + dev_no;
}

// Interrupt da device: trova chi ha fatto interrupt, fa ACK e sblocca il processo
void handleDeviceInterrupt(int line, unsigned int cause, state_t *exception_state) {
    ACQUIRE_LOCK(global_lock());
    devregarea_t *dev_area = (devregarea_t *)RAMBASEADDR;
    unsigned int bitmap = dev_area->interrupt_dev[line - 3];
    int dev_no = -1;
    int sem_index = getDeviceIndex(line, dev_no);

    // Trova il device specifico che ha causato l'interrupt
    for (int i = 0; i < 8; i++) {
        if (bitmap & (1 << i)) {
            dev_no = i;
            break;
        }
    }

    if (dev_no == -1) {
        RELEASE_LOCK(global_lock());
        return; // nessun device attivo trovato
    }


    unsigned int dev_status = 0;

    // Gestione speciale per i terminali (2 sub-device)
    if (line == IL_TERMINAL) {
        termreg_t *term = (termreg_t *)DEVREGADDR(line, dev_no);
        if ((term->transm_status & 0xFF) == BUSY || (term->transm_status & 0xFF) == READY) {
            dev_status = term->transm_status;
            term->transm_command = ACK;
        } else {
            dev_status = term->recv_status;
            term->recv_command = ACK;
            sem_index += 1;
        }
    } else {
        // Per gli altri device (disk, flash, ecc.)
        dtpreg_t *dev = (dtpreg_t *)DEVREGADDR(line, dev_no);
        dev_status = dev->status;
        dev->command = ACK;
    }

    // Sblocca il processo che stava aspettando questo device

    pcb_t *unblocked = removeBlocked(device_semaphores(sem_index));
    if (unblocked != NULL) {
        unblocked->p_s.gpr[10] = dev_status; // ritorna status del device in v0
        insertProcQ(ready_queue(), unblocked);
        softBlockCount--;
    }

    RELEASE_LOCK(global_lock());

    // Riprende il processo se esiste, altrimenti schedula
    if (current_process())
        LDST(exception_state);
    else
        scheduler();
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
        case IL_DISK:     case IL_FLASH:
        case IL_ETHERNET: case IL_PRINTER:
        case IL_TERMINAL:
            handleDeviceInterrupt(excCode, cause, exception_state);
            break;
        default:
            break;
    }
}

#endif // MULTIPANDOS_INTERRUPTS_H
