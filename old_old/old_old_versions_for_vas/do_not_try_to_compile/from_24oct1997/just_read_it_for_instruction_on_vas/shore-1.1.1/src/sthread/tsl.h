/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * lock definition file for testandset() package.
 *
 * Author: Josef Burger
 * $Id: tsl.h,v 1.11 1995/11/27 18:47:08 bolo Exp $
 */

typedef struct tslcb_t {
#if defined(hp800) || defined(hppa)
    /*
     * gads, what a kludge
     * 
     * HPPA needs a 16 byte length aligned at a 16 byte boundary.
     * The tsl() package will automatically align the address
     * given to it to the NEXT 16 byte boundary.
     * 
     * If you use malloc, to allocate locks, it will return a
     * 8-byte aligned pointer, and you can use the lock[6] definition
     * for alignment to work correctly.
     *
     * If you use statically-allocated locks, either verify that 
     * the compiler will guarantee a 8-byte alignment for the structure,
     * or use a length of 32 (28 if space is tight) to guarantee
     * enough space for alignment.
     */
#if GUARANTEE_EIGHT
	int	lock[6];	/* MUST be 8-byte aligned */
#else
	int	lock[8];	/* if 4-aligned */
#endif
#endif /* hp800 */

#if defined(ibm032)
	short	lock;
#endif /* ibm032 */

#if defined(hp300) || defined(sparc) || defined(i386) || defined(vax)
	char	lock;
#endif	/* lots of stuff */

#if defined(mips)
	int	lock;
#endif

#if defined(i860) || defined(__i860)
	int	lock;
#endif

#if defined(Rs6000) || defined(PowerPC)
	int	lock;
#endif /* Rs6000 */

} tslcb_t;

extern void tsl_init(tslcb_t*);
extern unsigned tsl(tslcb_t*, int);
extern void tsl_release(tslcb_t*);
extern unsigned tsl_examine(tslcb_t*) ;
