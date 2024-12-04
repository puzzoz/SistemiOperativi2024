#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

void initPcbs() {
}

void freePcb(pcb_t* p) {
}

pcb_t* allocPcb() {
}

void mkEmptyProcQ(struct list_head* head) {
}

int emptyProcQ(struct list_head* head) {
}

void insertProcQ(struct list_head* head, pcb_t* p) {
}

pcb_t* headProcQ(struct list_head* head) {
    if (list_empty(head))
        return NULL;
    else{
        return container_of(head->next,pcb_t,p_list);
    }
}

pcb_t* removeProcQ(struct list_head* head) {
        if (list_empty(head))
            return NULL;
        else {
            pcb_t* p= container_of(head->next,pcb_t,p_list);
            list_del(p->p_list);
            return p;
        }
}

pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
    struct list_head* pos;
    list_for_each(pos,head){
        if (container_of(pos,pcb_t,p_list)==p){
            list_del(p->p_list);
            return p;
        }
    }
    return NULL;
}

int emptyChild(pcb_t* p) {
    return (list_empty(p->p_child)))
}

void insertChild(pcb_t* prnt, pcb_t* p) {
}

pcb_t* removeChild(pcb_t* p) {
}

pcb_t* outChild(pcb_t* p) {
}
