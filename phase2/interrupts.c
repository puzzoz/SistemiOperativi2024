#include "headers/interrupts.h"
#include <uriscv/const.h>
#include "../headers/types.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>
#include "headers/initial.h"
#include "headers/scheduler.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/asl.h"

// Macro per calcolare l'indirizzo del registro di un device, data la sua linea e numero.
#define DEVREGADDR(line, no) (RAMBASEADDR + 0x54 + ((line - 3) * DEVREGSIZE * DEVPERINT) + (no * DEVREGSIZE))


// Interrupt del clock di sistema (ogni 100ms): sblocca i processi in wait clock
static void handlePseudoClockInterrupt(state_t *exception_state) {
    LDIT(PSECOND);// Ricarica il timer del clock per la prossima interrupt
    // Uso una MUTEX_GLOBAL per evitare race condition mentre si manipola la ready queue
    MUTEX_GLOBAL(
        int *pseudoClockSem = device_semaphores(PSEUDO_CLOCK_SEM);// Semaforo associato al clock
        pcb_t *unblocked;
            // Rimuove e sblocca tutti i processi in attesa del clock
        while ((unblocked = removeBlocked(pseudoClockSem)) != NULL) {
            insertProcQ(&readyQueue, unblocked); // Li rimette nella ready queue
            softBlockCount--; // Decrementa il conteggio dei processi bloccati su device
        }
        unblocked = *current_process(); // Verifica se c'è un processo attivo
    )
    // Se c'era un processo in esecuzione, lo riprende. Altrimenti invoca lo scheduler
    if (unblocked != NULL) {
        LDST(exception_state);// Ritorna all'esecuzione del processo corrente
    } else {
        scheduler();//Nessun processo attivo-> invoca lo scheduler
    }
}


// Interrupt del PLT: fine time slice per il processo corrente
static void handlePLTInterrupt(state_t *exception_state) {
    setTIMER(-1); // ACK del PLT per segnalare che l'interrupt è stato gestito
    MUTEX_GLOBAL(
            updateProcessCPUTime();// Aggiorna il tempo CPU del processo interrotto
            (*current_process())->p_s = *exception_state;// Copia lo stato processore dal contesto salvato al PCB
            insertProcQ(&readyQueue, *current_process());// Rimette il processo in coda ready
            )
    scheduler();// Chiamata allo scheduler per selezionare un nuovo processo
}

// Calcola l'indice del semaforo dato numero linea e device
static int getDeviceIndex(int line, int dev_no) {
    return ((line - 3) * 8) + dev_no;
    // Ogni linea ha 8 device: linee 3-7 -> 5 linee -> 40 device -> index da 0 a 39
}

// Interrupt da device: trova chi ha fatto interrupt, fa ACK e sblocca il processo
void handleDeviceInterrupt(int excCode, state_t *exception_state) {
    int line;
    // Mappa codice d'interrupt a linea di interrupt
    switch (excCode) {
        case IL_DISK:     line = 3; break;
        case IL_FLASH:    line = 4; break;
        case IL_ETHERNET: line = 5; break;
        case IL_PRINTER:  line = 6; break;
        case IL_TERMINAL: line = 7; break;
        default: return; // Caso di errore: interrupt non gestito
    }

    // Legge il bitmap della linea per capire quale device ha generato l’interrupt
    unsigned int bitmap = *((unsigned int *)(0x10000040 + (line - 3) * 4));

    // Determina il numero del device che ha fatto l'interrupt
    int dev_no = -1;
    for (int i = 0; i < 8; i++) {
        if (bitmap & (1u << i)) {
            dev_no = i;
            break;
        }
    }
    if (dev_no < 0) {
        return;  // Nessun interrupt pending trovato
    }

    int sem_index = getDeviceIndex(line, dev_no); // Calcola l'indice del semaforo
    unsigned int dev_status = 0;

    //Caso speciale: TERMINALI
    if (excCode == IL_TERMINAL) {
        termreg_t *term = (termreg_t *)DEVREGADDR(line, dev_no);
        int transm_sem = sem_index;
        int recv_sem = sem_index + 1; // Terminali hanno due semafori receive e transmit
        int hasTransmBlocked, hasRecvBlocked;

        // Verifica se ci sono processi bloccati in attesa della trasmissione o ricezione
        MUTEX_GLOBAL(
                hasTransmBlocked = (headBlocked(device_semaphores(transm_sem)) != NULL);
                hasRecvBlocked = (headBlocked(device_semaphores(recv_sem)) != NULL);
        )

        // Priorità alla trasmissione se entrambi bloccati
        if (hasTransmBlocked) {
            dev_status = term->transm_status;
            term->transm_command = ACK;
            sem_index = transm_sem;
        } else if (hasRecvBlocked) {
            dev_status = term->recv_status;
            term->recv_command = ACK;
            sem_index = recv_sem;
        } else {
            // Se nessuno è bloccato, si valuta lo stato corrente del terminale
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
        // Caso generico: altri device
        devreg_t *dev = (devreg_t *)DEVREGADDR(line, dev_no);
        dev_status = ((unsigned int *)dev)[0];    // scrive lo stato del device
        ((unsigned int *)dev)[1] = ACK;             // scrive l'ACK nel command
    }

    // Sblocco processo bloccato sul semaforo del device
    MUTEX_GLOBAL(
            int* sem = device_semaphores(sem_index);
            pcb_t *unblocked = removeBlocked(sem); // Sblocca un eventuale processo
            if (unblocked != NULL) {
                insertProcQ(&readyQueue, unblocked); // Lo rimette nella ready queue
                softBlockCount--;// Aggiorna il numero di processi in attesa di device
                *sem = 0;// Reset del semaforo
            }
    )

    // Ripristino o scheduling
    if (unblocked != NULL) {
        unblocked->p_s.reg_a0 = dev_status;// Restituisce lo stato come valore di ritorno
        LDST(exception_state); // Riprende il processo interrotto
    } else {
        scheduler();//Nessun processo attivo-> invoca lo scheduler
    }
}


// Dispatch dell’interrupt: inoltra alla routine corretta
void dispatchInterrupt(unsigned int cause, state_t *exception_state) {
    unsigned int excCode = cause & CAUSE_EXCCODE_MASK; // Estrae codice da `cause`

    // Smista in base al tipo di interrupt
    switch (excCode) {
        case IL_CPUTIMER:
            handlePLTInterrupt(exception_state);// Timer locale
            break;
        case IL_TIMER:
            handlePseudoClockInterrupt(exception_state); // Timer globale (clock)
            break;
        case IL_DISK:
        case IL_FLASH:
        case IL_ETHERNET:
        case IL_PRINTER:
        case IL_TERMINAL:
            handleDeviceInterrupt(excCode, exception_state);// Interrupt da device
            break;
        default:
            PANIC(); // Caso di errore: codice d'interrupt non gestito
            break;
    }

}