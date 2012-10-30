/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __bzero_h__
#define __bzero_h__
/*
 * sysdefs.h defines bzero to be an error message to force users
 * to use memset.  But, the FD_ZERO system macro uses bzero on
 * sunos, so undef it here.
 */

#include <externc.h>

#	ifdef bzero
#		undef bzero 
#	endif

#	ifdef bcopy
#		undef bcopy 
#	endif

#if defined(HPUX8) || defined(Ultrix42)
#include <string.h>
#else
#   if (__GNUC_MINOR__ < 7)
#	    include <memory.h>
#   else
#	    include <string.h>
#   endif /* __GNUC_MINOR__ < 7 */
#endif

#	define bzero(a,b) memset(a,'\0',b)
#	define bcopy(a,b,c) memcpy(b,a,c)

BEGIN_EXTERNCLIST 
	int ffs(int);
END_EXTERNCLIST 

#endif 
