//
// This file is part of the GNU ARM Eclipse Plug-ins project.
// Copyright (c) 2014 Liviu Ionescu.
//

#include "Timer.h"
#include "cortexm/ExceptionHandlers.h"

// ----------------------------------------------------------------------------

// Forward declarations.

extern void gra_tick(void);

void timer_tick (void);

// ----------------------------------------------------------------------------

volatile timer_ticks_t timer_delayCount;
volatile timer_ticks_t timer_delayCount2;

// ----------------------------------------------------------------------------

void
timer_start (void)
{
  // Use SysTick as reference for the delay loops.
  SysTick_Config (SystemCoreClock / TIMER_FREQUENCY_HZ);
}

void add_timer_task (timer_ticks_t ticks)
{
   timer_delayCount2 = ticks;
}

void timer_sleep (timer_ticks_t ticks)
{
  timer_delayCount = ticks;

  // Busy wait until the SysTick decrements the counter to zero.
  while (timer_delayCount != 0u)
    ;
}

void timer_tick (void)
{
  // Decrement to zero the counter used by the delay routine.
  if (timer_delayCount != 0u)
    {
      --timer_delayCount;
    }

  if (timer_delayCount2 != 0u)
    {
      --timer_delayCount2;
    }

  if(timer_delayCount2 == 1u)
  {
     gra_tick();
  }
}

// ----- SysTick_Handler() ----------------------------------------------------

void
SysTick_Handler (void)
{
  timer_tick ();
}

// ----------------------------------------------------------------------------
