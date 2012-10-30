/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#if !defined(__SMCALLS_H__) && !defined(RPC_HDR)
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/smcalls.h,v 1.8 1995/04/24 19:47:31 zwilling Exp $
 */

/* 
 * No file can include <msg.h> AND use the SM directly
 */

#define __SMCALLS_H__

#include <copyright.h>
#include <assert.h>

#ifdef DEBUG
#	define DBGSSM(a) \
	(_debug.flag_on(("ss_m"),__FILE__) &&\
	(_debug.clog <<::dec(__LINE__) << " " << __FILE__ << ": " a << flushl ))
#else
#	define DBGSSM(a)  0
#endif

#define SMCALL(x) (DBGSSM(#x),(smerrorrc = ShoreVasLayer.Sm->x),smerrorrc)
#define CALL(x) (DBGSSM(#x),(smerrorrc = x),smerrorrc)

#define HANDLECALL(x) {\
	check_lsn(); DBGSSM(<< "handle." << #x );\
	if((smerrorrc = handle.x)){\
		OBJERR(SVAS_SmFailure,ET_VAS);\
		FAIL;\
	}\
	check_lsn();\
}

#endif /* __SMCALLS_H__ */
