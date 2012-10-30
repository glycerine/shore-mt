/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef W_BOOLEAN_H
#define W_BOOLEAN_H

/*
 *  Booleans
 *
 * When making with a compiler that doesn't have "bool" built-in type,
 * define NO_BOOL.
 *
 * (gcc 2.6 has bool as a built-in type )
 *
 * RPCGEN already knows about bool & bool_t, but not about its values
 */

#ifdef NO_BOOL

/*
 * gcc treats bool as byte-sized thing,
 * and the size of it *MUST* be maintained
 * if any code using bool is going to interact
 * properly with the RPC package.
 */
typedef char		bool;

const char false = '\0';
const char true = '\1';

#endif


#ifndef RPCGEN

#ifdef BOOL_COMPAT
#ifdef TRUE
#undef TRUE
#endif
#ifdef FALSE
#undef FALSE
#endif

#define TRUE true
#define FALSE false
#endif	/* BOOL_COMPAT */
/*
// our modified rpcgen recognizes bool_t
// unfortunately, conventional <rpc/types.h>
// defines it to be an int
*/
#endif /* RPCGEN */

/*
// small_bool_t is guaranteed, under
// all circumstances, to be 1 byte
*/
#ifndef LARGE_BOOL_SIZE
/* normally defined if needed in w_workaround.h, but that is apparently not
 * included in all contexts where w_boolean.h is.
 * this is troublesome...
 */
#if defined(__GNUC__) && ( __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ >= 7))
	#define LARGE_BOOL_SIZE sizeof(bool)
#endif
#endif

#if defined(SOLARIS2) || defined(LARGE_BOOL_SIZE)
	/*
	// on Solaris, we're not using our
	// modified xdr package so we
	// must define this:
	// note also: gcc 2.7.* has sizeof(bool)=4, or
	// maybe just bool==int, so check the size also.
	// LARGE_BOOL is defined for gcc 2.7 +, in w_workaround.h.
	// it would have been nice to say sizeof(bool)>1, but that's 
	// too hard for cpp.
	*/
	typedef unsigned char small_bool_t;
#else
	/*
	// when using our modified xdr pkg,
	// bool is 1-byte 
	*/
	typedef bool small_bool_t;
#endif /* SOLARIS2 */

#endif /*W_BOOLEAN_H*/
