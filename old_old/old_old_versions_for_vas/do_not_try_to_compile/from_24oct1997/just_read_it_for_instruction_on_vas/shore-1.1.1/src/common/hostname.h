/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef _HOSTNAME_H_
#define _HOSTNAME_H_
/*
 * $Header: /p/shore/shore_cvs/src/common/hostname.h,v 1.5 1995/07/25 17:02:58 mcauliff Exp $
 */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(Sparc) || defined(Mips) || defined(I860)
	extern int gethostname(char *, int);
#endif
#if defined(HPUX8) && !defined(_INCLUDE_HPUX_SOURCE)
	extern int gethostname(char *, size_t);
#endif

#ifdef __cplusplus
}
#endif

#endif /*_HOSTNAME_H_*/
