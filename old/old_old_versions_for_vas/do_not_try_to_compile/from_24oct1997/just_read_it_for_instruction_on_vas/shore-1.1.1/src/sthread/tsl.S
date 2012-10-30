/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/sthread/tsl.S,v 1.14 1997/09/14 11:53:25 solomon Exp $	
 */
/*
 * unsigned
 * tsl(addr, value)
 *        tslcb_t *addr;
 *
 * tsl() returns '0' if it could set the byte lock to 
 * a non-zero 'value', which may be ignored depending upon
 * the implementation.  Otherwise, tsl() returns
 * the current value of the  byte lock.
 *
 * Depending upon a particular 'value' for the byte may be
 * a pitfall of using this function.  Also, assuming that
 * the syncronization unit is a byte may be invalid also.
 *
 * void
 * tsl_release(addr)
 *	tslcb_t *addr;
 *
 * Release a tsl() lock.  A simple store may not be enough
 * in some cases.
 *
 * void
 * tsl_init(addr)
 *	tslcb_t *addr;
 * Initialize a tsl() lock.
 *
 * unsigned
 * tsl_examine(addr)
 *	tslcb_t *addr;
 * Retrieve the value of a tsl() lock; 0==not locked, 1==locked
 *
 *
 * ** A future enhancement to this package would be to have the
 * machine-dependent test+set lock protect another (adjacent) location,
 * so that the "value" argument of tsl() could be supported
 * on all architectures.
 *
 * Author: Joe Burger (bolo@cs.wisc.edu) on 28 November 1990
 */

#if defined(sparc)
/*
 * The 'value' argument is ignored with the sparc implementation,
 * although it could be implemented by writing the 'value' to the
 * byte after ldstub "locks" it.
 *
 * Function of 'ldstub':
 * Ldstub fetches the contents of the byte pointed to by 'addr'
 * and replaces the contents of the byte with hex 0xff (decimal 255).
 * Presumably this is done with a RWM bus cycle.
 * The 'C' function return value is the previous value of the byte.
 *
 */
#ifdef SOLARIS2
#define	PROC(x)	.seg "text" ; .proc 14 ; .global x ; x :
#else
#ifdef __STDC__
#define	PROC(x)	.seg "text" ; .proc 14 ; .global _##x ; _##x :
#else
#define	PROC(x)	.seg "text" ; .proc 14 ; .global _/**/x ; _/**/x :
#endif
#endif

PROC(tsl)
	retl
	ldstub	[%o0],%o0	/* in delay slot */

PROC(tsl_release)
	retl
	stb	%g0,[%o0]

PROC(tsl_init)
	retl
	stb	%g0,[%o0]

PROC(tsl_examine)
	retl
	ldub	[%o0],%o0
#endif /* sparc */

#if defined(mc68000) || defined(mc68010) || defined(mc68020) || defined(mc68030) || defined(mc68040)

/*
 * cas: compare and swap with operand; usage:  cas  dc,du,<ea>
 * The 'dc' "compare" operand is compared to the value of the operand
 * at <ea>.  If they are equal, the value at <ea> is replaced/updated
 * with the value of the 'du' "update" operand.
 */
#define	PROC(x)	.text ; .even ; .globl x ; x:

PROC(_tsl)
	movl	sp@(4),a0	/* address */
	movl	sp@(8),d1	/* value */
	clrl	d0		/* compare operand for CAS */
	casb	d0,d1,a0@
	/* return is in d0 == old value of <ea> */
	rts

PROC(_tsl_release)
	movl	sp@(4),a0	/* address */
	clrl	d0
	movb	d0,a0@
	rts

PROC(_tsl_init)
	movl	sp@(4),a0	/* address */
	clrl	d0
	movb	d0,a0@
	rts

PROC(_tsl_examine)
	movl	sp@(4),a0
	clrl	d0
	movb	a0@,d0
	rts
#endif /* mc 68000 */

#if defined(vax)

/*
 * bbssi: branch on bit set and set interlocked
 * The bit is checked for its current value, then is always set.
 * If the bit is set, the branch is taken, else execution falls through
 */
#define	PROC(x)	.text ; .align 1 ; .globl x; x :

PROC(_tsl)
	.word	0x00	/* don't save any registers */
	bbssi	$0,*4(ap),1f
	/* bit not set, we "have" the byte */
	clrl	r0	/* return value */
	ret
1:	/* bit was set, we can't have it yet */
	movl	$1,r0	/* return value */
	ret
PROC(_tsl_release)
	.word	0x00	/* don't save any registers */
	clrb	*4(ap)
	ret
PROC(_tsl_init)
	.word	0x00	/* don't save any registers */
	clrb	*4(ap)
	ret
PROC(_tsl_examine)
	.word	0x00
	movb	*4(ap),r0	/* get the lock */
	bicl2	$0xfe,r0	/* and clear all unused bits */
	ret
#endif /* vax */

#if defined(i386)
/*
 * bts: bit test and set
 * The bit is copied into the carry flag (CF) and then set bit
 * xchg: exchange memory and register
 * exchange contents of memory location and register
 *
 * currently ignores flag value
 */

#define	PROC(x)	.text ; .align 2 ; .globl x ; x:

#ifdef SYSV
PROC(tsl)
#else
PROC(_tsl)
#endif
	pushl	%ebp	/* build stack frame for debuggers */
	movl	%esp,%ebp
	movl	8(%ebp),%ecx	/* addr -> cx */
	movl	$1,%eax		/* "1" == test and set grabbed */
	lock
	xchgb  (%ecx),%al	/* return value in %eax */
	leave
	ret
#ifdef SYSV
PROC(tsl_release)
#else
PROC(_tsl_release)
#endif
	pushl	%ebp	/* build stack frame for debuggers */
	movl	%esp,%ebp
	movl	8(%ebp),%ecx	/* addr */
	movb	$0,(%ecx)	/* clear it */
	leave
	ret

#ifdef SYSV
PROC(tsl_init)
#else
PROC(_tsl_init)
#endif
	pushl	%ebp	/* build stack frame for debuggers */
	movl	%esp,%ebp
	movl	8(%ebp),%ecx	/* addr */
	movb	$0,(%ecx)	/* clear it */
	leave
	ret

#ifdef SYSV
PROC(tsl_examine)
#else
PROC(_tsl_examine)
#endif
	pushl	%ebp
	movl	%esp,%ebp
	movl	8(%ebp),%ecx
	movl	$0,%eax
	movb	(%ecx),%al
	leave
	ret
#endif /* i386 */

#if defined(mips) && !defined(Irix) 
/*
 * mips doesn't have a test-and-set instruction.
 * this placeholder is here so the mips assembler doesn't choke
 * on the empty input file
 */
	.align	2
/*
 * Future versions of the MIPS architecture (r4000 or r6000)
 * are rumored to have some form of test and set.
 * They are the same machines that require a dedicated 
 * tsl_release() to release the lock.
 */
#endif /* mips && !Irix */

#if defined(Mips) && defined(Irix)

	.file	1 "tsl.c"
	.option pic2

gcc2_compiled.:
__gnu_compiled_c:
	.text

#define PROC(x)		.align 2 ; .globl x ; .ent x

PROC(tsl_init)
tsl_init:
	.frame	$fp,16,$31		# vars= 0, regs= 2/0, args= 0, extra= 8
	.mask	0x50000000,-4
	.fmask	0x00000000,0
	.set	noreorder
	.cpload	$25
	.set	reorder
	subu	$sp,$sp,16
	.cprestore 0
	sw	$fp,12($sp)
	sw	$28,8($sp)
	move	$fp,$sp
	sw	$4,16($fp)
					# START my code
	li	$3,0x0
	sw	$3,0($4)		# initialize the lock to 0
					# END my code
$L1:
	move	$sp,$fp			# sp not trusted here
	lw	$fp,12($sp)
	addu	$sp,$sp,16
	j	$31
	.end	tsl_init

PROC(tsl)
tsl:
	.frame	$fp,16,$31		# vars= 0, regs= 2/0, args= 0, extra= 8
	.mask	0x50000000,-4 ; .fmask	0x00000000,0
	.set	noreorder
	.cpload	$25
	.set	reorder
	subu	$sp,$sp,16
	.cprestore 0
	sw	$fp,12($sp)
	sw	$28,8($sp)
	move	$fp,$sp
	sw	$4,16($fp)
	sw	$5,20($fp)
					# START my code 
$L10:
	li	$3,0x1			# 1. Cannot be moved out of the loop
	ll	$2,0($4)	
	sc	$3,0($4)
	bnez	$3,$L2
	j	$L10
$L2:
	move	$sp,$fp			# sp not trusted here
	lw	$fp,12($sp)
	addu	$sp,$sp,16
	j	$31
	.end	tsl

PROC(tsl_examine)
tsl_examine:
	.frame	$fp,16,$31		# vars= 0, regs= 2/0, args= 0, extra= 8
	.mask	0x50000000,-4
	.fmask	0x00000000,0
	.set	noreorder
	.cpload	$25
	.set	reorder
	subu	$sp,$sp,16
	.cprestore 0
	sw	$fp,12($sp)
	sw	$28,8($sp)
	move	$fp,$sp
	sw	$4,16($fp)
					# START my code 
	lw	$2,0($4)
					# END my code
$L3:
	move	$sp,$fp			# sp not trusted here
	lw	$fp,12($sp)
	addu	$sp,$sp,16
	j	$31
	.end	tsl_examine

PROC(tsl_release)
tsl_release:
	.frame	$fp,16,$31		# vars= 0, regs= 2/0, args= 0, extra= 8
	.mask	0x50000000,-4
	.fmask	0x00000000,0
	.set	noreorder
	.cpload	$25
	.set	reorder
	subu	$sp,$sp,16
	.cprestore 0
	sw	$fp,12($sp)
	sw	$28,8($sp)
	move	$fp,$sp
	sw	$4,16($fp)
					# START my code 
	li	$3,0x0
	sw	$3,0($4)		# reset the lock to 0
					# END my code
$L4:
	move	$sp,$fp			# sp not trusted here
	lw	$fp,12($sp)
	addu	$sp,$sp,16
	j	$31
	.end	tsl_release

#endif /* mips && Irix */

#if defined(hppa) || defined(hp800)
	 /* 	
	 * Gads this one is backwards! :
	 * *loc == '0' means it is locked
	 * *loc != '0' means it is unlocked
	 *
	 * Also, the lock needs to be 16 bytes on 16 byte alignment
	 * The code auto-magically aligns the pointer to the next
	 * 16 byte alignment, the user has to provide enough space
	 * for alignment if they don't guarantee 16-byte alignment
	 * of the lock
	 */

	.SPACE $PRIVATE$
	.SUBSPA $DATA$,QUAD=1,ALIGN=8,ACCESS=31
	.SPACE $TEXT$
	.SUBSPA $LIT$,QUAD=0,ALIGN=8,ACCESS=44
	.SUBSPA $CODE$,QUAD=0,ALIGN=8,ACCESS=44,CODE_ONLY
	.IMPORT $global$,DATA
	.IMPORT $$dyncall,MILLICODE
; gcc_compiled.:
	.SPACE $TEXT$
	.SUBSPA $CODE$

	.align 4
	.EXPORT tsl,PRIV_LEV=3,ARGW0=GR,ARGW1=GR,RTNVAL=GR
tsl
	.PROC
	.CALLINFO FRAME=24,NO_CALLS
	.ENTRY
	addi	15,26,26	/* align addr to nearest 16: addr += (16-1) */
	ldi	15,1
	andcm	26,1,26		/* addr &= ~15 */
	ldcws	0(0,26),28	/* delay; clear *arg0, old *arg0 -> ret0 */
	comclr,<>  28,0,28	/* was it  == 0 ??  (locked) */
	ldo	1(0),28			/* if so, return failure */
	bv 0(2)			/* return to RP */
	nop

	.EXIT
	.PROCEND
	.align 4
	.EXPORT tsl_release,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
tsl_release
	.PROC
	.CALLINFO FRAME=24,NO_CALLS
	.ENTRY
	addi	15,26,26	/* align to nearest 16: addr += 15 */
	ldi	15,1
	andcm	26,1,26		/* addr &= ~15 */
	ldi  1,19		/* 1 -> caller saves 19 */
	sync			/* hopefully sync any stores */
	bv 0(2)			/* return to RP */
	stw 19,0(0,26)		/* delay: reg(19)==1 -> *arg0 */
	.EXIT
	.PROCEND

	.align 4
	.EXPORT tsl_examine,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
tsl_examine
	.PROC
	.CALLINFO FRAME=24,NO_CALLS
	.ENTRY
	addi	15,26,26	/* align to nearest 16: addr += 15 */
	ldi	15,1
	andcm	26,1,26		/* addr &= ~15 */
	ldw	0(26),28	/* (addr) -> ret0 */
	nop
	comclr,<>  28,0,28	/* was it  == 0 ??  (locked) */
	ldi	1,28			/* if so, return 1 */
	bv 0(2)			/* return to RP */
	nop
	.EXIT
	.PROCEND

	.align 4
	.EXPORT tsl_init,PRIV_LEV=3,ARGW0=GR,RTNVAL=GR
tsl_init
	.PROC
	.CALLINFO FRAME=24,NO_CALLS
	.ENTRY
	addi	15,26,26	/* align to nearest 16: addr += 15 */
	ldi	15,1
	andcm	26,1,26		/* addr &= ~15 */
	ldi  1,19		/* 1 -> caller saves 19 */
	bv 0(2)			/* return to RP */
	stw 19,0(0,26)		/* delay: reg(19)==1 -> *arg0 */
	.EXIT
	.PROCEND

#endif /* hppa */

#if defined(ibm032)

/*
 * tsh: test and set halfword
 *	the hi-order byte of the hword is set to 0xff
 *	the lo-order byte is unaltered.
 * 
 * The 'value' argument is ignored
 */
#define	DEFINEPROC(x,y)	.data ; .align 2 ; .globl x; x: .long y
#define	PROC(x)	.text ; .align 2 ; .globl x ; x:
#define	HCC

#if defined(HCC)
	.globl  .oVncs
	.set    .oVncs,0
	.globl  .oVhc2.1s
	.set    .oVhc2.1s,0
	.data
L00DATA:
	DEFINEPROC(_tsl_examine,_.tsl_examine)
	DEFINEPROC(_tsl,_.tsl)
	DEFINEPROC(_tsl_release,_.tsl_release)
	DEFINEPROC(_tsl_init,_.tsl_init)
#endif

#if defined(PCC)
LL0:	pcc stuff not supported
#endif

#define	arg0	r2
#define	ret0	arg0

L00TEXT:
PROC(_.tsl)
	brx	r15		/* return */
	tsh	ret0,0(arg0)	/* do the tsl, 0==got it */
	.long	0xdf02df00	/* trace table -- no stack frame */

PROC(_.tsl_examine)
	brx	r15		/* return */
	lhs	ret0,0(arg0)	/* fetch the word */
	.long	0xdf02df00	/* trace table -- no stack frame */

PROC(_.tsl_release)
	lis	r0,0
	brx	r15
	sths	r0,0(r2)
	.long	0xdf02df00	/* trace table -- no stack frame */

PROC(_.tsl_init)
	lis	r0,0
	brx	r15
	sths	r0,0(r2)
	.long	0xdf02df00	/* trace table -- no stack frame */

#if defined(PCC)
        .ltorg
        .globl .oVpcc
        .globl .oVncs
        .set .oVpcc, 0
        .set .oVncs, 0
#endif

#endif /* ibm032 */

#if defined(i860) || defined(__i860)

#define	arg0	r16
#define	arg1	r17
#define	arg2	r18	/* used as scratch */
#define	zero	r0
#define	ra	r1
#define	retval	arg0
#ifdef __STDC__
#define	FUNC(name)	.globl _##name; .align 8; _##name:
#else
#define	FUNC(name)	.globl _/**/name; .align 8; _/**/name:
#endif

/*
   void testandset_init(&lock)
   testandset(&lock, value)
   void testandset_release(&lock)
   testandset_examine(&lock)
 */
FUNC(tsl_init)
	bri	ra
	st.l	zero, 0(arg0)

FUNC(tsl_release)
	bri	ra
	st.l	zero, 0(arg0)

/* grabbed from mach3.0 cthreads support */
FUNC(tsl)
	nop
	lock
	ld.l	0(arg0),arg1
	mov	1,arg2
	unlock
	st.l	arg2,0(arg0)	// must be AFTER unlock inst (wlb)
	bri	r1
	addu	zero,arg1,arg0

#endif /* i860 */


#if defined(PowerPC)
/* untested lock Code almost verbatim from _The PowerPC Architecture_ by IBM */

#define	arg0	r3
#define	arg1	r4
#define	retval	r3
#define	FUNC(name)	.globl	.##name; .align 8; .##name:

.set	BO_ALWAYS,20
.set	CR0_LT,0
.set	r0,0
.set	r3,3
.set	r4,4
	
FUNC(tsl)
	/* XXX should test for arg1 == 0 */
	lwarx	r0, 0, arg0
	cmpwi	r0, 0
	bne-	T.nope
	stwcx	arg1, 0, arg0
T.NOPE:
	ai	retval, r0, 0
	bcr	BO_ALWAYS, CR0_LT

FUNC(tsl_release)
	cal	r0, 0(r0)
	st	r0, 0(arg0)
	bcr	B0_ALWAYS, CR0_LT

FUNC(tsl_init)
	cal	r0, 0(r0)
	st	r0, 0(r3)
	bcr	B0_ALWAYS, CR0_LT

FUNC(tsl_examine)
	lwz	retval, 0(arg0)
	bcr	B0_ALWAYS, CR0_LT

	
#endif /* rs6000 */	
