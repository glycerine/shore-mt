/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/fc/unix_error.h,v 1.5 1995/04/24 19:31:44 zwilling Exp $
 */

#ifndef _UNIX_ERROR_H_
#define _UNIX_ERROR_H_

#include <errno.h>

extern int errno;
extern char *sys_errlist[];
extern int sys_nerr;

#ifdef __cplusplus
extern "C" {
#endif

/* gcc 2.6.0 include files contain perror */
#if (!defined(__GNUC__)) || (__GNUC_MINOR__ < 6)
  void perror(const char *s);
#endif
  char *strerror(int errnum);

#ifdef __cplusplus
}
#endif

#endif
