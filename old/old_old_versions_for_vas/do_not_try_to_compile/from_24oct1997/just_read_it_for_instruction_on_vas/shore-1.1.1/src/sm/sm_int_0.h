/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef SM_INT_0_H
#define SM_INT_0_H

#if defined(SM_SOURCE) && !defined(SM_LEVEL)
#    define SM_LEVEL 0
#endif

#include <sysdefs.h>
#include <basics.h>
#include <debug.h>
#include <sthread.h>
#include <vec_t.h>
#include <zvec_t.h>
#include <latch.h>
#include <rsrc.h>

#include <lid_t.h>
#include "sm_s.h"
#include "sm_base.h"
#include "smthread.h"
#include <tid_t.h>
#include "smstats.h"


#if (SM_LEVEL >= 0) 
#    include <bf.h>
#    include <page.h>
#    include <pmap.h>
#    include <io.h>
#    include <log.h>

#ifdef MULTI_SERVER
#    include <srvid_t.h>
#    include <comm.h>
#endif

#endif

#ifdef DEBUG
#define SMSCRIPT(x) 
#define RES_SMSCRIPT(x)
/*
#define SMSCRIPT(x) \
	scriptlog->clog << info_prio << "sm " x << flushl;
#define RES_SMSCRIPT(x)\
	scriptlog->clog << info_prio << "set res [sm " x << "]" << flushl;\
			scriptlog->clog << info_prio << "verbose $res" << flushl;
*/
#else
#define SMSCRIPT(x) 
#define RES_SMSCRIPT(x) 
#endif

#endif /*SM_INT_0_H*/

