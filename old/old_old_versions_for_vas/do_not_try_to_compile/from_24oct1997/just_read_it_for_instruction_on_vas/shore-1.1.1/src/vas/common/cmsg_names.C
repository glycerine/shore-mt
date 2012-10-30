/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/common/cmsg_names.C,v 1.3 1995/05/01 16:36:31 nhall Exp $
 */
#include <copyright.h>
#include <stream.h>
#include "msg_stats.h"

// This is in common so that both client and server
// can use it.

typedef unsigned long 	u_long;

static const int cmsg_values[] = {
#include "cmsg_stats.i"
	-1
};
static const char *cmsg_strings[] = {
#include "cmsg_names.i"
	(char *)0
};

struct msg_info cmsg_names = {
	cmsg_values,
	cmsg_strings
};
