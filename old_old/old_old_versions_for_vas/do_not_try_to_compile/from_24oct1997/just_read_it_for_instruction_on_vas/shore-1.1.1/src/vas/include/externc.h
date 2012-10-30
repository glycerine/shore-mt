/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __EXTERNC_H__
#define __EXTERNC_H__

#ifdef EXTERNC
#undef EXTERNC
#endif
#ifdef BEGIN_EXTERNCLIST
#undef BEGIN_EXTERNCLIST
#endif
#ifdef END_EXTERNCLIST
#undef END_EXTERNCLIST
#endif

#ifdef __cplusplus
#	define EXTERNC extern "C"
# 	define BEGIN_EXTERNCLIST extern "C" {
# 	define END_EXTERNCLIST  }
#	define _PROTO_(x)  x
#else
# 	define BEGIN_EXTERNCLIST 
# 	define END_EXTERNCLIST 
# 	define EXTERNC 
#	define _PROTO_(x)  ( )
#endif

#endif
