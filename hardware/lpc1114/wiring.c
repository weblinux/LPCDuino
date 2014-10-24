/*
  wiring.c - Partial implementation of the Wiring API for the ATmega8.
  Part of Arduino - http://www.arduino.cc/

  Copyright (c) 2005-2006 David A. Mellis

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General
  Public License along with this library; if not, write to the
  Free Software Foundation, Inc., 59 Temple Place, Suite 330,
  Boston, MA  02111-1307  USA

  $Id$
*/

#include "wiring_private.h"
#include <stdio.h>

// the prescaler is set so that timer0 ticks every 48 clock cycles, and the
// the overflow handler is called every 1000 ticks.
#define MICROSECONDS_PER_TIMER0_OVERFLOW (clockCyclesToMicroseconds(48 * 1000))

// the whole number of milliseconds per timer0 overflow
#define MILLIS_INC (MICROSECONDS_PER_TIMER0_OVERFLOW / 1000)

// the fractional number of milliseconds per timer0 overflow. we shift right
// by three to fit these numbers into a byte. (for the clock speeds we care
// about - 8 and 16 MHz - this doesn't lose precision.)
#define FRACT_INC ((MICROSECONDS_PER_TIMER0_OVERFLOW % 1000) >> 3)
#define FRACT_MAX (1000 >> 3)

volatile unsigned long timer0_overflow_count = 0;
volatile unsigned long timer0_millis = 0;
static unsigned char timer0_fract = 0;

void TIMER0_OVF_vect()
{
  // copy these to local variables so they can be stored in registers
  // (volatile variables must be read from memory on every access)
  unsigned long m = timer0_millis;
  unsigned char f = timer0_fract;

  m += MILLIS_INC;
  f += FRACT_INC;
  if (f >= FRACT_MAX) {
    f -= FRACT_MAX;
    m += 1;
  }

  timer0_fract = f;
  timer0_millis = m;
  timer0_overflow_count++;

  //  Clear the interrupt flag
  TMR16B0IR     = 1;
}

unsigned long millis()
{
  unsigned long m;

  // disable interrupts while we read timer0_millis or we might get an
  // inconsistent value (e.g. in the middle of a write to timer0_millis)
  noInterrupts();
  m = timer0_millis;
  interrupts();

  return m;
}

unsigned long micros() {
  unsigned long m;
  uint16_t t;

  noInterrupts();
  m = timer0_overflow_count;
  t = TMR16B0TC;

  // if there is a pending overflow - increment by 1
  if ((TMR16B0IR & _BV(MR0INT)) && (t<999))
    {
      m++;
    }

  interrupts();

  //  there's a typecasting issue here with t.  
  return (m*1000) + t;
}

void delay(unsigned long ms)
{
  uint32_t start = micros();

  while (ms > 0) {
    if ((micros() - start) >= 1000) {
      ms--;
      start += 1000;
    }
  }
}

/* Delay for the given number of microseconds.  */
void delayMicroseconds(unsigned int us)
{
  uint32_t start = micros(), end;

  end = start + us;

  while (end>micros())
    asm("nop");
}


void init()
{
  uint32_t i;

  //  run the PLL at 48 mhz using the 12 mhz internal resonator as the source
  PDRUNCFG     &= ~(1 << 5);          // Power-up System Osc      
  SYSOSCCTRL    = 0x00000000;
  for (i = 0; i < 200; i++) asm("nop");
  SYSPLLCLKSEL  = 0x00000000;         // Select PLL Input         
  SYSPLLCLKUEN  = 0x01;               // Update Clock Source      
  SYSPLLCLKUEN  = 0x00;               // Toggle Update Register   
  SYSPLLCLKUEN  = 0x01;
  while (!(SYSPLLCLKUEN & 0x01));     // Wait Until Updated       
  SYSPLLCTRL    = 0x00000023;
  PDRUNCFG     &= ~(1 << 7);          // Power-up SYSPLL          
  while (!(SYSPLLSTAT & 0x01));       // Wait Until PLL Locked    
  MAINCLKSEL    = 0x00000003;         // Select PLL Clock Output  
  MAINCLKUEN    = 0x01;               // Update MCLK Clock Source 
  MAINCLKUEN    = 0x00;               // Toggle Update Register   
  MAINCLKUEN    = 0x01;
  while (!(MAINCLKUEN & 0x01));       // Wait Until Updated       
  
  //  enable clocks on advanced high-performance bus
  SYSAHBCLKCTRL = 0x3FFFF;

  //  configure timers
  //  the concept here is to run the TMR16B0 at 1 Mhz (tick per microsecond) 
  //  and run the ISR ever millisecond.

  //  Timer 16B0
  TMR16B0PR     = 47;                  // divide the 48 Mhz clock by 48
  TMR16B0MCR    = 3;                   // reset and interrupt on match
  TMR16B0MR0    = 1000;                // matching on 1000
  ISER          = 0x10000;             // enable timer0 interrupt
  TMR16B0TCR    = 0x1;                 // enable timer

  //  Timer 16B1
  TMR16B1PR     = 47;                  // divide the 48 Mhz clock by 48
  TMR16B1TCR    = 0x1;                 // enable timer

  //  Timer 32B0
  TMR32B0PR     = 47;                  // divide the 48 Mhz clock by 48
  TMR32B0TCR    = 0x1;                 // enable timer

  //  Timer 32B1
  TMR32B1PR     = 47;                  // divide the 48 Mhz clock by 48
  TMR32B1TCR    = 0x1;                 // enable timer
}






