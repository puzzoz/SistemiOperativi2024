#ifndef MULTIPANDOS_INITIAL_H
#define MULTIPANDOS_INITIAL_H

#include <uriscv/types.h>
#include "../../headers/types.h"
#include "../../phase1/headers/pcb.h"
#include "../../headers/const.h"

#define DEVICE_SEMS 48
#define PSEUDO_CLOCK_SEM  DEVICE_SEMS

#define MUTEX_GLOBAL(expr) ACQUIRE_LOCK(global_lock()); expr; RELEASE_LOCK(global_lock());
#define DEV_NO_BY_DEV_ADDR(devAddr) (((unsigned int)devAddr - START_DEVREG) / sizeof(devreg_t))

unsigned int* global_lock();
extern unsigned int softBlockCount;

int* process_count(); //num di processi iniziati ma non ancora terminati

pcb_t** current_process();

int *device_semaphores(unsigned int devNo); // 2 semafori per ogni subdevice

//queue dei PCB in READY state
struct list_head* ready_queue();


//funzione Placeholder per uTLB_RefillHandler
extern void uTLB_RefillHandler(void);

//livello 3 exception handler nucleo
extern void exceptionHandler(void);

#endif
