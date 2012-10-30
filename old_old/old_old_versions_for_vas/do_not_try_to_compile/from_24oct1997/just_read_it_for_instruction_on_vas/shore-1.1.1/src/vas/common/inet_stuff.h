/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __inet_stuff_h__
#define __inet_stuff_h__
/*
 * $Header: /p/shore/shore_cvs/src/vas/common/inet_stuff.h,v 1.13 1995/08/04 23:07:50 nhall Exp $
 */
#include <copyright.h>
#include <externc.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef SOLARIS2
#include <sys/byteorder.h>
#endif

enum { other_machine=2, same_machine=1, same_process=0, error_remoteness=-1};

BEGIN_EXTERNCLIST

#ifdef OSF1AD
#include <machine/endian.h>
#endif

	char	*inet_ntoa(struct in_addr in);


	void	dumpaddrs(int fd, char *str);
	int 	max_sock_opt(int, int, int max=-1);
	int 	get_sock_opt(int, int);
	void 	set_sock_opt(int, int);
	void    get_remoteness(
		int                 fd,
		int                 &remoteness // OUT
	);

END_EXTERNCLIST

#endif /*__inet_stuff_h__*/
