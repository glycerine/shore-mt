/* @(#)svc_udp.c	2.2 88/07/29 4.0 RPCSRC */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#if !defined(lint) && defined(SCCSIDS)
static char sccsid[] = "@(#)svc_udp.c 1.24 87/08/11 Copyr 1984 Sun Micro";
#endif

/*
 * svc_udp.c,
 * Server side for UDP/IP based RPC.  (Does some caching in the hopes of
 * achieving execute-at-most-once semantics.)
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <rpc/rpc.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include "svc_stats.h"

#ifdef PURIFY
#include <purify.h>
#endif

#define rpc_buffer(xprt) ((xprt)->xp_p1)
#ifndef MAX
#define MAX(a, b)     ((a > b) ? a : b)
#endif

static bool_t		svcudp_recv();
static bool_t		svcudp_reply();
static enum xprt_stat	svcudp_stat();
static bool_t		svcudp_getargs();
static bool_t		svcudp_freeargs();
static void			svcudp_destroy();
static void			svcudp_disablecache();

static struct xp_ops svcudp_op = {
	svcudp_recv,
	svcudp_stat,
	svcudp_getargs,
	svcudp_reply,
	svcudp_freeargs,
	svcudp_destroy
};

extern int errno;
#ifdef DEBUG
static int debug=0;
static int debug1=0;
static int debugm=0;
#endif

/*
 * kept in xprt->xp_p2
 */
struct svcudp_data {
	u_int   su_iosz;	/* byte size of send.recv buffer */
	u_long	su_xid;		/* transaction id */
	XDR	su_xdrs;	/* XDR handle */
	char	su_verfbody[MAX_AUTH_BYTES];	/* verifier body */
	char * 	su_cache;	/* cached data, NULL if no cache */
};
#define	su_data(xprt)	((struct svcudp_data *)(xprt->xp_p2))

#include "svc_udp_cache.h"

/*
 * Usage:
 *	xprt = svcudp_create(sock);
 *
 * If sock<0 then a socket is created, else sock is used.
 * If the socket, sock is not bound to a port then svcudp_create
 * binds it to an arbitrary port.  In any (successful) case,
 * xprt->xp_sock is the registered socket number and xprt->xp_port is the
 * associated port number.
 * Once *xprt is initialized, it is registered as a transporter;
 * see (svc.h, xprt_register).
 * The routines returns NULL if a problem occurred.
 */
SVCXPRT *
svcudp_bufcreate(sock, sendsz, recvsz)
	register int sock;
	u_int sendsz, recvsz;
{
	bool_t madesock = FALSE;
	register SVCXPRT *xprt;
	register struct svcudp_data *su;
	struct sockaddr_in addr;
	int len = sizeof(struct sockaddr_in);

#ifdef DEBUG
	if(debugm) {
		fprintf(stderr, "svcudp_bufcreate sock=%d %d\n",sock, __LINE__);
	}
#endif

	if (sock == RPC_ANYSOCK) {
		if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
			perror("svcudp_create: socket creation problem");
			return ((SVCXPRT *)NULL);
		}
		madesock = TRUE;
	}
	bzero((char *)&addr, sizeof (addr));
	addr.sin_family = AF_INET;
	if (bindresvport(sock, &addr)) {
		addr.sin_port = 0;
		(void)bind(sock, (struct sockaddr *)&addr, len);
	}
	if (getsockname(sock, (struct sockaddr *)&addr, &len) != 0) {
		perror("svcudp_create - cannot getsockname");
		if (madesock)
			(void)close(sock);
		return ((SVCXPRT *)NULL);
	}
	xprt = (SVCXPRT *)mem_alloc(sizeof(SVCXPRT));
	if (xprt == NULL) {
		(void)fprintf(stderr, "svcudp_create: out of memory\n");
		return (NULL);
	}
	su = (struct svcudp_data *)mem_alloc(sizeof(*su));
	if (su == NULL) {
		(void)fprintf(stderr, "svcudp_create: out of memory\n");
		return (NULL);
	}
	su->su_xid = 0; /* else umr */
	su->su_iosz = ((MAX(sendsz, recvsz) + 3) / 4) * 4;
	if ((rpc_buffer(xprt) = (caddr_t) mem_alloc(su->su_iosz)) == NULL) {
		(void)fprintf(stderr, "svcudp_create: out of memory\n");
		assert(0);
		return (NULL);
	}
	xdrmem_create(
	    &(su->su_xdrs), rpc_buffer(xprt), su->su_iosz, XDR_DECODE);
	su->su_cache = NULL;
	xprt->xp_p2 = (caddr_t)su;
	xprt->xp_verf.oa_base = su->su_verfbody;
	xprt->xp_ops = &svcudp_op;
	xprt->xp_port = ntohs(addr.sin_port);
	xprt->xp_sock = sock;
	xprt_register(xprt);
	return (xprt);
}

SVCXPRT *
svcudp_create(sock)
	int sock;
{

	return(svcudp_bufcreate(sock, UDPMSGSIZE, UDPMSGSIZE));
}

static enum xprt_stat
svcudp_stat(xprt)
	SVCXPRT *xprt;
{

	return (XPRT_IDLE); 
}



#ifdef DEBUG
extern void memdump();
#endif

static bool_t
svcudp_recv(xprt, msg)
	register SVCXPRT *xprt;
	struct rpc_msg *msg;
{
	register struct svcudp_data *su = su_data(xprt);
	register XDR *xdrs = &(su->su_xdrs);
	register int rlen;
	char *reply;
	u_long replylen;
	int 	is_retrans =0;


#ifdef DEBUG
	if(debug) {
		fprintf(stderr, "svcudp_recv %d\n" , __LINE__);
	}
#endif

    again:
	xprt->xp_addrlen = sizeof(struct sockaddr_in);

	assert(rpc_buffer(xprt) != 0);

#ifdef DEBUG
	if(debug) {
	fprintf(stderr, 
		"svcudp_recv called recvfrom(%d,0x%x, %d,%d,0x%x,0x%x)\n",
		xprt->xp_sock, rpc_buffer(xprt), (int) su->su_iosz,
	    0, (struct sockaddr *)&(xprt->xp_raddr), &(xprt->xp_addrlen));
	}
#endif
#ifdef PURIFY
	/*
	// purify doesn't seem to recognize
	// read() system call as an initializer
	*/
	if(purify_is_running()) {
		bzero(rpc_buffer(xprt), (int)su->su_iosz);
	}
#endif
	svc_stats()->udp.recvfroms++;

	rlen = recvfrom(xprt->xp_sock, rpc_buffer(xprt), (int) su->su_iosz,
	    0, (struct sockaddr *)&(xprt->xp_raddr), &(xprt->xp_addrlen));

	{	/* keep track of largest request */ 	
		if (svc_stats()->udp.reqmax < rlen) 
			svc_stats()->udp.reqmax = rlen;
	}

#ifdef DEBUG
	if(debug) {
		fprintf(stderr, "recvfrom got rlen %d at %d\n",
			rlen,__LINE__);
		memdump(rpc_buffer(xprt), rlen, "UDP-READ");
	}
#endif

	if (rlen == -1 && errno == EINTR) {
#ifdef DEBUG
	if(debug) {
		fprintf(stderr, "EINTR svcudp_recv %d\n" , __LINE__);
	}
#endif
		goto again;
	}
	if (rlen == -1 == EINTR) {
		perror("recvfrom");
		exit(1);
	}
	if (rlen < 4*sizeof(u_long)){
#ifdef DEBUG
	if(debug) {
		fprintf(stderr, "svcudp_recv returned DROP rlen %d at %d\n",
			rlen,__LINE__);
	}
#endif
		return (FALSE);
	}
	xdrs->x_op = XDR_DECODE;
	XDR_SETPOS(xdrs, 0);
	if (! xdr_callmsg(xdrs, msg)) {

#ifdef DEBUG
	if(debug) {
		fprintf(stderr, "svcudp_recv returned DROP at %d\n" , __LINE__);
	}
#endif
		return (FALSE);
	}

	xprt->xp_xid = su->su_xid = msg->rm_xid;
	if (su->su_cache != NULL) {
		switch (cache_get(xprt, msg, &reply, &replylen)) {
		case SENDREPLY:
			svc_stats()->udp.rereplies++;
			/* retrans with cached reply */

#ifdef DEBUG
	if(debug) {
			fprintf(stderr, "cached (id=%d); sendto(%d,,%d,0,%s)\n",
				msg->rm_xid,
				xprt->xp_sock, replylen,
				inet_ntoa(
				((struct sockaddr_in *)(&xprt->xp_raddr))->sin_addr));
	}
#endif

			{	/* keep track of largest reply */ 	
				if (svc_stats()->udp.replymax < replylen) 
					svc_stats()->udp.replymax = replylen;
			}

			(void) sendto(xprt->xp_sock, reply, (int) replylen, 0,
			  (struct sockaddr *) &xprt->xp_raddr, xprt->xp_addrlen);
#			ifdef DEBUG
			if(debug) {
				fprintf(stderr, "svcudp_recv returned DROP at %d\n" , __LINE__);
			}
#			endif
			return FALSE;

		case DROPIT:
			svc_stats()->udp.drops++;
#			ifdef DEBUG
			if(debug) {
				fprintf(stderr, "svcudp_recv returned DROP at %d\n" , __LINE__);
			}
#			endif
			return FALSE;

		case DOIT:
			svc_stats()->udp.done++;
#			ifdef DEBUG
			if(debug) {
				fprintf(stderr, "svcudp_recv returned DONE at %d\n" , __LINE__);
			}
#			endif
			return TRUE;
		}
	}
#ifdef DEBUG
	if(debug) {
	fprintf(stderr, "svcudp_recv returned DONE at %d\n" , __LINE__);
	}
#endif

	/* same as DOIT */
	return (TRUE);
}

static bool_t
svcudp_reply(xprt, msg)
	register SVCXPRT *xprt; 
	struct rpc_msg *msg; 
{
	register struct svcudp_data *su = su_data(xprt);
	register XDR *xdrs = &(su->su_xdrs);
	register int slen;
	register bool_t stat = FALSE;

	xdrs->x_op = XDR_ENCODE;
	XDR_SETPOS(xdrs, 0);
	msg->rm_xid = su->su_xid;
	if (xdr_replymsg(xdrs, msg)) {
		slen = (int)XDR_GETPOS(xdrs);
		svc_stats()->udp.replies++;

		if (sendto(xprt->xp_sock, rpc_buffer(xprt), slen, 0,
		    (struct sockaddr *)&(xprt->xp_raddr), xprt->xp_addrlen)
		    == slen) {
			stat = TRUE;
			if (su->su_cache && slen >= 0) {
				cache_set(xprt, msg, (u_long) slen);
			}
		}
	}
	return (stat);
}

static bool_t
svcudp_getargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{

	return ((*xdr_args)(&(su_data(xprt)->su_xdrs), args_ptr));
}

static bool_t
svcudp_freeargs(xprt, xdr_args, args_ptr)
	SVCXPRT *xprt;
	xdrproc_t xdr_args;
	caddr_t args_ptr;
{
	register XDR *xdrs = &(su_data(xprt)->su_xdrs);

	xdrs->x_op = XDR_FREE;
	return ((*xdr_args)(xdrs, args_ptr));
}

static void
svcudp_destroy(xprt)
	register SVCXPRT *xprt;
{
	register struct svcudp_data *su = su_data(xprt);
#ifdef DEBUG
	if(debugm) {
		fprintf(stderr, "svcudp_destroy %d\n" , __LINE__);
	}
#endif

	xprt_unregister(xprt);
	(void)close(xprt->xp_sock);
	XDR_DESTROY(&(su->su_xdrs));
	mem_free(rpc_buffer(xprt), su->su_iosz);

#ifdef DEBUG
	if(debugm) {
		fprintf(stderr, "svcudp_destroy %d\n" , __LINE__);
	}
#endif
	if (su->su_cache != NULL) {
		svcudp_disablecache(&(su->su_cache));
	}
#ifdef DEBUG
	if(debugm) {
		fprintf(stderr, "svcudp_destroy %d\n" , __LINE__);
	}
#endif
	mem_free((caddr_t)su, sizeof(struct svcudp_data));
	mem_free((caddr_t)xprt, sizeof(SVCXPRT));
}


/***********this could be a separate file*********************/

/*
 * Fifo cache for udp server
 * Copies pointers to reply buffers into fifo cache
 * Buffers are sent again if retransmissions are detected.
 */

#define SPARSENESS 4	/* 75% sparse */

#define CACHE_PERROR(msg)	\
	(void) fprintf(stderr,"%s\n", msg)

#ifdef DEBUG
void *
zalloc(sz, line, file) 
	unsigned int 	sz;
	int 			line;
	char			*file;
{
	void *z = malloc(sz);
	if(debugm) {
		fprintf(stderr, 
		"%8x size %10d mem_alloc at line  %4d file %s\n",
		z, sz,  line, file);
	}
	bzero(z, sz);
	return z;
}
void 
zfree(ptr, sz, line, file) 
	void			*ptr;
	unsigned int 	sz;
	int 			line;
	char			*file;
{
	if(debugm) {
		fprintf(stderr, 
		"%8x size %10d mem_free  at line  %4d file %s\n",
		ptr, sz, line, file);
	}
	fflush(stderr);
	free(ptr);
}
#define ALLOC(type, size)	\
	(type *)zalloc((sizeof(type) * (size)), __LINE__, __FILE__);
#else
#define ALLOC(type, size)	\
	(type *) mem_alloc((unsigned) (sizeof(type) * (size)))
#endif

#define BZERO(addr, type, size)	 \
	bzero((char *) addr, sizeof(type) * (int) (size)) 



/*
 * the hashing function
 */
#define CACHE_LOC(transp, xid)	\
 (xid % (SPARSENESS*((struct udp_cache *) su_data(transp)->su_cache)->uc_size))	
/* 
 * the # entries in the table (for cleaning up)
 */
#define CACHE_MAX(uc)	\
 ((int)(SPARSENESS*(uc)->uc_size))


/*
 * Enable use of the cache. 
 */
svcudp_enablecache(transp, size)
	SVCXPRT *transp;
	u_long size;
{
	struct svcudp_data *su = su_data(transp);
	struct udp_cache *uc;

	if (su->su_cache != NULL) {
		CACHE_PERROR("enablecache: cache already enabled");
		return(0);	
	}
	uc = ALLOC(struct udp_cache, 1);
#ifdef DEBUG
	if(debugm) {
		fprintf(stderr, 
		"svcudp_enablecache %x size=%d at line  %d\n",
			uc, size, __LINE__);
	}
#endif
	if (uc == NULL) {
		CACHE_PERROR("enablecache: could not allocate cache");
		return(0);
	}
	uc->uc_size = size;
	uc->uc_nextvictim = 0;
	uc->uc_entries = ALLOC(cache_ptr, size * SPARSENESS);
	if (uc->uc_entries == NULL) {
		CACHE_PERROR("enablecache: could not allocate cache data");
		return(0);
	}
	BZERO(uc->uc_entries, cache_ptr, size * SPARSENESS);
	uc->uc_fifo = ALLOC(cache_ptr, size);
	if (uc->uc_fifo == NULL) {
		CACHE_PERROR("enablecache: could not allocate cache fifo");
		return(0);
	}
	BZERO(uc->uc_fifo, cache_ptr, size);
	su->su_cache = (char *) uc;
	return(1);
}

static void
svcudp_disablecache(_uc)
	struct udp_cache **_uc;
{
	struct udp_cache *uc =  *_uc;
	int	size = uc->uc_size;
	int i,j;
	register cache_ptr victim,v;	

	if (uc->uc_entries != NULL) {
		j = CACHE_MAX(uc);
#ifdef DEBUG
	if(debugm) {
		fprintf(stderr, 
		"svcudp_disablecache %x size=%d j=%d at line  %d\n",
			uc, size,
			j,  __LINE__);
	}
#endif
		for(i=0; i<j; i++) {
			victim = uc->uc_entries[i];
			while(victim) {
				v = victim->cache_next;
				if(victim->cache_reply) {
					mem_free(victim->cache_reply,1/*2nd arg ignored*/);
				}
				mem_free(victim,1/*2nd arg ignored*/);  
				victim = v;
			}
		}
		mem_free(uc->uc_entries, (size * SPARSENESS) * sizeof(cache_ptr));
		uc->uc_entries = NULL;
	}
	if (uc->uc_fifo != NULL) {
		mem_free(uc->uc_fifo, size * sizeof(cache_ptr));
		uc->uc_fifo = NULL;
	}
	mem_free(uc, sizeof(struct udp_cache) * 1);
	*_uc = NULL;
}


/*
 * Set an entry in the cache
 */
static void
cache_set(xprt, msg, replylen)
	SVCXPRT *xprt;
	struct rpc_msg *msg; 
	u_long replylen;	
{
	register cache_ptr victim;	
	register cache_ptr *vicp;
	register struct svcudp_data *su = su_data(xprt);
	struct udp_cache *uc = (struct udp_cache *) su->su_cache;
	u_int loc;

#	ifdef DEBUG
	if(debug1) {
			fprintf(stderr, "cache_set %x " , xprt);
	}
#	endif
	svc_stats()->udp.cache_sets++;

	assert((msg->rm_direction == REPLY));
	victim = cache_find(xprt, 
			msg->rm_xid, 
			uc->uc_proc,
			uc->uc_vers,
			uc->uc_prog,
			&xprt->xp_raddr);

	if(victim == NULL) {
		victim = cache_victim(xprt, uc);
		if(victim == NULL) {
			return;
		}
	}

	switch(victim->cache_status) {
		default:
			/* dassert that other values already match */

			return; /* don't cache it again! */
			break;

		case CS_NEW:
			assert(victim->cache_xid == 0);
			victim->cache_xid = su->su_xid;
			victim->cache_proc = uc->uc_proc;
			victim->cache_vers = uc->uc_vers;
			victim->cache_prog = uc->uc_prog;
			victim->cache_addr = uc->uc_addr;

			/* insert it in the cache */
			loc = CACHE_LOC(xprt, victim->cache_xid);
			victim->cache_next = uc->uc_entries[loc];	
			uc->uc_entries[loc] = victim;
			uc->uc_fifo[uc->uc_nextvictim++] = victim;
			uc->uc_nextvictim %= uc->uc_size;

			/* drop down */

		case CS_INPROGRESS:
			/*
			 * Store the reply
			 */
			victim->cache_replylen = replylen;
			{
				/* switch the cached reply buf for the reply */
				char *buf= victim->cache_reply;
				victim->cache_reply = rpc_buffer(xprt);
				rpc_buffer(xprt) = buf;
			}

			xdrmem_create(&(su->su_xdrs), 
				rpc_buffer(xprt), su->su_iosz, XDR_ENCODE);

			/* dassert that other values already match */
			assert(victim->cache_xid == su->su_xid);
			assert(victim->cache_proc == uc->uc_proc);
			assert(victim->cache_vers == uc->uc_vers);
			assert(victim->cache_prog == uc->uc_prog);

			break;
	}

	victim->found_count = 0;
	victim->cache_status = CS_FIRSTREPLY;

}

cache_ptr 
cache_victim(xprt, uc)
	SVCXPRT *xprt;
	struct udp_cache *uc;
{
	register cache_ptr victim;	
	register cache_ptr *vicp;
	register struct svcudp_data *su = su_data(xprt);
	char *newbuf=0;
	u_int loc;

#ifdef DEBUG
	if(debug1) {
		fprintf(stderr, "cache_vctim: %d\n", su->su_xid);
	}
#endif
	{
		/* old way */
		/*
		 * Find space for the new entry, either by
		 * reusing an old entry, or by mallocing a new one
		 */
		victim = uc->uc_fifo[uc->uc_nextvictim];
		if (victim != NULL) {
			loc = CACHE_LOC(xprt, victim->cache_xid);
			for (vicp = &uc->uc_entries[loc]; *vicp != NULL && *vicp != victim; 
			  vicp = &(*vicp)->cache_next)
			  ;
			if (*vicp == NULL) {
				CACHE_PERROR("cache_set: victim not found");
				return NULL; 
			}
			*vicp = victim->cache_next;	/* remove from cache */
			newbuf = victim->cache_reply;
		} 
	}
	if(victim == NULL) {
		victim = ALLOC(struct cache_node, 1);
		if (victim == NULL) {
			CACHE_PERROR("cache_set: victim alloc failed");
			return NULL;
		}
		newbuf = (caddr_t) mem_alloc(su->su_iosz);
		if (newbuf == NULL) {
			CACHE_PERROR("cache_set: could not allocate new rpc_buffer");
			assert(0);
			return NULL;
		}
	}
	victim->cache_reply = newbuf; /* old buf or malloced buf */
	victim->cache_xid = 0;
	victim->cache_status = CS_NEW;

	return victim;
}

static int
cache_preset(xprt, msg, uc)
	SVCXPRT *xprt;
	struct rpc_msg *msg; 
	struct udp_cache *uc;
{
	u_int loc;
	register cache_ptr victim;	

	svc_stats()->udp.cache_presets++;
#	ifdef DEBUG
	if(debug1) {
			fprintf(stderr, "cache_preset %x " , xprt);
	}

	assert((msg->rm_direction == CALL));

	victim = cache_find(xprt, 
			msg->rm_xid, 
			uc->uc_proc,
			uc->uc_vers,
			uc->uc_prog,
			xprt->xp_raddr);

	if(victim) {
		return 0;
	}
	assert(victim==0);
#	endif /*DEBUG*/

	victim = cache_victim(xprt,uc);
	if(!victim) {
		return;
	}

#ifdef DEBUG
	if(debug) {
		fprintf(stderr, "cache_preset: %d victim at 0x%x\n", 
			msg->rm_xid, victim);
	}
#endif /*DEBUG*/

	assert((victim->cache_reply != 0) );

	/* have no reply to cache */
	victim->cache_xid = msg->rm_xid;
	victim->cache_proc = uc->uc_proc;
	victim->cache_vers = uc->uc_vers;
	victim->cache_prog = uc->uc_prog;
	victim->cache_addr = uc->uc_addr;

	victim->found_count = 0;
	victim->cache_status = CS_INPROGRESS; 

	loc = CACHE_LOC(xprt, victim->cache_xid);
	victim->cache_next = uc->uc_entries[loc];	
	uc->uc_entries[loc] = victim;
	uc->uc_fifo[uc->uc_nextvictim++] = victim;
	uc->uc_nextvictim %= uc->uc_size;
	return 1;
}

/*
 * Try to get an entry from the cache
 * return 1 if found, 0 if not found
 */

static cache_ptr 
cache_find(xprt, xid, proc, vers, prog, addr)
	SVCXPRT *xprt;
	u_long xid;
	u_long proc;
	u_long vers;
	u_long prog;
	struct sockaddr_in *addr;
{
	u_int loc;
	register struct svcudp_data *su = su_data(xprt);
	struct udp_cache *uc = (struct udp_cache *) su->su_cache;
	register cache_ptr ent;

#	define EQADDR(a1, a2)	(bcmp((char*)&a1, (char*)&a2, sizeof(a1)) == 0)

#	ifdef DEBUG
	if(debug1) {
		fprintf(stderr, "cache_find(%d %d %s)\n", xid, proc,
			inet_ntoa(addr->sin_addr));
	}
#	endif

	loc = CACHE_LOC(xprt, xid);
	for (ent = uc->uc_entries[loc]; ent != NULL; ent = ent->cache_next) {
		if (ent->cache_xid == xid &&
		  ent->cache_proc == proc &&
		  ent->cache_vers == vers &&
		  ent->cache_prog == prog &&
		  EQADDR(ent->cache_addr, *addr)) {

#			ifdef DEBUG
			if(debug1) {
				fprintf(stderr, "cache_find: %d found at 0x%x\n", 
					ent->cache_xid, ent);
			}
#			endif

			/* ent->found_count++; gets incremented by caller */
			svc_stats()->udp.cache_finds++;
			return ent;
		}
	}
	svc_stats()->udp.cache_nofinds++;
	return(0);
}

static int
cache_get(xprt, msg, replyp, replylenp)
	SVCXPRT *xprt;
	struct rpc_msg *msg;
	char **replyp;
	u_long *replylenp;
{
	u_int loc;
	register cache_ptr ent;
	register struct svcudp_data *su = su_data(xprt);
	register struct udp_cache *uc = (struct udp_cache *) su->su_cache;

#	ifdef DEBUG
	if(debug1) {
			fprintf(stderr, "cache_get %d ", msg->rm_xid);
	}
#	endif
	svc_stats()->udp.cache_gets++;

	/* su_xid was set before this call */

	assert(msg->rm_direction == CALL);
	ent = cache_find(xprt, msg->rm_xid, msg->ru.RM_cmb.cb_proc,
			msg->ru.RM_cmb.cb_vers, msg->ru.RM_cmb.cb_prog, xprt->xp_raddr);

	if(ent != NULL) {
		svc_stats()->udp.cache_hits++;
		*replyp = ent->cache_reply;
		*replylenp = ent->cache_replylen;

		if(ent->cache_status == CS_INPROGRESS) {
			/* really need to see how long it's been */
#			ifdef DEBUG
			if(debug) {
					fprintf(stderr, "DROPIT cache_get %d\n" , __LINE__);
			}
#			endif
			return DROPIT;

		} else {

#			ifdef DEBUG
			if(debug) {
					fprintf(stderr, "SENDREPLY cache_get %d\n" , __LINE__);
			}
#			endif
			ent->cache_status = CS_REXREPLY;

			svc_stats()->udp.retrans_total++;
			if(ent->found_count++ == 0) {
				/*
				// prevents us from counting this particular
				// retransmission more than once
				*/
				svc_stats()->udp.retrans++;
				if(ent->found_count > svc_stats()->udp.retrans_max) {
					svc_stats()->udp.retrans_max = ent->found_count;
				}
			}

			return SENDREPLY;
		}
	}
	svc_stats()->udp.cache_misses++;
	assert(ent==NULL);
	/*
	 * Failed to find entry
	 * Remember a few things so we can do a set later
	 * NB: THIS IS NOT THREAD-SAFE-- if we can recieve another
	 * request before reply goes out, we're hosed!
	 * (This *shouldn't* happen because we have one 
	 * of these caches for each fd on which we listen, and 
	 * exactly one thread listening on each fd.)
	 */
	uc->uc_proc = msg->rm_call.cb_proc;
	uc->uc_vers = msg->rm_call.cb_vers;
	uc->uc_prog = msg->rm_call.cb_prog;
	uc->uc_addr = xprt->xp_raddr;

	if( cache_preset(xprt, msg, uc) ) {

#ifdef DEBUG
		if(debug1) {
			fprintf(stderr, "cache_get for xid %d stashed %d %s\n",
				msg->rm_xid, uc->uc_proc, inet_ntoa(uc->uc_addr.sin_addr));
		}
		if(debug) {
			fprintf(stderr, "DO IT cache_get %d\n" , __LINE__);
		}
#endif
		return DOIT;
	} else {
		return DROPIT;
	}
}

