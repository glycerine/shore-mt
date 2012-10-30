/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: chkpt_serial.cc,v 1.2 1997/06/15 03:14:27 solomon Exp $
 */

#define SM_SOURCE
#define CHKPT_SERIAL_C

#ifdef __GNUG__
#   pragma implementation
#endif

#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

#include <sm_int_0.h>
#include <chkpt_serial.h>


/*********************************************************************
 *
 *  Fuzzy checkpoints and prepares cannot be inter-mixed in the log.
 *  This mutex is for serializing them.
 *
 *********************************************************************/
smutex_t                      chkpt_serial_m::_chkpt_mutex("_xct_chkpt");

void
chkpt_serial_m::chkpt_mutex_release()
{
    FUNC(chkpt_m::chkpt_mutex_release);
    DBGTHRD(<<"done logging a prepare");
    _chkpt_mutex.release();
}

void
chkpt_serial_m::chkpt_mutex_acquire()
{
    FUNC(chkpt_m::chkpt_mutex_acquire);
    DBGTHRD(<<"wanting to log a prepare...");
    W_COERCE(_chkpt_mutex.acquire());
    DBGTHRD(<<"may now log a prepare...");
}

