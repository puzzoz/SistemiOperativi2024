#ifndef MULTIPANDOS_INITIAL_H
#define MULTIPANDOS_INITIAL_H

#include "../../headers/types.h"
#include "../../phase1/headers/pcb.h"
#include "../../headers/const.h"

#define MUTEX_GLOBAL(expr) ACQUIRE_LOCK(global_lock()); expr; RELEASE_LOCK(global_lock());

unsigned int* global_lock();

int* process_count(); //num di processi iniziati ma non ancora terminati

pcb_t* current_process(int CPUn);

int *device_semaphores(int CPUn); // 2 semafori per ogni subdevice


//queue dei PCB in READY state
struct list_head* ready_queue();

//PER INTERRUPT
extern struct list_head Locked_disk;
extern struct list_head Locked_flash;
extern struct list_head Locked_ethernet;
extern struct list_head Locked_printer;
extern struct list_head Locked_terminal_transm;
extern struct list_head Locked_terminal_recv;
extern struct list_head Locked_pseudo_clock;
//PER INTERRUPT

//funzione Placeholder per uTLB_RefillHandler
extern void uTLB_RefillHandler(void);

//livello 3 exception handler nucleo
extern void exceptionHandler(void);

#endif
