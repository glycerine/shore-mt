/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifdef notdef
// 
// removed for now because lid_t now has a hash func
//
/*
 * $Header: /p/shore/shore_cvs/src/vas/client/in.C,v 1.4 1995/04/24 19:43:28 zwilling Exp $
 */
#include <copyright.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <externc.h>
#include <debug.h>

BEGIN_EXTERNCLIST 
#include <arpa/inet.h>
	u_long localaddrshift(u_long);
END_EXTERNCLIST 

u_long
localaddrshift(u_long z)
{
	int res;
	in_addr a;

	a.s_addr = z;
	res = inet_lnaof(a);
	DBG( << "net of " <<  inet_ntoa(a) << "=" << res );
	// shift it over half a longword
	return ((u_long) res << ((sizeof(u_long)/2)*8));
}

#endif
