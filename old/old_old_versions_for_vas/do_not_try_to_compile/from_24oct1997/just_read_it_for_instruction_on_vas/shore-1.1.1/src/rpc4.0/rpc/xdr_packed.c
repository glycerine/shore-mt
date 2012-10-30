/* @(#)xdr.c	2.1 88/07/29 4.0 RPCSRC */
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
static char sccsid[] = "@(#)xdr.c 1.35 87/08/12";
#endif

/*
 * xdr_packed.c, Generic XDR routines implementation for
 * packed data types
 *
 * Copyright (C) 1986, Sun Microsystems, Inc.
 *
 * These are the "generic" xdr routines used to serialize and de-serialize
 * most common data items.  See xdr.h for more info on the interface to
 * xdr.
 */

#include <stdio.h>

#include <rpc/types.h>
#include <rpc/xdr.h>

/*
 * constants specific to the xdr "protocol"
 */
#define XDR_FALSE	((long) 0)
#define XDR_TRUE	((long) 1)
#define LASTUNSIGNED	((u_int) 0-1)

/*
 * XDR packed short integers
 */
bool_t
xdr_short_pk(xdrs, usp)
	register XDR *xdrs;
	short *usp;
{
	short l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (short) *usp;
		return (XDR_PUTBYTES(xdrs, &l, sizeof(l)));

	case XDR_DECODE:
		if(!XDR_GETBYTES(xdrs, &l, sizeof(l))) {
			return (FALSE);
		}
		*usp = (short) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR packed unsigned short integers
 */
bool_t
xdr_u_short_pk(xdrs, usp)
	register XDR *xdrs;
	u_short *usp;
{
	u_short l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (u_short) *usp;
		return (XDR_PUTBYTES(xdrs, &l, sizeof(l)));

	case XDR_DECODE:
		if(!XDR_GETBYTES(xdrs, &l, sizeof(l))) {
			return (FALSE);
		}
		*usp = (u_short) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}


/*
 * XDR a packed char
 */
bool_t
xdr_char_pk(xdrs, cp)
	XDR *xdrs;
	char *cp;
{
	char l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (char) *cp;
		return (XDR_PUTBYTES(xdrs, &l, sizeof(l)));

	case XDR_DECODE:
		if(!XDR_GETBYTES(xdrs, &l, sizeof(l))) {
			return (FALSE);
		}
		*cp = (char) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR a packed unsigned char
 */
bool_t
xdr_u_char_pk(xdrs, cp)
	XDR *xdrs;
	u_char *cp;
{
	u_char l;

	switch (xdrs->x_op) {

	case XDR_ENCODE:
		l = (u_char) *cp;
		return (XDR_PUTBYTES(xdrs, &l, sizeof(l)));

	case XDR_DECODE:
		if(!XDR_GETBYTES(xdrs, &l, sizeof(l))) {
			return (FALSE);
		}
		*cp = (u_char) l;
		return (TRUE);

	case XDR_FREE:
		return (TRUE);
	}
	return (FALSE);
}

/*
 * XDR booleans
 */
bool_t
xdr_bool_pk(xdrs, bp)
	register XDR *xdrs;
	bool_t *bp;
{
	bool_t b;

	return (xdr_char_pk(xdrs, (char *)bp));
}

