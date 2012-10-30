/* @(#)types.h	2.3 88/08/15 4.0 RPCSRC */
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
/*      @(#)types.h 1.18 87/07/24 SMI      */

/*
 * Rpc additions to <sys/types.h>
 */
#ifndef __TYPES_RPC_HEADER__
#define __TYPES_RPC_HEADER__

#if defined(__cplusplus) && defined(__GNUC__) && ((__GNUC__>=2)&&(__GNUC_MINOR__>=6))
typedef bool bool_t;
/* a one-byte thing, unfortunately.
 * This means that for the C++ users,
 * we need to generate xdr code to convert from
 * a one-byte Boolean to an int over the wire,
 * *and* we need to figure out how to cope with
 * the size differences so that xdr will work.
 */
#else

/* xdr package treats bool_t as an int.
 * bools are always sent on the wire as unsigned
 * ints
 */
typedef int bool_t;
typedef char bool; /* DO NOT USE IN ANY OF THIS CODE-- this is for
	C code generated by rpcgen, that's compiled with gcc */

#endif

typedef int enum_t;

/*
// #define	bool_t	int
// #define	enum_t	int
*/

#ifndef FALSE
#define	FALSE	(0)
#endif
#ifndef TRUE
#define	TRUE	(1)
#endif
#define __dontcare__	-1
#ifndef NULL
#	define NULL 0
#endif

#include <sys/types.h>
#if defined(_cplusplus) 
/* && !defined(hpux) && !defined(Mips) */
#include <sys/stdtypes.h>
#endif
#include <stdlib.h>

#ifdef DEBUG
void *zalloc();
void zfree();
#define mem_alloc(bsize)	zalloc(bsize,__LINE__,__FILE__)
#ifdef calloc
#undef calloc
#endif
#define calloc(b,sz)	zalloc(b*sz,__LINE__,__FILE__)
#define mem_free(ptr,bsize)		zfree(ptr,bsize,__LINE__,__FILE__)
#else
#define mem_alloc(bsize)	malloc(bsize)
#define mem_free(ptr, bsize)	free(ptr)
#endif

#include <sys/time.h>

#if !defined(INADDR_LOOPBACK) && !defined(hpux)
/*
#define       INADDR_LOOPBACK         (u_long)0x7F000001
*/
#endif
#ifndef MAXHOSTNAMELEN
#define        MAXHOSTNAMELEN  64
#endif

#endif /* ndef __TYPES_RPC_HEADER__ */
