#include "kaskas/subsystems/fluidsystem.hpp"

static volatile uint32_t s_counter = 0;

void interrupt_callback() { s_counter++; }

uint32_t Fluidsystem::interrupt_counter() { return s_counter; }
void Fluidsystem::reset_interrupt_counter() { s_counter = 0; }

void Fluidsystem::attach_interrupt() { _cfg.interrupt.attach_interrupt(interrupt_callback); }
void Fluidsystem::detach_interrupt() { _cfg.interrupt.detach_interrupt(); }