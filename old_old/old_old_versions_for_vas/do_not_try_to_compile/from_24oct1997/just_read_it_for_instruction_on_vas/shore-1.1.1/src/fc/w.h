/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: w.h,v 1.14 1997/05/19 19:39:24 nhall Exp $
 */
#ifndef W_H
#define W_H

#ifndef W_BASE_H
#include <w_base.h>
#endif

#ifndef W_MINMAX_H
#include <w_minmax.h>
#endif

#ifndef W_LIST_H
#include <w_list.h>
#endif

#ifndef W_HASH_H
#include <w_hash.h>
#endif

#ifdef W_INCL_SHMEM
#ifndef SHMEM_H
#include <w_shmem.h>
#endif
#endif

#ifdef W_INCL_CIRCULAR_QUEUE
#ifndef W_CIRQUEUE_H
#include <w_cirqueue.h>
#endif
#endif

#ifdef W_INCL_TIMER
#ifndef W_TIMER_H
#include <w_timer.h>
#endif
#endif

#ifdef W_INCL_BITMAP
#ifndef W_BITMAP_H
#include <w_bitmap.h>
#endif
#endif

#endif /*W_H*/
