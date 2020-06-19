/*-
 * Copyright (c) 1983, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This file is taken from Cygwin distribution. Please keep it in sync.
 * The differences should be within __MINGW32__ guard.
 */

#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "gmon.h"
#include "profil.h"
#include <stdint.h>
#include <string.h>

#define MINUS_ONE_P (-1)
#define bzero(ptr,size) memset (ptr, 0, size);
// #define ERR(s) write(2, s, sizeof(s))
// #define ERR(s) { __asm volatile("BKPT"); }
char *lasterror = "OK";
#define ERR(s) { lasterror = s; }

struct gmonparam _gmonparam = { GMON_PROF_OFF, NULL, 0, NULL, 0, NULL, 0, 0L, 0, 0, 0};
static char already_setup = 0; /* flag to indicate if we need to init */
static int	s_scale;
/* see profil(2) where this is described (incorrectly) */
#define		SCALE_1_TO_1	0x10000L

int gprof_memory_requested;

static void moncontrol(int mode);

#if 0
/* required for gcc ARM Embedded 4.9-2015-q2 */
void *_sbrk(int incr) {
  extern char __HeapLimit; /* Defined by the linker */
  static char *heap_end = 0;
  char *prev_heap_end;

  if (heap_end==0) {
    heap_end = &__HeapLimit;
  }
  prev_heap_end = heap_end;
  heap_end += incr;
  return (void *)prev_heap_end;
}
#endif

// #define STATIC_MEMORY_BUFFER 16324

#ifdef STATIC_MEMORY_BUFFER
#define mem_textsize STATIC_MEMORY_BUFFER
#define mem_size mem_textsize / HISTFRACTION + mem_textsize / HASHFRACTION + (int)(mem_textsize * ARCDENSITY / 100) * 12
uint8_t mem_buffer[mem_size];
#endif

__attribute__((no_instrument_function))
static void *fake_sbrk(int size) {
  gprof_memory_requested = size;

#ifdef STATIC_MEMORY_BUFFER
  void *rv ;
  if (size <= mem_size) rv = &mem_buffer;
  else rv = NULL;
#else
  void *rv = malloc(size);
#endif

  if (rv) {
    return rv;
  } else {
    return (void *) MINUS_ONE_P;
  }
}

__attribute__((no_instrument_function))
void monstartup (const size_t lowpc, const size_t highpc) {
	register size_t o;
	char *cp;
	struct gmonparam *p = &_gmonparam;

	/*
	 * round lowpc and highpc to multiples of the density we're using
	 * so the rest of the scaling (here and in gprof) stays in ints.
	 */
	p->lowpc = ROUNDDOWN(lowpc, HISTFRACTION * sizeof(HISTCOUNTER));
	p->highpc = ROUNDUP(highpc, HISTFRACTION * sizeof(HISTCOUNTER));
	p->textsize = p->highpc - p->lowpc;
	p->kcountsize = p->textsize / HISTFRACTION;
	p->fromssize = p->textsize / HASHFRACTION;
	p->tolimit = p->textsize * ARCDENSITY / 100;
	if (p->tolimit < MINARCS) {
		p->tolimit = MINARCS;
	} else if (p->tolimit > MAXARCS) {
		p->tolimit = MAXARCS;
	}
	p->tossize = p->tolimit * sizeof(struct tostruct);

	cp = fake_sbrk(p->kcountsize + p->fromssize + p->tossize);
	if (cp == (char *)MINUS_ONE_P) {
    p->state = GMON_PROF_ABORT;
		ERR("monstartup: out of memory\n");
		return;
	}

	/* zero out cp as value will be added there */
	bzero(cp, p->kcountsize + p->fromssize + p->tossize);

	p->tos = (struct tostruct *)cp;
	cp += p->tossize;
	p->kcount = (u_short *)cp;
	cp += p->kcountsize;
	p->froms = (u_short *)cp;

	p->tos[0].link = 0;

	o = p->highpc - p->lowpc;
	if (p->kcountsize < o) {
#ifndef notdef
		s_scale = ((float)p->kcountsize / o ) * SCALE_1_TO_1;
#else /* avoid floating point */
		int quot = o / p->kcountsize;

		if (quot >= 0x10000)
			s_scale = 1;
		else if (quot >= 0x100)
			s_scale = 0x10000 / quot;
		else if (o >= 0x800000)
			s_scale = 0x1000000 / (o / (p->kcountsize >> 8));
		else
			s_scale = 0x1000000 / ((o << 8) / p->kcountsize);
#endif
	} else {
		s_scale = SCALE_1_TO_1;
	}
	moncontrol(1); /* start */
}

#include "TeensyGProf.h"

__attribute__((no_instrument_function))
void _mcleanup(void) {
	static const char gmon_out[] = "gmon.out";
	int fd;
	int hz;
	int fromindex;
	int endfrom;
	size_t frompc;
	int toindex;
	struct rawarc rawarc;
	struct gmonparam *p = &_gmonparam;
	struct gmonhdr gmonhdr, *hdr;
	const char *proffile;
#ifdef DEBUG
	int log, len;
	char dbuf[200];
#endif

	if (p->state == GMON_PROF_ABORT) {
    return;
  }

	if (p->state == GMON_PROF_ERROR) {
		ERR("_mcleanup: tos overflow or error\n");
	}

	hz = PROF_HZ;
	moncontrol(0); /* stop */
	proffile = gmon_out;
	fd = TeensyProf_open(proffile , O_CREAT|O_TRUNC|O_WRONLY|O_BINARY, 0666);
	if (fd < 0) {
		//perror( proffile );
		return;
	}
#ifdef DEBUG
	log = TeensyProf_open("gmon.log", O_CREAT|O_TRUNC|O_WRONLY, 0664);
	if (log < 0) {
		//perror("mcount: gmon.log");
		return;
	}
	len = sprintf(dbuf, "[mcleanup1] kcount 0x%x ssiz %d\n",
	    p->kcount, p->kcountsize);
	write(log, dbuf, len);
#endif
	hdr = (struct gmonhdr *)&gmonhdr;
	hdr->lpc = p->lowpc;
	hdr->hpc = p->highpc;
	hdr->ncnt = p->kcountsize + sizeof(gmonhdr);
	hdr->version = GMONVERSION;
	hdr->profrate = hz;
	TeensyProf_write(fd, (char *)hdr, sizeof *hdr);
	TeensyProf_write(fd, p->kcount, p->kcountsize);
	endfrom = p->fromssize / sizeof(*p->froms);
	for (fromindex = 0; fromindex < endfrom; fromindex++) {
		if (p->froms[fromindex] == 0) {
			continue;
		}
		frompc = p->lowpc;
		frompc += fromindex * HASHFRACTION * sizeof(*p->froms);
		for (toindex = p->froms[fromindex]; toindex != 0; toindex = p->tos[toindex].link) {
#ifdef DEBUG
			len = sprintf(dbuf,
			"[mcleanup2] frompc 0x%x selfpc 0x%x count %d\n" ,
				frompc, p->tos[toindex].selfpc,
				p->tos[toindex].count);
			write(log, dbuf, len);
#endif
			rawarc.raw_frompc = frompc;
			rawarc.raw_selfpc = p->tos[toindex].selfpc;
			rawarc.raw_count = p->tos[toindex].count;
			TeensyProf_write(fd, &rawarc, sizeof rawarc);
		}
	}
	TeensyProf_close(fd);
}

/*
 * Control profiling
 *	profiling is what mcount checks to see if
 *	all the data structures are ready.
 */
__attribute__((no_instrument_function))
static void moncontrol(int mode) {
	struct gmonparam *p = &_gmonparam;

	if (mode) {
		/* start */
		profil((char *)p->kcount, p->kcountsize, p->lowpc, s_scale);
		p->state = GMON_PROF_ON;
	} else {
		/* stop */
		profil((char *)0, 0, 0, 0);
		p->state = GMON_PROF_OFF;
	}
}

__attribute__((no_instrument_function))
int gprof_start() {
  if (already_setup == 1) return 1;
  already_setup = 1;
#ifdef __IMXRT1062__
  extern char _stext; /* start of text/code symbol, defined by linker */
  extern char _etext; /* end of text/code symbol, defined by linker */
  monstartup((const size_t)&_stext, (const size_t)&_etext);
#else
  extern char _etext; /* end of text/code symbol, defined by linker */
  monstartup(0, (const size_t)&_etext);
#endif
  return _gmonparam.state == GMON_PROF_ON;
}

__attribute__((no_instrument_function))
int gprof_end() {
	if (_gmonparam.state == GMON_PROF_ABORT) {
    return 1;
  }
  _mcleanup();
  return 0;
}

int gprof_memory() {
  return gprof_memory_requested;
}

// long tickcounter = 0;

__attribute__((no_instrument_function))
void _mcount_internal(size_t *frompcindex, size_t *selfpc) {
  register struct tostruct	*top;
  register struct tostruct	*prevtop;
  register long			toindex;
  struct gmonparam *p = &_gmonparam;

  #ifdef GMON_DISABLE
  return;
  #endif

  // tickcounter++;

  if (already_setup == 0) {
    if (! gprof_start()) {
      already_setup = -1;
    }
  }

  /*
   *	check that we are profiling
   *	and that we aren't recursively invoked.
   */
  if (p->state!=GMON_PROF_ON) {
    goto out;
  }
  p->state++;

  /*
   *	check that frompcindex is a reasonable pc value.
   *	for example:	signal catchers get called from the stack,
   *			not from text space.  too bad.
   */
  frompcindex = (size_t*)((long)frompcindex - (long)p->lowpc);
  if ((unsigned long)frompcindex > p->textsize) {
      goto done;
  }

  frompcindex = (size_t*)&p->froms[((long)frompcindex) / (HASHFRACTION * sizeof(*p->froms))];
  toindex = *frompcindex;
  if (toindex == 0) {
    /*
    *	first time traversing this arc
    */
    toindex = ++p->tos[0].link; /* the link of tos[0] points to the last used record in the array */
    if (toindex >= p->tolimit) { /* more tos[] entries than we can handle! */
	    goto overflow;
	  }
    *frompcindex = toindex;
    top = &p->tos[toindex];
    top->selfpc = (size_t)selfpc;
    top->count = 1;
    top->link = 0;
    goto done;
  }
  if (toindex >= p->tolimit) { /* more tos[] entries than we can handle! */
    goto overflow_skip;
  }
  top = &p->tos[toindex];
  if (top->selfpc == (size_t)selfpc) {
    /*
     *	arc at front of chain; usual case.
     */
    top->count++;
    goto done;
  }
  /*
   *	have to go looking down chain for it.
   *	top points to what we are looking at,
   *	prevtop points to previous top.
   *	we know it is not at the head of the chain.
   */
  for (; /* goto done */; ) {
    if (top->link == 0) {
      /*
       *	top is end of the chain and none of the chain
       *	had top->selfpc == selfpc.
       *	so we allocate a new tostruct
       *	and link it to the head of the chain.
       */
      toindex = ++p->tos[0].link;
      if (toindex >= p->tolimit) {
        goto overflow;
      }
      top = &p->tos[toindex];
      top->selfpc = (size_t)selfpc;
      top->count = 1;
      top->link = *frompcindex;
      *frompcindex = toindex;
      goto done;
    }
    /*
     *	otherwise, check the next arc on the chain.
     */
    prevtop = top;
    top = &p->tos[top->link];
    if (top->selfpc == (size_t)selfpc) {
      /*
       *	there it is.
       *	increment its count
       *	move it to the head of the chain.
       */
      top->count++;
      toindex = prevtop->link;
      prevtop->link = top->link;
      top->link = *frompcindex;
      *frompcindex = toindex;
      goto done;
    }
  }
  done:
  overflow_skip:
    p->state--;
    /* and fall through */
  out:
    return;		/* normal return restores saved registers */
  overflow:
    ERR("_mcount_internal: tos overflow\n");
    p->state++; /* halt further profiling */
    // #define	TOLIMIT	"mcount: tos overflow\n"
    // write (2, TOLIMIT, sizeof(TOLIMIT));
  goto out;
}

#ifdef USE_INLINE
__attribute__((naked)) __attribute__((no_instrument_function))
void __gnu_mcount_nc() {
  __asm__(
#if 1 /* dummy version, doing nothing */
  "mov    ip, lr" "\n"
  "pop    { lr }" "\n"
  "bx     ip" "\n"
#else
  "push {r0, r1, r2, r3, lr}     /* save registers */" "\n"
  "bic r1, lr, #1                /* R1 contains callee address, with thumb bit cleared */" "\n"
  "ldr r0, [sp, #20]             /* R0 contains caller address */" "\n"
  "bic r0, r0, #1                /* clear thumb bit */" "\n"
  "bl _mcount_internal           /* jump to internal _mcount() implementation */" "\n"
  "pop {r0, r1, r2, r3, ip, lr}  /* restore saved registers */" "\n"
  "mov ip, lr" "\n"
  "pop {lr}" "\n"
  "bx  ip" "\n"
#endif
  );
}
#endif

__attribute__((no_instrument_function))
void _monInit(void) {
  _gmonparam.state = GMON_PROF_OFF;
  already_setup = 0;
}
