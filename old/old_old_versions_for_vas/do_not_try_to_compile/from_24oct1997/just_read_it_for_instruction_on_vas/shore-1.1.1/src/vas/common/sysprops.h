/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __SYSPROPS_H__
#define __SYSPROPS_H__

/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/sysprops.h,v 1.11 1995/07/14 22:38:30 nhall Exp $
 */
#include <copyright.h>
#include <sys/stat.h>

#ifndef S_IFIFO
#define S_IFIFO  (001 << 12)
#endif /*S_IFIFO*/

#ifndef S_IFCHR
#define S_IFCHR  (002 << 12)
#endif /*S_IFCHR*/

#ifndef S_IFDIR
#define S_IFDIR  (004 << 12)
#endif /*S_IFDIR*/

#ifndef S_IFBLK
#define S_IFBLK  (006 << 12)
#endif /*S_IFBLK*/

#ifndef S_IFREG
#define S_IFREG  (010 << 12)
#endif /*S_IFREG*/

#ifndef S_IFLNK
#define S_IFLNK  (012 << 12)
#endif /*S_IFLNK*/

#ifndef S_IFSOCK
#define S_IFSOCK  (014 << 12)
#endif /*S_IFSOCK*/

#ifndef S_IFMT
#define S_IFMT  (017 << 12)
#endif /*S_IFMT*/

#define MODE_ONLY_BITS 07777

/* mapping of mode bits for non-Unix objects */
#define S_IFPOOL S_IFSOCK
#define S_IFINDX S_IFBLK	/* nothing for now */
#define S_IFXREF S_IFIFO
#define S_IFNTXT S_IFCHR    /* /dev/null */


union _sysprops {
	struct _common_sysprops common;
	struct _common_sysprops_withtext commontxt;
	struct _common_sysprops_withindex commonidx;
	struct _common_sysprops_withtextandindex commontxtidx;
	struct _reg_sysprops reg;
	struct _reg_sysprops_withtext regtxt;
	struct _reg_sysprops_withindex regidx;
	struct _reg_sysprops_withtextandindex regtxtidx;
	struct _anon_sysprops anon;
	struct _anon_sysprops_withtext anontxt;
	struct _anon_sysprops_withindex anonidx;
	struct _anon_sysprops_withtextandindex anontxtidx;
};
typedef union _sysprops _sysprops;

// the form the tag takes on disk:
// has 2 Booleans folded into the tag:
// has-any-indexes, and has-text-field
typedef unsigned int sysp_tag;

// these are in sysprops_util.C
ObjectKind	ptag2kind(sysp_tag, bool *x=0, bool *y=0);

int	sysp_swap(const void *d, union _sysprops *s); // from disk
int	sysp_swap(void *d, const union _sysprops *s); // to disk

// sysp_split assumes the _sysprops is already xdr-ed
ObjectKind	sysp_split(
	union _sysprops 	&s, 
	AnonProps			**_a=0, 
	RegProps			**_r=0,
	ObjectOffset		*_tstart=0,
	int					*_nindex=0,
	int					*_sz=0
);
sysp_tag	kind2ptag(ObjectKind, bool , bool);
sysp_tag	kind2ptag(ObjectKind k, ObjectOffset    tstart, int	nindexes);

#endif /*__SYSPROPS_H__*/
