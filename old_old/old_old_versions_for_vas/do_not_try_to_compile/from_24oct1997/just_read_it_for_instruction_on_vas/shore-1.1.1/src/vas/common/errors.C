/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/common/errors.C,v 1.14 1995/04/24 19:44:09 zwilling Exp $
 */

#include <copyright.h>
#include <errors.h>
#include <externc.h>

void
catastrophic(
	const char *msg,
	USAGEFUNC	f
) 
{
	if(errno) {
		perror(msg);
	} else {
		cerr << msg << endl;
	}
	if(f) {
		(*f)(cerr);
	}
	// skip destructors
	_exit(1);
}

