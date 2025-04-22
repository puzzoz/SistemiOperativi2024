#ifndef MULTIPANDOS_INITIAL_H
#define MULTIPANDOS_INITIAL_H

#include "./headers/initial.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "./headers/scheduler.h"
#define DEVICE_SEMS 48
#define PSEUDO_CLOCK_SEM  DEVICE_SEMS
#endif //MULTIPANDOS_INITIAL_H

//LEVEL 3 GLOBAL VARIABLES

static int processCount = 0; //num di processi iniziati ma non ancora terminati
int clock_sem = 0;

typedef unsigned int size_t;
__attribute__((unused)) void memset(void *dest, int value, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        ((unsigned char*)dest)[i] = (unsigned char)value;
    }
}

unsigned int softBlockCount;

//queue dei PCB in READY state
list_head readyQueue;


//vettore di pointer ai pcb con state "running" in ogni CPU currentProcess
pcb_t *currentProcess[NCPU];

//int deviceSemaphores[NCPU][2]; // 2 semafori per ogni subdevice
int deviceSemaphores[DEVICE_SEMS+1];

unsigned int globalLock; //puo' avere solo valore 0 e 1


//funzione Placeholder per uTLB_RefillHandler
extern void uTLB_RefillHandler(void);


void initializePassUpVector() {
    for (int cpu_id = 0; cpu_id < NCPU; cpu_id++){
        passupvector_t *passupvector = (passupvector_t *)(0x0FFFF900 + (cpu_id * sizeof(passupvector_t)));
        
        passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;

        // Stack Pointer per TLB-Refill event handler
        if (cpu_id == 0) {
            passupvector->tlb_refill_stackPtr = KERNELSTACK;
        } else {
            passupvector->tlb_refill_stackPtr = RAMSTART + ((cpu_id + 64) * PAGESIZE);
        }

        
        passupvector->exception_handler = (memaddr)exceptionHandler;

        // settiamo stack pointer
        if (cpu_id == 0) {
            passupvector->exception_stackPtr = KERNELSTACK;
        } else {
            passupvector->exception_stackPtr = 0x20020000 + (cpu_id * PAGESIZE);
        }
    }
}

void initializeVariables(){
    processCount = 0;
    softBlockCount = 0;

    INIT_LIST_HEAD(&readyQueue);

    for (int i = 0; i < NCPU; i++) {
        currentProcess[i] = NULL;
    }

    memset(deviceSemaphores, 0, sizeof(deviceSemaphores));

    globalLock = 0;
}
extern void test();

void instantiateProcess() {
    pcb_t *newProcess = allocPcb();

    if (newProcess != NULL) {
        // inizializzo stato processore
        newProcess->p_s.status = MSTATUS_MIE_MASK | MSTATUS_MPP_M; // attivo interrupts and kernel mode
        newProcess->p_s.mie = MIE_ALL; // attivo tutti gli interrupts
        newProcess->p_s.reg_sp = RAMTOP(newProcess->p_s.reg_sp); // SP settata a RAMTOP
        newProcess->p_s.pc_epc = (memaddr)test; // program counter settato a test

        // inizializzo il pcb a null
        newProcess->p_parent = NULL;
        newProcess->p_child.next = NULL;
        newProcess->p_child.prev = NULL;
        newProcess->p_sib.next = NULL;
        newProcess->p_sib.prev = NULL;

        newProcess->p_time = 0;
        
        newProcess->p_semAdd = NULL;
        newProcess->p_supportStruct = NULL;

        insertProcQ(&readyQueue, newProcess);
        processCount++;
    }
}

void interruptRouting(){
    // configuriamo la Interrupt Routing Table (IRT)
    for (int i = 0; i < IRT_NUM_ENTRY; i++) {
        memaddr *irt_entry = (memaddr *)(IRT_START + (i * sizeof(memaddr)));
        *irt_entry = IRT_RP_BIT_ON | ((1 << NCPU) - 1); 
    }

    // Impostiamo la Task Priority Register (TPR) per ogni CPU
    for (int cpu_id = 0; cpu_id < NCPU; cpu_id++) {
        if (currentProcess[cpu_id] == NULL) {
            *((memaddr *)TPR) = 1;
        } else {
            *((memaddr *)TPR) = 0;
        }
    }

}


int main(){
    initializePassUpVector();
    initializeVariables();

    initPcbs();
    instantiateProcess();
    initASL();
    

    LDIT(PSECOND * (*((memaddr *)TIMESCALEADDR)));


    for (int i = 0; i < NCPU; i++) {
        currentProcess[i] = allocPcb();
        currentProcess[i]->p_s.status = MSTATUS_MPP_M;
        currentProcess[i]->p_s.pc_epc = (memaddr) scheduler;
        if(i>=1){
            currentProcess[i]->p_s.reg_sp=0x20020000 + (i * PAGESIZE);
        }
        currentProcess[i]->p_s.mie = 0;
        currentProcess[i]->p_parent = NULL;
        currentProcess[i]->p_child.next = NULL;
        currentProcess[i]->p_child.prev = NULL;
        currentProcess[i]->p_sib.next = NULL;
        currentProcess[i]->p_sib.prev = NULL;
        currentProcess[i]->p_time = 0;
        currentProcess[i]->p_semAdd = NULL;
        currentProcess[i]->p_supportStruct = NULL;
    }

    scheduler();
    return 1;
}

int* process_count() { return &processCount; }

unsigned int* global_lock() { return &globalLock; }

pcb_t** current_process() { return (getPRID() < NCPU) ? &currentProcess[getPRID()] : NULL; }

struct list_head* ready_queue() { return &readyQueue; }

//int *device_semaphores(unsigned int devNo) { return (devNo < NCPU) ? deviceSemaphores[devNo] : NULL; }
int *device_semaphores(unsigned int devNo) {return (devNo <= PSEUDO_CLOCK_SEM) ? &deviceSemaphores[devNo] : NULL; }