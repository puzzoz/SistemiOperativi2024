#include "./headers/pcb.h"

static semd_t semd_table[MAXPROC];
static struct list_head semdFree_h;
static struct list_head semd_h;

#define getSem(semLAdd) container_of(semLAdd, semd_t, s_link)
#define getPcb(pcbLAdd) container_of(pcbLAdd, pcb_t, p_list)

semd_t *findSem(const int *semAdd) {
    if (semAdd == NULL) return NULL;
    struct list_head *curr = &semd_h;
    while (curr != NULL) {
        if (getSem(curr)->s_key == semAdd) break;
        curr = list_next(curr);
    }
    return getSem(curr);
}

pcb_t *findPcb(const int *semAdd, int p_id) {
    if (semAdd == NULL) return NULL;
    semd_t *sem = findSem(semAdd);
    if (sem == NULL) return NULL;
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

pcb_t* delPcb(pcb_t *p) {
    if (p != NULL) list_del(&(p->p_list));
    return p;
}

void initASL() {
    for (int i = 0; i < MAXPROC; ++i) {
        list_add_tail(&semd_table[i].s_link, &semdFree_h);
    }
}

int insertBlocked(int* semAdd, pcb_t* p) {
    semd_t *sem = findSem(semAdd);
    if (sem == NULL) {
        if (list_empty(&semdFree_h)) return TRUE;
        sem = getSem(&semdFree_h);
        list_del(&semdFree_h);
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

pcb_t* removeBlocked(int* semAdd) {
    semd_t *sem = findSem(semAdd);
    if (sem == NULL) return NULL;
    pcb_t *p = delPcb(getPcb(&sem->s_procq));
    if (emptyProcQ(&sem->s_procq)) {
        list_del(&sem->s_link);
        list_add_tail(&sem->s_link, &semdFree_h);
    }
    return p;
}

pcb_t* outBlockedPid(int pid) {
    struct list_head *currSem = &semd_h; pcb_t *pcb;
    while (currSem != NULL && pcb == NULL) {
        pcb = findPcb(getSem(currSem)->s_key, pid);
        currSem = list_next(currSem);
    }
    return delPcb(pcb);
}

pcb_t* outBlocked(pcb_t* p) {
    if (p == NULL) return NULL;
    return delPcb(findPcb(p->p_semAdd, p->p_pid));
}

pcb_t* headBlocked(int* semAdd) {
    semd_t *sem = findSem(semAdd);
    if (sem == NULL) return NULL;
    return getPcb(sem->s_procq.next);
}