#ifndef MULTIPANDOS_INITIAL_H
#define MULTIPANDOS_INITIAL_H

#include <uriscv/types.h>
#include "../../headers/types.h"
#include "../../phase1/headers/pcb.h"
#include "../../headers/const.h"

#define DEVICE_SEMS 48
#define PSEUDO_CLOCK_SEM  DEVICE_SEMS

#define MUTEX_GLOBAL(expr) ACQUIRE_LOCK(&globalLock); expr; RELEASE_LOCK(&globalLock);
#define DEV_NO_BY_DEV_ADDR(devAddr) (((unsigned int)devAddr - START_DEVREG) / sizeof(devreg_t))

extern unsigned int globalLock;
extern unsigned int softBlockCount;
extern int processCount;

pcb_t** current_process();

int *device_semaphores(unsigned int devNo); // 2 semafori per ogni subdevice

//queue dei PCB in READY state
extern list_head readyQueue;

#endif
