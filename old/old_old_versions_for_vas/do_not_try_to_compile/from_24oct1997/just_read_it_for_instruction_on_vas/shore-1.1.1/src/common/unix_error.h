/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/unix_error.h,v 1.5 1995/04/24 19:28:52 zwilling Exp $
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
  void perror(const char *s);
  char *strerror(int errnum);
#ifdef __cplusplus
}
#endif

#endif
