#ifndef _CONFIG_H_
#define _CONFIG_H_
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/* Find out what kind of platform we are are on.  It would be nice to use
 * GNU autoconf (e.g. HAVE_THIS and USE_THAT), but this program is _so_ 
 * platform-specific, there's little hope of doing better than simply
 * enumerating all the platforms supported and special-casing each one.
 *
 * We use the gcc "assert" feature so that
 *    #if #platform(xxx)
 * will succeed for exactly one value of xxx chosen from the list of recognized
 * platforms, currently:
 *		sunos
 *		solaris
 *		hpux
 *		ultrix
 *		linux
 * WARNING:  Not all of these platforms work yet.  Currently working:
 *		sunos
 *
 */

/* sunos */
#if #system(unix) && #system(bsd) && #cpu(sparc)
#if #platform
#error ambiguous platform
#endif
#assert platform (sunos)
#endif

/* solaris */
#if #system(unix) && #system(svr4) && (#cpu(sparc) || #cpu(i386))
#if #platform
#error ambiguous platform
#endif
#assert platform (solaris)
#endif

/* hpux */
#if #system(unix) && #system(hpux) && #cpu(hppa)
#if #platform
#error ambiguous platform
#endif
#assert platform (hpux)
#endif

/* (mips) ultrix */
#if #system(unix) && #system(bsd) && #cpu(mips)
#if #platform
#error ambiguous platform
#endif
#assert platform (ultrix)
#endif

/* linux */
#if #system(unix) && #system(posix) && #cpu(i386)
#if #platform
#error ambiguous platform
#endif
#assert platform (linux)
#endif

#if !#platform
#error unknown platform type
#endif

#endif /* _CONFIG_H_ */
