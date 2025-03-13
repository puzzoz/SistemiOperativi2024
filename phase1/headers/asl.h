#ifndef ASL_H_INCLUDED
#define ASL_H_INCLUDED

#include "../../headers/listx.h"
#include "../../headers/types.h"

// macro che ritorna il semaforo che contiene la lista semLAdd
#define getSem(semLAdd) container_of(semLAdd, semd_t, s_link)

// inizializza un ciclo su una lista di semafori con elemento iterativo curr
#define for_each_semd(head) semd_t *curr; list_for_each_entry(curr, head, s_link)

void initASL();
int insertBlocked(int* semAdd, pcb_t* p);
pcb_t* removeBlocked(int* semAdd);
pcb_t* outBlockedPid(int pid);
pcb_t* outBlocked(pcb_t* p);
pcb_t* headBlocked(int* semAdd);

#endif
