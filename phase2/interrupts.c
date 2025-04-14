#ifndef MULTIPANDOS_INTERRUPTS_H
#define MULTIPANDOS_INTERRUPTS_H

#include "headers/interrupts.h"
#include "../../headers/const.h"
#include "../../headers/listx.h"
#include "../../headers/types.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include "headers/initial.h"
#include "../phase1/headers/pcb.h"


// Rimuove e restituisce il primo processo bloccato su un dato device da una lista
static pcb_t *extractBlockedByDevNo(int device_number, struct list_head *list) {
    for_each_pcb(list) {
        if (curr->dev_no == device_number)
            return outProcQ(list, curr);
    }
    return NULL;
}

// Gestisce l'interrupt del timer di sistema (quello che scatta ogni 100ms)
// Serve per sbloccare i processi che stavano aspettando un tick (tipo wait clock)
static void handlePseudoClockInterrupt(state_t *exception_state) {
    setIntervalTimer(PSECOND);  // reset del timer

    pcb_t *unblocked;

    MUTEX_GLOBAL(
        while ((unblocked = removeProcQ(&Locked_pseudo_clock)) != NULL) {
            send(ssi_pcb, unblocked, 0);
            insertProcQ(&Ready_Queue, unblocked);
            soft_blocked_count--;
    })

    if (*current_process())
        LDST(exception_state);  // riprende il processo corrente
    else
        scheduler();            // altrimenti schedula un altro
}

// Gestisce l'interrupt del timer locale (PLT), cioè il time slice del processo
// Se arriva qui vuol dire che il processo ha finito il suo tempo
static void handlePLTInterrupt(state_t *exception_state) {
    setPLT(-1);  // ACK
    updateCPUtime(current_process);  // aggiorna tempo usato
    saveState(&(current_process->p_s), exception_state);  // salva stato nel PCB

    MUTEX_GLOBAL(
        insertProcQ(&Ready_Queue, current_process);  // lo rimette in ready
    );

    scheduler();  // schedula un nuovo processo
}

// Questa è la funzione principale che smista gli interrupt a seconda della linea
void dispatchInterrupt(int cause, state_t *exception_state) {
    if (CAUSE_IP_GET(cause, IL_CPUTIMER))
        handlePLTInterrupt(exception_state);
    else if (CAUSE_IP_GET(cause, IL_TIMER))
        handlePseudoClockInterrupt(exception_state);
    else if (CAUSE_IP_GET(cause, IL_DISK))
        handleDeviceInterrupt(IL_DISK, cause, exception_state);
    else if (CAUSE_IP_GET(cause, IL_FLASH))
        handleDeviceInterrupt(IL_FLASH, cause, exception_state);
    else if (CAUSE_IP_GET(cause, IL_ETHERNET))
        handleDeviceInterrupt(IL_ETHERNET, cause, exception_state);
    else if (CAUSE_IP_GET(cause, IL_PRINTER))
        handleDeviceInterrupt(IL_PRINTER, cause, exception_state);
    else if (CAUSE_IP_GET(cause, IL_TERMINAL))
        handleDeviceInterrupt(IL_TERMINAL, cause, exception_state);
}

// Gestisce un interrupt proveniente da un device (tipo disco, ethernet, terminale ecc.)
// Capisce quale device ha generato l’interrupt, fa l’ACK e sblocca il processo in attesa
static void handleDeviceInterrupt(int line, int cause, state_t *exception_state) {
    ACQUIRE_LOCK(&Global_Lock);

    devregarea_t *dev_area = (devregarea_t *)BUS_REG_RAM_BASE;
    unsigned int bitmap = dev_area->interrupt_dev[line - 3];
    int dev_no = -1;
    unsigned int dev_status = 0;
    pcb_t *unblocked = NULL;

    // cerca il primo device che ha effettivamente l’interrupt attivo
    for (int i = 0; i < 8; i++) {
        if (bitmap & (1 << i)) {
            dev_no = i;
            break;
        }
    }

    if (dev_no == -1) {
        RELEASE_LOCK(&Global_Lock);
        return;
    }

    // gestione separata per i terminali (hanno due sub-device: transmit e receive)
    if (line == IL_TERMINAL) {
        termreg_t *term = (termreg_t *)DEV_REG_ADDR(line, dev_no);
        if ((term->transm_status & 0xFF) == 5) {
            dev_status = term->transm_status;
            term->transm_command = ACK;
            unblocked = extractBlockedByDevNo(dev_no, &Locked_terminal_transm);
        } else {
            dev_status = term->recv_status;
            term->recv_command = ACK;
            unblocked = extractBlockedByDevNo(dev_no, &Locked_terminal_recv);
        }
    } else {
        // tutti gli altri device (disco, flash, ethernet, stampante)
        dtpreg_t *dev = (dtpreg_t *)DEV_REG_ADDR(line, dev_no);
        dev_status = dev->status;
        dev->command = ACK;

        switch (line) {
            case IL_DISK:
                unblocked = extractBlockedByDevNo(dev_no, &Locked_disk); break;
            case IL_FLASH:
                unblocked = extractBlockedByDevNo(dev_no, &Locked_flash); break;
            case IL_ETHERNET:
                unblocked = extractBlockedByDevNo(dev_no, &Locked_ethernet); break;
            case IL_PRINTER:
                unblocked = extractBlockedByDevNo(dev_no, &Locked_printer); break;
        }
    }

    // se c'era effettivamente un processo bloccato, lo sblocca e lo rimette in coda
    if (unblocked) {
        unblocked->p_s.reg_v0 = dev_status;
        send(ssi_pcb, unblocked, (memaddr)dev_status);
        insertProcQ(&Ready_Queue, unblocked);
        soft_blocked_count--;
    }

    RELEASE_LOCK(&Global_Lock);

    if (*current_process())
        LDST(exception_state);
    else
        scheduler();
}

#endif // MULTIPANDOS_INTERRUPTS_H
