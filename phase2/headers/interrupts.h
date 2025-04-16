#ifndef MULTIPANDOS_INTERRUPTS_H
#define MULTIPANDOS_INTERRUPTS_H

#include "../../headers/types.h"
void dispatchInterrupt(int cause, state_t *exception_state);

#endif //MULTIPANDOS_INTERRUPTS_H
