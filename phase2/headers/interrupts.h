#ifndef MULTIPANDOS_INTERRUPTS_H
#define MULTIPANDOS_INTERRUPTS_H

#include "../../headers/types.h" //forse non server, però mi dice sempre che non sa state_t
#include <uriscv/state.h>
void dispatchInterrupt(int cause, state_t *exception_state);

#endif //MULTIPANDOS_INTERRUPTS_H
