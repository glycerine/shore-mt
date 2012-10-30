/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef BASICS_H
#define BASICS_H
/*
 *  $Header: /p/shore/shore_cvs/src/common/basics.h,v 1.51 1997/06/13 22:31:03 solomon Exp $
 */
#ifdef __GNUG__
#pragma interface
#endif

#include <config.h>

#ifndef W_WORKAROUND_H
#include <w_workaround.h>
#endif

/*
 * NB -- THIS FILE MUST BE LEGITIMATE INPUT TO cc and RPCGEN !!!!
 * Please do as follows:
 * a) keep all comments in traditional C style.
 * b) If you put something c++-specific make sure it's 
 * 	  got ifdefs around it
 */

#ifndef RPCGEN
#include <unix_error.h>
#include <sys/types.h>
#include <assert.h>
#endif

#if defined(__cplusplus)&&!defined(EXTERN)
#define EXTERN extern "C"
#endif

#ifndef INLINE
#ifdef DEBUG
#   define INLINE
#else
#   define INLINE inline
#endif /*DEBUG*/
#endif /*INLINE*/

typedef short   int2;
typedef long    int4;
typedef unsigned char	uint1;
typedef unsigned short	uint2;
typedef unsigned long	uint4;

/* sizes-in-bytes for all persistent data in the SM. */
typedef uint4	smsize_t;

/*
 * Sizes-in-Kbytes for for things like volumes and devices.
 * A KB is assumes to be 1024 bytes.
 * Note: a different type was used for added type checking.
 */
typedef unsigned int	smksize_t;

#ifndef W_BOOLEAN_H
#include <w_boolean.h>
#endif

#ifdef __cplusplus
const int2	max_int2 = 0x7fff; 		/*  (1 << 15) - 1; 	*/
const int2	min_int2 = 0x8000; 		/* -(1 << 15);		*/
const int4	max_int4 = 0x7fffffff;	 	/*  (1 << 31) - 1;  */
const int4	max_int4_minus1 = max_int4 -1;
const int4	min_int4 = 0x80000000; 		/* -(1 << 31);		*/

const uint2	max_uint2 = 0xffff;
const uint2	min_uint2 = 0;
const uint4	max_uint4 = 0xffffffff;
const uint4	min_uint4 = 0;
#else
const max_int2 = 0x7fff; 		/*  (1 << 15) - 1; 	*/
const min_int2 = 0x8000; 		/* -(1 << 15);		*/
const max_int4 = 0x7fffffff;	 	/*  (1 << 31) - 1;  */
const max_int4_minus1 = 0x7ffffffe; 	/*  (1 << 31) - 2;  */
const min_int4 = 0x80000000; 		/* -(1 << 31);		*/

const max_uint2 = 0xffff;
const min_uint2 = 0;
const max_uint4 = 0xffffffff;
const min_uint4 = 0;
#endif

#ifdef __cplusplus
/*
 * Safe Integer conversion (ie. casting) functions
 */
inline int u4i(uint4 x)  {assert(x<=max_int4); return (int) x; }
inline uint uToi(int x)  {assert(x>=0); return (uint) x; }
#endif

/*
 * Alignment related definitions
 */
#define ALIGNON 0x8
#define ALIGNON1 (ALIGNON-1)


#ifdef align
#undef align
#endif

#ifndef align
/*
 * Alignment related definitions
 */
/*
// don't even THINK about making this an inline function,
// because then we can't define any static constants
// that use it.  We should be able to do so, in order
// to encourage the compiler to fold constants.
*/
#define ALIGNON 0x8
#define ALIGNON1 (ALIGNON-1)
#define align(sz) ((uint4)((sz + ALIGNON1) & ~ALIGNON1))
#endif /*align*/

#ifdef __cplusplus
/*
inline uint4 align(smsize_t sz)
{
	return (sz + ((sz & ALIGNON1) ? (ALIGNON - (sz & ALIGNON1)) : 0));
}
*/

inline bool is_aligned(smsize_t sz)
{
    return (align(sz) == sz);
}

inline bool is_aligned(const void* p)
{
    return is_aligned((unsigned int) p);
}
#endif

    /* used by sm and all layers: */
enum CompareOp {
	badOp=0x0, eqOp=0x1, gtOp=0x2, geOp=0x3, ltOp=0x4, leOp=0x5,
	/* for internal use only: */
	NegInf=0x100, eqNegInf, gtNegInf, geNegInf, ltNegInf, leNegInf,
	PosInf=0x400, eqPosInf, gtPosInf, gePosInf, ltPosInf, lePosInf
    };

/*
 * Lock modes for the Storage Manager.
 * Note: Capital letters are used to match common usage in DB literature
 * Note: Values MUST NOT CHANGE since order is significant.
 */
enum lock_mode_t {
    NL = 0, 		/* no lock				*/
    IS, 		/* intention share (read)		*/
    IX,			/* intention exclusive (write)		*/
    SH,			/* share (read) 			*/
    SIX,		/* share with intention exclusive	*/
    UD,			/* update (allow no more readers)	*/
    EX			/* exclusive (write)			*/
};

/* used by lock manager and all layers: */
enum lock_duration_t {
    t_instant 	= 0,	/* released as soon as the lock is acquired */
    t_short 	= 1,	/* held until end of some operation         */
    t_medium 	= 2,	/* held until explicitly released           */
    t_long 	= 3,	/* held until xct commits                   */
    t_very_long = 4	/* held across xct boundaries               */
};

enum vote_t {
    vote_bad,	/* illegit value			    */
    vote_readonly,  /* no ex locks acquired for this tx         */
    vote_abort,     /* cannot commit                            */
    vote_commit     /* can commit if so told                    */
};

/* used by lock escalation routines */
enum escalation_options {
    dontEscalate		= max_int4_minus1,
    dontEscalateDontPassOn,
    dontModifyThreshold		= -1
};


#ifdef __cplusplus
inline bool bigendian()
{
    const int i = 3;
    const char* c = (const char*) &i;
    return ((int)c[sizeof(int)-1]) == i; 
}
#endif /* __cplusplus */

/*
 * These types are auto initialized filler space for alignment
 * in structures.  The auto init helps with purify.
 */
struct fill1 {
	uint1 u1;
#ifdef __cplusplus
	fill1() : u1(0) {}
#endif /* __cplusplus */
};

struct fill2 {
	uint2 u2;
#ifdef __cplusplus
	fill2() : u2(0) {}
#endif /* __cplusplus */
};

struct fill4 {
	uint4 u4;
#ifdef __cplusplus
	fill4() : u4(0) {}
#endif /* __cplusplus */
};

#endif /* BASICS_H */
