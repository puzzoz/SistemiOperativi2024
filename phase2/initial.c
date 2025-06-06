#include "./headers/initial.h"
#include "../phase1/headers/asl.h"
#include "./headers/scheduler.h"

//LEVEL 3 GLOBAL VARIABLES

int processCount = 0; //num di processi iniziati ma non ancora terminati

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

int deviceSemaphores[DEVICE_SEMS+1]; // 2 semafori per ogni subdevice

unsigned int globalLock; //puo' avere solo valore 0 e 1


void initializePassUpVector() {
    for (int cpu_id = 0; cpu_id < NCPU; cpu_id++){
        passupvector_t *passupvector = (passupvector_t *)(0x0FFFF900 + (cpu_id * sizeof(passupvector_t)));

        passupvector->tlb_refill_handler = (memaddr)uTLB_RefillHandler;

        // Stack Pointer per TLB-Refill event handler
        if (cpu_id == 0) {
            passupvector->tlb_refill_stackPtr = KERNELSTACK;
        } else {
            //riga delle vecchie specifiche:
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
    processCount = 0;
    softBlockCount = 0;

    INIT_LIST_HEAD(&readyQueue);

    for (int i = 0; i < NCPU; i++) {
        currentProcess[i] = NULL;
    }

    for (int i = 0; i < DEVICE_SEMS + 1; ++i) {
        deviceSemaphores[i] = 0;
    }

    globalLock = 0;
}
extern void test();

void instantiateProcess() {
    pcb_t *newProcess = allocPcb();

    // inizializzo stato processore
    newProcess->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M; // attivo interrupts and kernel mode
    newProcess->p_s.mie = MIE_ALL; // attivo tutti gli interrupts
    RAMTOP(newProcess->p_s.reg_sp); // SP settata a RAMTOP
    insertProcQ(&readyQueue, newProcess);
    processCount++;
    newProcess->p_s.pc_epc = (memaddr)test; // program counter settato a test
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
    initPcbs();
    initASL();
    initializeVariables();
    interruptRouting();
    instantiateProcess();

    LDIT(PSECOND * (*((memaddr *)TIMESCALEADDR)));

    for (int i = 0; i < NCPU; i++) {
        pcb_t *p = allocPcb();
        currentProcess[i] = p;
        //metto in kernel‐mode con MPIE=1
        p->p_s.status = MSTATUS_MPIE_MASK | MSTATUS_MPP_M;
        //disabilito interrupt locali
        p->p_s.mie    = 0;
        p->p_s.pc_epc = (memaddr)scheduler;
        if (i >= 1) {
            //inizializzo passUpVector e SP per la CPU i
            INITCPU(i, &p->p_s);
            p->p_s.reg_sp = 0x20020000 + (i * PAGESIZE);
        }
        //la CPU i parte subito in scheduler()
    }
    scheduler();
}

int *device_semaphores(unsigned int devNo) {return (devNo <= PSEUDO_CLOCK_SEM) ? &deviceSemaphores[devNo] : NULL; }