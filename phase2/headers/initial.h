#ifndef MULTIPANDOS_INITIAL_H
#define MULTIPANDOS_INITIAL_H

#include <uriscv/types.h>
#include "../../headers/types.h"
#include "../../phase1/headers/pcb.h"
#include "../../headers/const.h"

#define DEVICE_SEMS 48
#define PSEUDO_CLOCK_SEM  DEVICE_SEMS

/**
 * Macro per eseguire un blocco di codice in un contesto di mutua
 * esclusione sul global lock.
 * è fondamentale assicurarsi che il blocco passato venga eseguito
 * completamente in ogni caso evitando espressioni come return, break
 * e continue, o il global lock non verrà rilasciato.
 *
 * @param expr il blocco da eseguire in un contesto di mutua esclusione
 */
#define MUTEX_GLOBAL(expr) ACQUIRE_LOCK(&globalLock); expr; RELEASE_LOCK(&globalLock);
/**
 * Macro per ottenere il numero di un device tramite il suo command
 * address
 *
 * @param devAddr il command address del dispositivo di cui si vuole ottenere il
 * device number
 */
#define DEV_NO_BY_DEV_ADDR(devAddr) (((unsigned int)devAddr - START_DEVREG) / sizeof(devreg_t))
/**
 * Macro per ottenere un puntatore al processo in stato running
 * della CPU attualmente in esecuzione
 */
#define CURRENT_PROCESS currentProcess[getPRID()]

extern unsigned int globalLock;
extern unsigned int softBlockCount;
extern int processCount;
extern pcb_t *currentProcess[];

//queue dei PCB in READY state
extern list_head readyQueue;

int *device_semaphores(unsigned int devNo);

#endif
