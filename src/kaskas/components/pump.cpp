#include "kaskas/subsystems/fluidsystem.hpp"

static volatile uint32_t s_counter = 0;

void interrupt_callback() { s_counter++; }

uint32_t Pump::interrupt_counter() { return s_counter; }
void Pump::reset_interrupt_counter() { s_counter = 0; }

void Pump::attach_interrupt() { _interrupt.attach_interrupt(interrupt_callback); }
void Pump::detach_interrupt() { _interrupt.detach_interrupt(); }