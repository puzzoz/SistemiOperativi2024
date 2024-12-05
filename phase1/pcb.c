#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

void initPcbs() {
    //inizializza la pcbFree list, contiene tutti gli elementi dell'array statico MAXPROC PCBs
    for(int i=0; i<MAXPROC; i++){
        INIT_LIST_HEAD(&pcbFree_table[i].p_list);   //inizializzo il nodo p_list (inizializza la testa della lista circolare)
        list_add_tail(&pcbFree_table[i], &pcbFree_h);
    }
}

void freePcb(pcb_t* p) {
    //inserisce l'elemento puntato da p sulla lista pcbFree
    //usata dai pcbs che non vengono piu' usati
    list_add_tail(&p->p_list, &pcbFree_h);
}

pcb_t* allocPcb() {
}

void mkEmptyProcQ(struct list_head* head) {
    // metodo usato per inizializzare una variabile per essere un head pointer ad un processo queue
    INIT_LIST_HEAD(head);
}

//Verifica se una coda è vuota
int emptyProcQ(struct list_head* head) {
    return list_empty(head);
}

void insertProcQ(struct list_head* head, pcb_t* p) {
    //inserisce il PCB puntato da p nella coda dei processi, la quale testa e' puntata da head
    struct list_head* last=head->prev;  //trovo l'ultimo nodo della lista
    //aggiorno i puntatori
    p->p_list.next=head;    //il prossimo nodo di p e' head
    p->p_list.prev=last;    //il nodo precedene di p e' last
    last->next=&p->p_list;  //aggiorno last e head
    head->prev=&p->p_list;
}

//Restituisce il primo PCB dalla coda dei processi.
pcb_t* headProcQ(struct list_head* head) {
    if (list_empty(head)) // Verifica se la lista è vuota.
        return NULL;
    else{
        return container_of(head->next,pcb_t,p_list); //Restituisce il puntatore al primo elemento successivo a head
    }
}

//Rimuove il PCB in testa alla lista
pcb_t* removeProcQ(struct list_head* head) {
        if (list_empty(head)) // Verifica se la lista è vuota.
            return NULL;
        else {
            pcb_t* p= container_of(head->next,pcb_t,p_list); //Restituisce il puntatore al primo elemento successivo a head
            list_del(&p->p_list); //Rimuove il puntatore dalla lista
            return p; //Ritorno la lista
        }
}

//Rimuove un PCB specifico dalla coda
pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
    struct list_head* pos;
    list_for_each(pos,head){   //Ciclo la lista
        if (container_of(pos,pcb_t,p_list)==p){   //Vedo se il PCB trovato è uguale a quello specifico
            list_del(&p->p_list); //lo rimuovo dalla lista
            return p;   //Ritorno la lista con il PCB rimosso
        }
    }
    return NULL;
}

//verifico se un Processo ha figli
int emptyChild(pcb_t* p) {
    return (list_empty(&p->p_child));
}

void insertChild(pcb_t* prnt, pcb_t* p) {
}

pcb_t* removeChild(pcb_t* p) {
}

pcb_t* outChild(pcb_t* p) {
}
