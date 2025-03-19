#ifndef MULTIPANDOS_EXCEPTIONS_H
#define MULTIPANDOS_EXCEPTIONS_H

#include "headers/exceptions.h"
#include <uriscv/liburiscv.h>
#include <uriscv/cpu.h>


void interruptExcHandler() {}

void syscallExcHandler() {}

void programTrapExcHandler() {}

void tlbExcHandler() {}

void exceptionHandler() {
    unsigned int cause = getCAUSE();
    if (CAUSE_IS_INT(cause)) interruptExcHandler(); // interrupt exception handling
    switch (CAUSE_GET_EXCCODE(cause)) {
        case EXC_ECU:
        case EXC_ECM:
            // SYSCALL exception handling
            syscallExcHandler();
            break;
        case EXC_IAM ... EXC_SAF:
        case EXC_ECS ... (EXC_ECM-1):
        case EXC_IPF ... (EXC_MOD-1):
            // Program Trap exception handling
            programTrapExcHandler();
            break;
        case EXC_MOD ... EXC_UTLBS:
            // TLB exception handling
            tlbExcHandler();
            break;
    }
}

#endif //MULTIPANDOS_EXCEPTIONS_H
