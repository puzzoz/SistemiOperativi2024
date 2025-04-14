#ifndef MULTIPANDOS_INITIAL_H
#define MULTIPANDOS_INITIAL_H

#include "./headers/initial.h"
#include "../phase1/headers/asl.h"
#include "../phase1/headers/pcb.h"
#include "../headers/types.h"
#include "../headers/const.h"

#endif //MULTIPANDOS_INITIAL_H

//LEVEL 3 GLOBAL VARIABLES
/*
static int processCount = 0; //num di processi iniziati ma non ancora terminati

//queue dei PCB in READY state
static struct list_head readyQueue_h;
static pcb_t readyQueue;

//vettore di pointer ai pcb con state "running" in ogni CPU currentProcess
pcb_t *currentProcess[NCPU];

int deviceSemaphores[NCPU][2]; // 2 semafori per ogni subdevice

int globalLock; //puo' avere solo valore 0 e 1


//funzione Placeholder per uTLB_RefillHandler
extern void uTLB_RefillHandler(void);

//Level 3 Nucleus exception handler
extern void exceptionHandler(void);
*/

void initializePassUpVector() {
    for (int cpu_id = 0; cpu_id < NCPU; cpu_id++){ 
        passupvector_t *passupvector = (passupvector_t *)(BIOSDATAPAGE + (cpu_id * sizeof(passupvector_t)));

        
        passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;

        // Stack Pointer per TLB-Refill event handler
        if (cpu_id == 0) {
            passupvector->tlb_refill_stackPtr = KERNELSTACK;
        } else {
            passupvector->tlb_refill_stackPtr = 0x20020000 + (cpu_id * PAGESIZE);
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
    process_count = 0;

    INIT_LIST_HEAD(&ready_queue);

    memset(&ready_queue, 0, sizeof(pcb_t));

    for (int i = 0; i < NCPU; i++) {
        current_process[i] = NULL;
    }

    memset(device_semaphores, 0, sizeof(device_semaphores));

    global_lock = 0;
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

        insertProcQ(&ready_queue, newProcess);

        process_count++;
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
        if (current_process[cpu_id] == NULL) {
            *((memaddr *)TPR) = 1;
        } else {
            *((memaddr *)TPR) = 0;
        }
    }

}


void main(){
    initializePassUpVector();

    initPcbs();
    initASL();
    
    initializeVariables();

    LDIT(PSECOND * (*((memaddr *)TIMESCALEADDR)));
    
    instantiateProcess();

    for (int i = 0; i < NCPU-1; i++) {
        current_process[i]->p_s.status = MSTATUS_MPP_M;
        current_process[i]->p_s.pc_epc = (memaddr) scheduler();
        if(i>=1){
            current_process[i]->p_s.reg_sp=0x20020000 + (i * PAGESIZE);
        }
        current_process[i]->p_s.mie = 0;
        current_process[i]->p_parent = NULL;
        current_process[i]->p_child.next = NULL;
        current_process[i]->p_child.prev = NULL;
        current_process[i]->p_sib.next = NULL;
        current_process[i]->p_sib.prev = NULL;
        current_process[i]->p_time = 0;
        current_process[i]->p_semAdd = NULL;
        current_process[i]->p_supportStruct = NULL;
    }

    scheduler();
}
