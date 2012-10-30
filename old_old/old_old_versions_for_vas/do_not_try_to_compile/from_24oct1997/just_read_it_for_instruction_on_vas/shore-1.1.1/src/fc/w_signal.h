/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef  W_SIGNAL_H
#define  W_SIGNAL_H
/*
 *  $Id: w_signal.h,v 1.12 1997/05/19 19:39:28 nhall Exp $
 */
/* 
 * workarounds for 
 * various systems' signal definitions, some of which
 * are wrong (not ANSI-C, even though they claim to be)
 */
#ifndef W_WORKAROUND_H
#include <w_workaround.h>
#endif

#ifdef GNUG_BUG_8
#define signal __gnubug_signal__
#endif 

#define	POSIX_SIGNALS

#include <stdlib.h>
#include <signal.h>

#ifdef GNUG_BUG_8
#undef signal
extern "C" void (*signal(int sig, void (*handler)(int)))(int);
#endif 

/* ANSI standard C defines signal() and handler: */
typedef void (*_W_ANSI_C_HANDLER)( int );
#define W_ANSI_C_HANDLER (_W_ANSI_C_HANDLER) /* type-cast */

/* POSIX defines sigaction() and  its handler differently from standard C */
typedef void (*_W_POSIX_HANDLER)(...);
#define W_POSIX_HANDLER (_W_POSIX_HANDLER) /* type-cast */

/* prevent use of bsd functions -- use posix ones instead */
#if 0
#ifdef sigblock
#undef sigblock
#endif
#define sigblock do not use BSD signal functions

#ifdef sigsetmask
#undef sigsetmask
#endif
#define sigsetmask do not use BSD signal functions

#ifdef sigmask
#undef sigmask
#endif
#define sigmask do not use BSD signal functions
#endif /*0*/
#endif  /* W_SIGNAL_H */
