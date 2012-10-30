/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * hpux does not have re_comp and re_exec.  Instead there is
 * regcomp() and regexec().
 */

#include <stddef.h>
#include "w_base.h"
#include "regex.posix.h"

static regex_t 	re_posix_re;
static bool 	re_ready = false;
static char*	re_error_str = "Bad regular expression";

char* 
re_comp_posix(const char* pattern)
{
	// assert(!re_ready);

	if (re_ready) {
	    regfree(&re_posix_re);
	}

	if (regcomp(&re_posix_re, pattern, REG_NOSUB) != 0) {
		return re_error_str;
	}

	re_ready = true;
	return NULL;
}

int	
re_exec_posix(const char* string)
{
	int status;

	if(!re_ready) {
		return -1; // no string compiled
	}

	status = regexec(&re_posix_re, string, (size_t)0, NULL, 0);
	// re_ready = false;
	if (status == 0) return 1; // found match
	return 0; // no match
}

