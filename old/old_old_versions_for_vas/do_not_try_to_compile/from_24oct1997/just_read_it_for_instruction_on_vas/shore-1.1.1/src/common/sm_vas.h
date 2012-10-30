/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm_vas.h,v 1.19 1997/05/19 19:41:08 nhall Exp $
 */
#ifndef SM_VAS_H
#define SM_VAS_H

/*
 * sm_vas.h is the include file that all value added servers should
 * to get access to the Shore Server storage manager interface.
 */
#include <stddef.h>
#include <iostream.h>
#include <strstream.h>

#ifndef	 RPC_HDR
#include "w.h"
#include "option.h"
#include "basics.h"
#include "lid_t.h"
#include "vec_t.h"
#include "zvec_t.h"
#include "tid_t.h"
#endif 	/* RPC_HDR */

#include "sm_s.h"
#undef SM_SOURCE
#include "sm_int_4.h"
#include "smthread.h"
#include "sm.h"
#include "file_s.h"
#include "pin.h"
#include "xct_dependent.h"
#include "scan.h"
#include "sort.h"
#include "nbox.h"

#include "lock.h"
#include "deadlock_events.h"

#endif /* SM_VAS_H */
