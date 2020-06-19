/* profil.c -- win32 profil.c equivalent

   Copyright 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.

   This file is part of Cygwin.

   This software is a copyrighted work licensed under the terms of the
   Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
   details. */

/*
 * This file is taken from Cygwin distribution, adopted to be used for bare embeeded targets.
 */

#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <math.h>
#include "profil.h"
#include <string.h>
#include <stdint.h>

/* global profinfo for profil() call */
static struct profinfo prof = {
  PROFILE_NOT_INIT, 0, 0, 0, 0
};

extern void (* _VectorsRam[])(void);
void systick_isr(void);
void (* save_isr)(void) = systick_isr;

// long tickcounter = 0;

uint32_t stackpc;

/* sample the current program counter */
//void SysTick_Handler(void) {
  //void OSA_SysTick_Handler(void);

__attribute__((no_instrument_function))
void gprof_systick_isr_step2(void) {
  static size_t pc, idx;

  if (prof.state==PROFILE_ON) {
    // pc = ((uint32_t*)(__builtin_frame_address(0)))[14]; /* get SP and use it to get the return address from stack */
    pc = stackpc;
    if (pc >= prof.lowpc && pc < prof.highpc) {
      idx = PROFIDX (pc, prof.lowpc, prof.scale);
      prof.counter[idx]++;
    }
    // tickcounter++;
  }
  save_isr(); /* call saved SysTick handler */
}

__attribute__((no_instrument_function, naked))
void gprof_systick_isr(void) {
  asm volatile("ldr r0, [sp, #24]"); // get PC from interrupt stack
  asm volatile("ldr r1, =stackpc");  // we're going to store it in stackpc
  asm volatile("str r0, [r1]");      // store it
  asm volatile("push {lr}");
  gprof_systick_isr_step2();
  asm volatile("pop {pc}");
}

/* Stop profiling to the profiling buffer pointed to by p. */
__attribute__((no_instrument_function))
static int profile_off (struct profinfo *p) {
  _VectorsRam[15] = save_isr;
  p->state = PROFILE_OFF;
  return 0;
}

/* Create a timer thread and pass it a pointer P to the profiling buffer. */
__attribute__((no_instrument_function))
static int profile_on (struct profinfo *p) {
  save_isr = _VectorsRam[15];
  _VectorsRam[15] = gprof_systick_isr;
  p->state = PROFILE_ON;
  return 0; /* ok */
}

/*
 * start or stop profiling
 *
 * profiling goes into the SAMPLES buffer of size SIZE (which is treated
 * as an array of u_shorts of size size/2)
 *
 * each bin represents a range of pc addresses from OFFSET.  The number
 * of pc addresses in a bin depends on SCALE.  (A scale of 65536 maps
 * each bin to two addresses, A scale of 32768 maps each bin to 4 addresses,
 * a scale of 1 maps each bin to 128k address).  Scale may be 1 - 65536,
 * or zero to turn off profiling
 */
__attribute__((no_instrument_function))
int profile_ctl (struct profinfo *p, char *samples, size_t size, size_t offset, unsigned int scale) {
  size_t maxbin;

  if (scale > 65536) {
    errno = EINVAL;
    return -1;
  }
  profile_off(p);
  if (scale) {
    memset(samples, 0, size);
    memset(p, 0, sizeof *p);
    maxbin = size >> 1;
    prof.counter = (u_short*)samples;
    prof.lowpc = offset;
    prof.highpc = PROFADDR(maxbin, offset, scale);
    prof.scale = scale;
    return profile_on(p);
  }
  return 0;
}

/* Equivalent to unix profil()
   Every SLEEPTIME interval, the user's program counter (PC) is examined:
   offset is subtracted and the result is multiplied by scale.
   The word pointed to by this address is incremented. */
__attribute__((no_instrument_function))
int profil (char *samples, size_t size, size_t offset, unsigned int scale) {
  return profile_ctl (&prof, samples, size, offset, scale);
}

