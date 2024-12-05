#include "./headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

// macro che fa ritornare NULL alla funzione in cui viene usata se var è NULL
#define invariant(var) if (var == NULL) return NULL;
// macro che ritorna il semaforo che contiene la lista semLAdd
#define getSem(semLAdd) container_of(semLAdd, semd_t, s_link)
// macro che ritorna il processo che contiene la lista pcbLAdd
#define getPcb(pcbLAdd) container_of(pcbLAdd, pcb_t, p_list)

/*
    ritorna il semaforo con key contenuta in semAdd, NULL se non lo trova
*/
semd_t *findSem(const int *semAdd) {
    invariant(semAdd)
    struct list_head *curr = &semd_h;
    while (curr != NULL) {
        if (getSem(curr)->s_key == semAdd) break;
        curr = list_next(curr);
    }
    return getSem(curr);
}

/*
    ritorna il processo con id p_id contenuto nella lista dei processi bloccati
    dal semaforo con key in semAdd, NULL altrimenti
*/
pcb_t *findPcb(const int *semAdd, int p_id) {
    invariant(semAdd)
    semd_t *sem = findSem(semAdd);
    invariant(sem)
    struct list_head *proc_h = &sem->s_procq;
    struct list_head *curr = proc_h;
    do {
        pcb_t *p = getPcb(curr);
        if (p != NULL) {
            if (p->p_pid == p_id) return p;
        }
        curr = list_next(curr);
    } while (!list_is_last(curr, proc_h));
    return NULL;
}

/*
    rimuove il processo p dalla lista in cui è contenuto
*/
pcb_t* delPcb(pcb_t *p) {
    if (p != NULL) list_del(&(p->p_list));
    return p;
}

/*
    inizializza la lista dei semafori liberi semdFree_h allocando MAXPROC
    semafori dall'array semd_table
*/
void initASL() {
    for (int i = 0; i < MAXPROC; ++i) {
        list_add_tail(&semd_table[i].s_link, &semdFree_h);
    }
}
/*
    inserisce il processo p tra i processi bloccati dal semaforo con chiave
    in semAdd. se il semaforo non viene trovato in semd_h e la lista dei semafori
    liberi semdFree_h non è vuota, un semaforo viene spostato in semd_h e il
    processo viene aggiunto. se semdFree_h è piena e il processo non viene aggiunto
    ritorna TRUE, FALSE altrimenti
*/
int insertBlocked(int* semAdd, pcb_t* p) {
    semd_t *sem = findSem(semAdd);
    if (sem == NULL) {
        if (list_empty(&semdFree_h)) return TRUE;
        // ottiene il primo semaforo libero
        sem = getSem(semdFree_h.next); // head delle due liste di semafori mai contenute in un semaforo
        list_del(semdFree_h.next);
        list_add_tail(&sem->s_link, &semd_h);
        sem->s_key = semAdd;
        LIST_HEAD(procq);
        sem->s_procq = procq;
        mkEmptyProcQ(&procq);
    }
    list_add_tail(&p->p_list, &sem->s_procq);
    p->p_semAdd = semAdd;
    return FALSE;
}

/*
    rimuove il primo processo bloccato dal semaforo con key in semAdd.
    se il semaforo non ha altri processi viene spostato nei semafori liberi
*/
pcb_t* removeBlocked(int* semAdd) {
    semd_t *sem = findSem(semAdd);
    invariant(sem)
    pcb_t *p = delPcb(getPcb(&sem->s_procq));
    if (emptyProcQ(&sem->s_procq)) {
        list_del(&sem->s_link);
        list_add_tail(&sem->s_link, &semdFree_h);
    }
    return p;
}

/*
    rimuove e ritorna il processo con id pid dal semaforo in cui è
    bloccato, NULL altrimenti
*/
pcb_t* outBlockedPid(int pid) {
    struct list_head *currSem = &semd_h; pcb_t *pcb;
    while (currSem != NULL && pcb == NULL) {
        pcb = findPcb(getSem(currSem)->s_key, pid);
        currSem = list_next(currSem);
    }
    return delPcb(pcb);
}

/*
    rimuove e ritorna il processo p dal semaforo in cui è bloccato,
    NULL altrimenti
*/
pcb_t* outBlocked(pcb_t* p) {
    invariant(p)
    return delPcb(findPcb(p->p_semAdd, p->p_pid));
}

/*
    ritorna il primo processo bloccato dal semaforo con key in semAdd,
    NULL altrimenti
*/
pcb_t* headBlocked(int* semAdd) {
    semd_t *sem = findSem(semAdd);
    invariant(sem)
    return getPcb(&sem->s_procq);
}