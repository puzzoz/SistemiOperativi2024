#include "./headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

// macro che ritorna il semaforo che contiene la lista semLAdd
#define getSem(semLAdd) container_of(semLAdd, semd_t, s_link)

// inizializza un ciclo su una lista di semafori con elemento iterativo curr
#define for_each_semd(head) semd_t *curr; list_for_each_entry(curr, head, s_link)

/*
 * ritorna il semaforo con key contenuta in semAdd, NULL se non lo trova
*/
semd_t *findSem(const int *semAdd) {
    invariant(semAdd)
    for_each_semd(&semd_h) {
        if (curr->s_key == semAdd) return curr;
    }
    return NULL;
}

/*
 * ritorna il processo con id p_id contenuto nella lista dei processi bloccati
 * dal semaforo con key in semAdd, NULL altrimenti
*/
pcb_t *findPcb(const int *semAdd, int p_id) {
    invariant(semAdd)
    semd_t *sem = findSem(semAdd);
    invariant(sem)
    struct list_head *proc_h = &sem->s_procq;
    for_each_pcb(proc_h) {
        if (curr->p_pid == p_id) return curr;
    }
    return NULL;
}

/*
 * inizializza la lista dei semafori liberi semdFree_h allocando MAXPROC
 * semafori dall'array semd_table
*/
void initASL() {
    INIT_LIST_HEAD(&semdFree_h);
    INIT_LIST_HEAD(&semd_h);
    for (int i = 0; i < MAXPROC; ++i) {
        list_add_tail(&semd_table[i].s_link, &semdFree_h);
    }
}

/*
 * inserisce il processo p tra i processi bloccati dal semaforo con chiave
 * in semAdd. se il semaforo non viene trovato in semd_h e la lista dei semafori
 * liberi semdFree_h non è vuota, un semaforo viene spostato in semd_h e il
 * processo viene aggiunto. se semdFree_h è piena e il processo non viene aggiunto
 * ritorna TRUE, FALSE altrimenti
*/
int insertBlocked(int* semAdd, pcb_t* p) {
    semd_t *sem = findSem(semAdd);
    if (sem == NULL) {
        if (list_empty(&semdFree_h)) return TRUE;
        // ottiene il primo semaforo libero
        sem = getSem(semdFree_h.next); // head delle due liste di semafori mai contenute in un semaforo
        list_del(semdFree_h.next);
        INIT_LIST_HEAD(&sem->s_link);
        list_add_tail(&sem->s_link, &semd_h);
        sem->s_key = semAdd;
        mkEmptyProcQ(&sem->s_procq);
    }
    p->p_semAdd = semAdd;
    insertProcQ(&sem->s_procq, p);
    return FALSE;
}

/*
 * se il semaforo sem non ha processi bloccati viene spostato in semdFree_h
 */
void freeSemIfEmpty(semd_t *sem) {
    if (emptyProcQ(&sem->s_procq)) {
        list_del(&sem->s_link);
        list_add_tail(&sem->s_link, &semdFree_h);
    }
}

/*
 * rimuove il primo processo bloccato dal semaforo con key in semAdd.
 * se il semaforo non ha altri processi viene spostato nei semafori liberi
*/
pcb_t* removeBlocked(int* semAdd) {
    semd_t *sem = findSem(semAdd);
    invariant(sem)
    pcb_t *p = removeProcQ(&sem->s_procq);
    freeSemIfEmpty(sem);
    return p;
}

/*
 * rimuove e ritorna il processo con id pid dal semaforo in cui è
 * bloccato, NULL altrimenti
*/
pcb_t* outBlockedPid(int pid) {
    struct list_head *currSem = &semd_h; pcb_t *pcb;
    while (currSem != NULL && pcb == NULL) {
        pcb = findPcb(getSem(currSem)->s_key, pid);
        currSem = list_next(currSem);
    }
    pcb_t *removed = outProcQ(&getSem(currSem)->s_procq, pcb);
    freeSemIfEmpty(getSem(currSem));
    return removed;
}

/*
 * rimuove e ritorna il processo p dal semaforo in cui è bloccato,
 * NULL altrimenti
*/
pcb_t* outBlocked(pcb_t* p) {
    invariant(p)
    for_each_semd(&semd_h) {
        pcb_t *out = outProcQ(&curr->s_procq, p);
        if (out != NULL) {
            freeSemIfEmpty(curr);
            return out;
        }
    }
    return NULL;
}

/*
 * ritorna il primo processo bloccato dal semaforo con key in semAdd,
 * NULL altrimenti
*/
pcb_t* headBlocked(int* semAdd) {
    semd_t *sem = findSem(semAdd);
    invariant(sem)
    return headProcQ(&sem->s_procq);
}