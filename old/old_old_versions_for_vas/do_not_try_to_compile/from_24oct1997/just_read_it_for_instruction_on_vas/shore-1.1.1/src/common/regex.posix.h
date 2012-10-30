/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: regex.posix.h,v 1.4 1997/01/24 16:31:27 nhall Exp $
 */
#ifndef _REGEX_POSIX_H_
#define _REGEX_POSIX_H_


/*
 * hpux does not have re_comp and re_exec.  Instead there is
 * regcomp() and regexec().
 */

#    define _INCLUDE_XOPEN_SOURCE
#    include <regex.h>
#    include <assert.h>

#    define re_comp re_comp_posix
#    define re_exec re_exec_posix

#ifdef __cplusplus
extern "C" {
#endif
	char* re_comp_posix(const char* pattern);
	int	re_exec_posix(const char* string);
#ifdef __cplusplus
}
#endif

#endif/* _REGEX_POSIX_H_ */
