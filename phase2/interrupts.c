#ifndef MULTIPANDOS_INTERRUPTS_H
#define MULTIPANDOS_INTERRUPTS_H

#include "headers/interrupts.h"
#include <uriscv/const.h>
#include "headers/types.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include "headers/initial.h"
#include "headers/pcb.h"
#include "headers/scheduler.h"
#include "headers/asl.h"

#define PSEUDO_CLOCK_SEM 48

void updateCPUtime(pcb_t* p) {
    cpu_t now;
    STCK(now);
    p->p_time += now - sliceStart;
}

void saveState(state_t *dest, state_t *src) {
    *dest = *src;
}

// Gestisce l'interrupt del timer di sistema (ogni 100ms)
static void handlePseudoClockInterrupt(state_t *exception_state) {
    setTIMER(PSECOND);  // reset del timer
    pcb_t *unblocked;
    while ((unblocked = removeBlocked(&deviceSemaphores[0][1])) != NULL) {
        unblocked->p_s.reg_v0 = 0;
        insertProcQ(ready_queue(), unblocked);
        softBlockCount--;
    }

    if (current_process(0))
        LDST(exception_state);
    else
        scheduler();
}

// Gestisce l'interrupt del PLT (fine time slice)
static void handlePLTInterrupt(state_t *exception_state) {
    setTIMER(-1);
    updateCPUtime(current_process(0));
    saveState(&(current_process(0)->p_s), exception_state);

    MUTEX_GLOBAL(
        insertProcQ(ready_queue(), current_process());  // lo rimette in ready
    );
    scheduler();
}

// Calcola l'indice del semaforo per un dato device
static int getDeviceIndex(int line, int dev_no) {
    return ((line - 3) * 8) + dev_no;
}

// Gestisce un interrupt da device
void handleDeviceInterrupt(int line, int cause, state_t *exception_state) {
    ACQUIRE_LOCK(global_lock());
    devregarea_t *dev_area = (devregarea_t *)BUS_REG_RAM_BASE;
    unsigned int bitmap = dev_area->interrupt_dev[line - 3];
    int dev_no = -1;

    for (int i = 0; i < 8; i++) {
        if (bitmap & (1 << i)) {
            dev_no = i;
            break;
        }
    }

    if (dev_no == -1) {
        RELEASE_LOCK(global_lock());
        return;
    }

    int dev_index = getDeviceIndex(line, dev_no);
    unsigned int dev_status = 0;

    if (line == IL_TERMINAL) {
        termreg_t *term = (termreg_t *)DEV_REG_ADDR(line, dev_no);
        if ((term->transm_status & 0xFF) == DEV_BUSY || (term->transm_status & 0xFF) == DEV_READY) {
            dev_status = term->transm_status;
            term->transm_command = ACK;
        } else {
            dev_status = term->recv_status;
            term->recv_command = ACK;
        }
    } else {
        dtpreg_t *dev = (dtpreg_t *)DEV_REG_ADDR(line, dev_no);
        dev_status = dev->status;
        dev->command = ACK;
    }

    pcb_t *unblocked = removeBlocked(&deviceSemaphores[0][0]);
    if (unblocked != NULL) {
        unblocked->p_s.reg_v0 = dev_status;
        insertProcQ(ready_queue(), unblocked);
        softBlockCount--;
    }

    if (current_process(0))
        LDST(exception_state);
    else
        scheduler();
}

// Smista gli interrupt alla funzione corretta
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

#endif // MULTIPANDOS_INTERRUPTS_H
