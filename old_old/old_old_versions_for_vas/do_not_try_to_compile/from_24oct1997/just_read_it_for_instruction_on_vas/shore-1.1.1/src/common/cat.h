/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/cat.h,v 1.9 1997/05/19 19:40:56 nhall Exp $
 */
#ifndef CAT_H
#define CAT_H

/*
 * NB -- THIS FILE MUST BE LEGITIMATE INPUT TO cc and RPCGEN !!!!
 * Please do as follows:
 * a) keep all comments in traditional C style.
 * b) If you put something c++-specific make sure it's 
 * 	  got ifdefs around it
 */

#if defined(__STRICT_ANSI__)||defined(__GNUC__)||defined(__STDC__)||defined(__ANSI_CPP__)
#	define __cat(a,b) a##b
#	define _cat(a,b) __cat(a,b)
#	define _string(a) #a
#else

#ifdef COMMENT
/*  For compilers that don't understand # and ##, try one of these: 
//# error	preprocessor does not understand ANSI catenate and string.
*/

#		define _cat(a,b) a\
b
#		define _cat(a,b) a/**/b
#		define _string(a) "a"

#endif

#ifdef sparc
#		define _cat(a,b) a\
b
#		define _string(a) "a"
#endif

#endif

#endif /*CAT_H*/
