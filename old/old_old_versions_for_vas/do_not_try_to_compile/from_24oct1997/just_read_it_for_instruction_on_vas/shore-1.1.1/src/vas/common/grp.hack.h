/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __GRP_HACK_H__
#define __GRP_HACK_H__
#ifdef OSF1AD
#ifndef grp_h

extern "C" {

#ifdef __grp_h_recursive
#include_next <grp.h>
#else
#define __grp_h_recursive

#include <stdio.h>

#define getgrent c_proto_getgrent
#define getgrgid c_proto_getgrgid
#define getgrnam c_proto_getgrnam
#define setgrent c_proto_setgrent
#define endgrent c_proto_endgrent
#define fgetgrent c_proto_fgetgrent

#include_next <grp.h>

#define grp_h 1

#undef getgrent 
#undef getgrgid
#undef getgrnam
#undef setgrent
#undef endgrent
#undef fgetgrent

extern struct group* getgrent();
extern struct group* fgetgrent(FILE*);
extern struct group* getgrgid(int);
extern struct group* getgrnam(const char*);
#ifdef __OSF1__
extern int	     setgrent();
#else
extern void          setgrent();
#endif
extern void          endgrent();

#endif
}

#endif
#else
#error grp.hack.h should not have been included
#endif

#endif /*__GRP_HACK_H__*/
