/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/ndbm_cplus.h,v 1.8 1995/04/24 19:28:10 zwilling Exp $
 */
#ifndef NDBM_CPLUS_H
#define NDBM_CPLUS_H

/*
 * Special include file to be used in place of ndbm.h.  This include
 * file works with those systems without a prototyped ndbm.h
 * For now this is SunOS with g++.
 */

#if defined(__GNUC__) && (defined(I860) || defined(Sparc) || defined(Mips))
#define	NDBM_PROTO_KLUDGE
#endif

/*
 * SunOS include files do not support C++, so this #defines away
 * the functions so that later in the file we can declare them
 * with prototypes.
 */
#ifdef NDBM_PROTO_KLUDGE
#   define dbm_open __dbm_open_undef
#   define dbm_close __dbm_close_undef
#   define dbm_fetch __dbm_fetch_undef
#   define dbm_firstkey __dbm_firstkey_undef
#   define dbm_nextkey __dbm_nextkey_undef
#   define dbm_forder __dbm_forder_undef
#   define dbm_delete __dbm_delete_undef
#   define dbm_store __dbm_store_undef
#endif /* NDBM_PROTO_KLUDGE */

#include <ndbm.h>

#ifdef NDBM_PROTO_KLUDGE

#   undef dbm_open
#   undef dbm_close
#   undef dbm_fetch
#   undef dbm_firstkey
#   undef dbm_nextkey
#   undef dbm_forder
#   undef dbm_delete
#   undef dbm_store

#   ifdef __cplusplus
	extern "C" {
#    endif

#   if defined(__STDC__) || defined(__cplusplus)
	extern DBM *dbm_open(const char *, int, int);
	extern void dbm_close(DBM *);
	extern datum dbm_fetch(DBM *, datum);
	extern datum dbm_firstkey(DBM *);
	extern datum dbm_nextkey(DBM *);
	extern long dbm_forder(DBM *, datum);
	extern int dbm_delete(DBM *, datum);
	extern int dbm_store(DBM *, datum, datum, int);
#   else /* not __STDC__ || __cplusplus */
	extern DBM *dbm_open();
	extern void dbm_close();
	extern datum dbm_fetch();
	extern datum dbm_firstkey();
	extern datum dbm_nextkey();
	extern long dbm_forder();
	extern int dbm_delete();
	extern int dbm_store();
#   endif /* else not __STDC__ || __cplusplus */

#   ifdef __cplusplus
	}
#   endif

#endif /* NDBM_PROTO_KLUDGE */

#endif /* NDBM_CPLUS_H */
