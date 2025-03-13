#include <stddef.h>
#include "./headers/pcb.h"

static struct list_head pcbFree_h;
static pcb_t pcbFree_table[MAXPROC];
static int next_pid = 1;

// necessario alla compilazione (in alternativa gestire linking con ld
// e parametro -nostdlib in CMakeLists.txt)
__attribute__((unused)) void memcpy(void *dest, const void *src, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        ((char*)dest)[i] = ((char*)src)[i];
    }
}

void bp() {}

void initPcbs() {
    //inizializza la pcbFree list, contiene tutti gli elementi dell'array statico MAXPROC PCBs
    INIT_LIST_HEAD(&pcbFree_h);
    for (int i=0; i<MAXPROC; i++){
        INIT_LIST_HEAD(&pcbFree_table[i].p_list);   //inizializzo il nodo p_list (inizializza la testa della lista circolare)
        list_add_tail(&pcbFree_table[i].p_list, &pcbFree_h);
    }
}

void freePcb(pcb_t* p) {
    //inserisce l'elemento puntato da p sulla lista pcbFree
    //usata dai pcbs che non vengono piu' usati
    list_add_tail(&p->p_list, &pcbFree_h);
}

pcb_t* allocPcb() {
    if (list_empty(&pcbFree_h)) return NULL; //se la lista di PCB disponibili è vuota ritorna NULL
    else {
        //recupera il primo PCB libero della lista, prende il puntatore alla struttura pcb_t del primo elemento della lista
        pcb_t *p = getPcb((&pcbFree_h)->next);
        //rimuove il primo elemento della lista dei PCB liberi, aggiornando poi la lista
        list_del((&pcbFree_h)->next);

        //inizializzo la coda dei processi child e siblings
        INIT_LIST_HEAD(&(p->p_list)); //prima inizializzo p_list come lista vuota
        INIT_LIST_HEAD(&(p->p_child)); //poi inizializzo la lista dei figli

        //imposto il padre del PCB a NULL perchè è il primo PCB
        p->p_parent = NULL;
        //inizializzo la lista dei sibling
        INIT_LIST_HEAD(&(p->p_sib));

        //inizializzazione dei campi
        p->p_parent = NULL;
        state_t newState;
        newState.status = 0;
        p->p_s = newState; //in teoria p_s dovrebbe avere altri membri che però non sono definiti in nessun file .h quindi non so...
        p->p_time = 0;
        p->p_semAdd = NULL;
        p->p_supportStruct = NULL;
        p->p_pid = 0;
        
        return p;
    }

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
    list_add_tail(&p->p_list, head);
}

//Restituisce il primo PCB dalla coda dei processi.
pcb_t* headProcQ(struct list_head* head) {
    if (list_empty(head)) // Verifica se la lista è vuota.
        return NULL;
    else{
        return getPcb(head->next); //Restituisce il puntatore al primo elemento successivo a head
    }
}

//Rimuove il PCB in testa alla lista
pcb_t* removeProcQ(struct list_head* head) {
        if (list_empty(head)) // Verifica se la lista è vuota.
            return NULL;
        else {
            pcb_t* p= headProcQ(head); //Restituisce il puntatore al primo elemento successivo a head
            list_del(&p->p_list); //Rimuove il puntatore dalla lista
            return p; //Ritorno la lista
        }
}

//Rimuove un PCB specifico dalla coda
pcb_t* outProcQ(struct list_head* head, pcb_t* p) {
    for_each_pcb(head){   //Ciclo la lista
        if (curr == p) {   //Vedo se il PCB trovato è uguale a quello specifico
            list_del(&p->p_list); //lo rimuovo dalla lista
            return p;   //Ritorno la lista con il PCB rimosso
        }
    }
    return NULL;
}

//verifico se un Processo ha figli
int emptyChild(pcb_t* p) {
    //l'output è il risultato della verifica di list empty che prende come 
    //parametro l'indirzzo (con &) della testa della lista p_child puntata da p
    return (list_empty(&p->p_child));
    //true (1) se vuota, false (0) altrimenti
}

void insertChild(pcb_t* prnt, pcb_t* p) {
    //per prima cosa setto prnt come genitore di p
    p->p_parent = prnt;
    //aggiungo alla fine, preservando la FIFO (linkando sempre i siblings)
    list_add_tail(&(p->p_sib), &(prnt->p_child));
}

pcb_t* removeChild(pcb_t* p) {
    //se la lista dei figli è vuota non devo togliere nulla
    if (emptyChild(p)) return NULL;
    else {
        //risalgo al primo figlio utilizando i fratelli 
        pcb_t *first_child = container_of(list_next(&(p->p_child)), pcb_t, p_sib);
        list_del(&(first_child->p_sib)); //rimuovo dalla lista dei siblings
        first_child->p_parent = NULL; //elimino il puntatore dal figlio al padre
        
        return first_child; //ritorno il child eliminato
    }
}

pcb_t* outChild(pcb_t* p) {
    if (p->p_parent == NULL) return NULL;
    else {
        list_del(&(p->p_sib)); //rimuovo dalla lista dei siblings
        p->p_parent = NULL; //elimino il puntatore dal figlio al padre
        return p; //ritorno il p
    }
}
