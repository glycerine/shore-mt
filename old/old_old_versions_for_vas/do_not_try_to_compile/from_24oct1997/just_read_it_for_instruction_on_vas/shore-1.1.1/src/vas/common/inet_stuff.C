/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/common/inet_stuff.C,v 1.10 1995/10/03 16:25:39 nhall Exp $
 */
#include <debug.h>
#include "errors.h"
#include <memory.h>
#include "inet_stuff.h"

BEGIN_EXTERNCLIST

#if !defined(HPUX8) && !defined(SOLARIS2)
	int	getsockname(int, struct sockaddr *, int *);
	int	getpeername(int, struct sockaddr *, int *);
#endif

#define _getsockname(a,b,c) getsockname((a), (struct sockaddr *)(b), (c))
#define _getpeername(a,b,c) getpeername((a), (struct sockaddr *)(b), (c))


#if !defined(Linux) && !defined(SOLARIS2) && !defined(HPUX8)
	int 	getsockopt(int,int,int,void*,int*);
	int 	setsockopt(int,int,int,void*,int);
#endif

	u_long	inet_addr(char *); // used locally only

END_EXTERNCLIST

int 	
get_sock_opt(int fd, int which)
{
#ifndef SOLARIS2
	int csz=0;
	int	szsz=sizeof(int);
	if(getsockopt(fd,SOL_SOCKET,which, &csz, &szsz) < 0) 
	{
		catastrophic("getsockopt");
	}
	DBG(<<"get: sockbuf size=" << csz);

	return csz;
#else
	return 50000; // TODO
#endif
}


void
set_sock_opt(int fd, int which)
{
#ifndef SOLARIS2    
	int e;
	int optval = 0x01010101;

	dassert(which == SO_KEEPALIVE || which == SO_REUSEADDR);

	e = setsockopt(fd,SOL_SOCKET,which, &optval, sizeof(optval));
	if(e<0) {
		catastrophic("setsockopt failed");
	}
#endif
}

int 	
max_sock_opt(int fd, int which, int max /* = -1 */) 
{
#ifndef SOLARIS2
	// Grot:
	// If sysconf told us the system-imposed max,
	// we wouldn't have to go through all this
	// rigmarole to maximize the size of the buffer
	//
	if(max==-1) max = 52428;

	// make the socket buffer as large as you can, up to the given max
	// return the smaller of the {current-size,max}

	// first get current size
        int csz=0;
	int	szsz=sizeof(int);

	if(getsockopt(fd,SOL_SOCKET,which, &csz, &szsz) < 0)
	{

		catastrophic("getsockopt");
	}

	DBG(<<"max: sockbuf size=" << csz);

	// if already that big, return the max
	if(csz >= max) return max;

	// try to make it that big right away
	int 	sz=max;
	int		hibad=max; // higood is csz
	int		diff;	// temporary value
	int 	e=0;

	while(1) {
		DBG(<<"cur size = " << csz << "; try sockbuf size=" << sz);
		e = setsockopt(fd,SOL_SOCKET,which, &sz, sizeof(int));
		if(e==0) {
			DBG(<<"worked--go up");
			// worked -- try to go bigger
			csz = sz; // highest good value

		} else {
			// failed -- try to go smaller
			DBG(<<"failed--go down");
			hibad = sz;
		}

		// set sz (next value to try) halfway
		// between the two: hibad, higood
		sz = hibad - csz;
		// quit trying if the difference is too small
		if(sz < 2) {
			return csz;
		}
		if(sz & 0x1) sz++;
		sz = sz/2;
		sz += csz;
	}

#	ifdef DEBUG
	int sz2;

	if(getsockopt(fd,SOL_SOCKET,which, &sz2, &szsz) < 0) {
		catastrophic("getsockopt");
	}
	if(sz2 != csz) {
		catastrophic("socket buffer sizes");
	}
#	endif
	DBG(<<"sockbuf size=" << csz);
	return csz;
#else
	// TODO: fix
	return 50000;
#endif
}
#ifdef SOLARIS2
#include <tiuser.h>
#include <rpc/types.h>
#include <rpc/rpc.h>
#include <rpc/svc.h>
extern SVCXPRT **svc_xports;
extern int __rpc_dtbsize();
#endif

void
get_addresses( 
	int	fd,
	struct sockaddr_in &me, 
	struct sockaddr_in &peer
)
{
	int sz = sizeof(me);
	memset(&me, '\0', sz);
	memset(&peer, '\0', sz);

#ifndef SOLARIS2

	if(_getsockname(fd, &me, &sz) < 0) {
		catastrophic("getsockname");
	}
	sz = sizeof(me);
	if(_getpeername(fd, &peer, &sz) < 0) {
		// not an error
		// perror("getpeername");
	}
#else
	struct t_info	t;
	SVCXPRT			*svcxprt;

	if (t_getinfo(fd, &t) == -1) {
		catastrophic("t_getinfo");
	}
	svcxprt = svc_xports[fd];
	dassert(svcxprt->xp_fd == fd);

	struct          sockaddr_in *sinp;

	/*
	if(svcxprt->xp_ltaddr.len > 0) {
		sinp = 	(struct sockaddr_in *)(svcxprt->xp_ltaddr.buf);
		if(sinp) me = *sinp;
	}
	*/

	// grot: local addr isn't set, so we'll
	// have to put in a hack
	if(me.sin_addr.s_addr == INADDR_ANY) {
		me.sin_addr.s_addr = INADDR_LOOPBACK;
	}

	if(svcxprt->xp_rtaddr.len > 0) {
		sinp = 	(struct sockaddr_in *)(svcxprt->xp_rtaddr.buf);
		if(sinp) peer = *sinp;
	}

#endif
}

/*
// valid only for server side, since it uses
// svc_xports[].  An implementation for the 
// client side would probably use rpcb_getaddr
*/
void
get_remoteness( 
	int 				fd,
	int					&remoteness // OUT
) 
{
	struct          sockaddr_in me, peer;

	get_addresses(fd, me, peer);

	if(me.sin_addr.s_addr != peer.sin_addr.s_addr) {
        DBG( << "Remote: me.sin_addr "
            << ::hex((unsigned long) me.sin_addr.s_addr)
            << " peer: "
            << ::hex((unsigned long) peer.sin_addr.s_addr)
        );
		dumpaddrs(fd, "get_remoteness");
        remoteness = other_machine;
    } else {
        remoteness = same_machine;
    }
}

void
dumpaddrs(int sock, char *str)
{   
	FUNC(dumpaddrs);
	struct sockaddr_in me;
	struct sockaddr_in peer;

	get_addresses(sock, me, peer);

	DBG( "DUMP ADDRS FROM " << str);

	DBG(
		<< form("me: family 0x%x port 0x%x in_addr 0x%x; ", 
			me.sin_family, 
			me.sin_port,
			me.sin_addr.s_addr) 
	)
	DBG(
		<< form("me : %s.%d\n", inet_ntoa(me.sin_addr),
			ntohs(me.sin_port))
	)
	DBG(
		<< form("peer: family 0x%x port 0x%x in_addr 0x%x; ", 
			peer.sin_family, 
			peer.sin_port,
			peer.sin_addr.s_addr) 
	)
	DBG(
		<< form("peer : %s.%d\n", inet_ntoa(peer.sin_addr),
			ntohs(peer.sin_port))
	)
}
