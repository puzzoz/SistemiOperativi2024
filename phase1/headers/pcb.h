#ifndef PCB_H_INCLUDED
#define PCB_H_INCLUDED

#include "../../headers/listx.h"
#include "../../headers/types.h"

// macro che fa ritornare NULL alla funzione in cui viene usata se var è NULL
#define invariant(var) if (var == NULL) return NULL;
// macro che ritorna il processo che contiene la lista pcbLAdd
#define getPcb(pcbLAdd) container_of(pcbLAdd, pcb_t, p_list)

// inizializza un ciclo su una lista di processi con elemento iterativo curr
#define for_each_pcb(head) pcb_t *curr; list_for_each_entry(curr, head, p_list)


/*
 * utilities per i breakpoint
 */
void *bp(void *arg);// una chiamata a questa funzione definisce un breakpoint (da configurare su uriscv)
#define BP bp(NULL); // ridefinizione più concisa di un breakpoint

void initPcbs();
void freePcb(pcb_t* p);
pcb_t* allocPcb();
void mkEmptyProcQ(struct list_head* head);
int emptyProcQ(struct list_head* head);
void insertProcQ(struct list_head* head, pcb_t* p);
pcb_t* headProcQ(struct list_head* head);
pcb_t* removeProcQ(struct list_head* head);
pcb_t* outProcQ(struct list_head* head, pcb_t* p);
int emptyChild(pcb_t* p);
void insertChild(pcb_t* prnt, pcb_t* p);
pcb_t* removeChild(pcb_t* p);
pcb_t* outChild(pcb_t* p);

#endif
