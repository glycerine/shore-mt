/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/common/vcmsg_names.C,v 1.3 1995/04/24 19:45:15 zwilling Exp $
 */
#include <stream.h>
#include "msg_stats.h"

// This is in common so that both client and server
// can use it.
// V messages and C messages are combined because
// they come from the same msg.h file.
// (unfortunate)

#ifndef __MSG_H__
#include "msg.defs.h"
#endif

typedef unsigned long 	u_long;

struct msg_names vcmsg_names[] = {
#include "msg_stats.i"
	{ 0, (char *)0 }
};
