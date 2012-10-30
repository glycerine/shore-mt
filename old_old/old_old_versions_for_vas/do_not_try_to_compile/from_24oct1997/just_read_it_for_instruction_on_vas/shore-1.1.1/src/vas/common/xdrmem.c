/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/xdrmem.c,v 1.20 1997/10/13 11:46:51 solomon Exp $
 */
#include <copyright.h>

#include <stdio.h>
#define MSG_XDR
#include <rpc/rpc.h>

#define __MSG_C__
#define __malloc_h
#include "msg.h"
#include "debug.h"
#include "xdrmem.h"

#if defined(HPUX8)
#	ifdef assert
#		undef assert
#	endif
#	define assert(x)
#else
#	include <assert.h>
#endif

void *memcpy();

typedef int	(xdrfunc)(XDR *, void *);

#define XDR_CHECKSUM 100

#ifdef DEBUG
u_short csum(
	const 	void    *v,
	int 	len
) 
{
	u_char 	c0=0, c1=0;
	u_char	*d;
	int		i;

	
	for(i=0, d = (u_char *)v; i<len; i++, d++) {
		c0 += *d;
		c0 %= 255;
		c1 += c0;
		c1 %= 255;
	}
	return (u_short)((c0<<8) | c1);
}
#else
static bool_t warned = 0;
#endif /*DEBUG*/

static int 
_mem_xdr( 
	void 		*transient, 
	void 		*disk, 
	xdr_kind		wht,
	xdr_direction	toHost,
	int			number
);

int mem2disk(
	const void 		*transient,
	void 		*disk,
	xdr_kind		wht
) {
	return _mem_xdr((void *)transient, disk, wht, transient2disk, 1);
}
int disk2mem(
	void 		*transient,
	const void 		*disk,
	xdr_kind		wht
) {
	return _mem_xdr(transient, (void *)disk, wht, disk2transient, 1);
}

int memarray2disk(
	const void 		*transient,
	void 		*disk,
	xdr_kind		wht,
	int			num
) {
	return _mem_xdr((void *)transient, disk, wht, transient2disk, num);
}
int disk2memarray(
	void 		*transient,
	const void 		*disk,
	xdr_kind		wht,
	int			num
) {
	return _mem_xdr(transient, (void *)disk, wht, disk2transient, num);
}
void 
cmp_checksum(
	const void 		*transient,
	const void 		*disk,
	smsize_t	    sz
) {
#ifdef DEBUG
	if( csum(transient,(int)sz) != csum(disk,(int)sz)) {
		fprintf(stderr, "Checksum of size %u failed!\n", sz);
		assert(0);
	}
#else
	if(!warned) {
		fprintf(stderr, 
		"Warning: vas common library compiled w/o -DDEBUG\n");
		warned = TRUE;
	}
#endif
}

/*
// TODO: Everywhere we have ASSUME, we have to 
// remove the assumption that the encoded form
// is the same size as the decoded form,
// or we need a way to check the assertion.
*/

int
_mem_xdr( 
	void 		*transient,
	void 		*disk,
	xdr_kind		wht,
	xdr_direction	toHost,
	int			number
)
{

	XDR 	temp;
	u_int	sz=0;
	enum xdr_op op = 
		(toHost==disk2transient)? XDR_DECODE : 
		(toHost==checksum)? XDR_CHECKSUM : XDR_ENCODE;
	xdrfunc	*func = 0;
 
	switch(wht) {
		case x_directory_value: 
			func = (xdrfunc *) xdr_directory_value;
			sz = sizeof(struct directory_value);
			break;

		case x_directory_body: 
			func = (xdrfunc *) xdr_directory_body;
			sz = sizeof(struct directory_body);
			break;

		case x_serial_t_list: 
			{
				char *t = transient;
				char *d = disk;
				int i;
				for(i =0; i<number; i++) {
					if( !_mem_xdr(t, d, x_serial_t, toHost, 0))
						return FALSE;

					t += sizeof(serial_t);
					d += sizeof(serial_t); /* ASSUME */
				}
				return TRUE;
			}
			break;

		case x_int:
			func = (xdrfunc *) xdr_int;
			sz = sizeof(int);
			break;

		case x_serial_t:
			/* one or two longwords */
			func = (xdrfunc *) xdr_serial_t;
			sz = sizeof(serial_t);
			break;

		case x_entry:
			func = (xdrfunc *) xdr__entry;
			sz = sizeof(struct _entry);
			break;

		case x_common_sysprops:
			func = (xdrfunc *) xdr__common_sysprops;
			sz = sizeof(_common_sysprops);
			break;

		case x_common_sysprops_withtext:
			func = (xdrfunc *) xdr__common_sysprops_withtext;
			sz = sizeof(_common_sysprops_withtext);
			break;

		case x_common_sysprops_withindex:
			func = (xdrfunc *) xdr__common_sysprops_withindex;
			sz = sizeof(_common_sysprops_withindex);
			break;

		case x_common_sysprops_withtextandindex:
			func = (xdrfunc *) xdr__common_sysprops_withtextandindex;
			sz = sizeof(_common_sysprops_withtextandindex);
			break;

		case x_anon_sysprops:
			func = (xdrfunc *) xdr__anon_sysprops;
			sz = sizeof(_anon_sysprops);
			break;

		case x_anon_sysprops_withtext:
			func = (xdrfunc *) xdr__anon_sysprops_withtext;
			sz = sizeof(_anon_sysprops_withtext);
			break;

		case x_anon_sysprops_withindex:
			func = (xdrfunc *) xdr__anon_sysprops_withindex;
			sz = sizeof(_anon_sysprops_withindex);
			break;

		case x_anon_sysprops_withtextandindex:
			func = (xdrfunc *) xdr__anon_sysprops_withtextandindex;
			sz = sizeof(_anon_sysprops_withtextandindex);
			break;

		case x_reg_sysprops:
			func = (xdrfunc *) xdr__reg_sysprops;
			sz = sizeof(_reg_sysprops);
			break;

		case x_reg_sysprops_withtext:
			func = (xdrfunc *) xdr__reg_sysprops_withtext;
			sz = sizeof(_reg_sysprops_withtext);
			break;

		case x_reg_sysprops_withindex:
			func = (xdrfunc *) xdr__reg_sysprops_withindex;
			sz = sizeof(_reg_sysprops_withindex);
			break;

		case x_reg_sysprops_withtextandindex:
			func = (xdrfunc *) xdr__reg_sysprops_withtextandindex;
			sz = sizeof(_reg_sysprops_withtextandindex);
			break;

		default:
			sz=0;/* keep compiler happy */
			func = (xdrfunc *) memcpy; /* keep compiler happy */
			fprintf(stderr, "FATAL ERROR: Unknown xdr_kind: %d\n", (int) wht);
			assert(0);
	}
#ifdef XDRPROBLEM
	switch(wht) {
		case x_common_sysprops:
		case x_common_sysprops_withtext:
		case x_common_sysprops_withindex:
		case x_common_sysprops_withtextandindex:
		case x_anon_sysprops:
		case x_reg_sysprops:
		case x_anon_sysprops_withtext:
		case x_reg_sysprops_withtext:
			func = (xdrfunc *) memcpy; /* ASSUME */
			break;
		default:
			break;
	}
#endif

	/* XDR_ENCODE: out to net == toDisk; transient->disk */
	/* XDR_DECODE: in from net == toHost; disk->transient */

	if(op==XDR_CHECKSUM) {
#ifdef DEBUG
		/*
		// true -> ok
		// false -> error
		// 
		// NB: this checksums the junk parts of a 
		// data structure too, but that should be 
		// a Good Thing in this context.
		*/
		return csum(transient,(int)sz) == csum(disk,(int)sz);
#else
		if(!warned) {
			fprintf(stderr, 
			"Warning: vas common library compiled w/o -DDEBUG\n");
			warned = TRUE;
		}
		return TRUE;
#endif
	} else if(func == (xdrfunc *)memcpy) {
		if(op == XDR_DECODE) {
			/* disk to memory */
			memcpy(transient, disk, sz);
		} else  {
			/* memory to disk */
			assert(op == XDR_ENCODE);
			memcpy(disk, transient, sz);
		}
		return TRUE;
	} else {
		xdrmem_create(&temp, (caddr_t)disk, sz, op);
		return (*func)(&temp, transient);
	}
}
