/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __ERRORS_H__
#define __ERRORS_H__
/*
 * $Header: /p/shore/shore_cvs/src/vas/common/errors.h,v 1.10 1995/04/24 19:44:11 zwilling Exp $
 */

#include <copyright.h>
#include <errno.h>
#include <stdio.h>
#include <stream.h>
#include <stdlib.h>
#include <assert.h>
#include <externc.h>

typedef void (*USAGEFUNC)(ostream &out);

EXTERNC void catastrophic(const char *msg, USAGEFUNC	f=0); 

#endif /*__ERRORS_H__*/
