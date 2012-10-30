/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sysdefs.h,v 1.23 1997/04/21 20:42:22 bolo Exp $
 */
#ifndef SYSDEFS_H
#define SYSDEFS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#ifdef Decstation
#include <sysent.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <setjmp.h>
#include <new.h>
#include <stream.h>
#include <strstream.h>
#include <fstream.h>
#include <limits.h>
//#include <rpc/rpc.h>

#define W_INCL_LIST
#include <w.h>
#include <sthread.h>

/* Some system include files use bcopy or bzero, so define these */

#define bcopy(a, b, c)   memcpy(b,a,c)
#define bzero(a, b)      memset(a,'\0',b)
#define bcmp(a, b, c)    memcmp(a,b,c)

#endif /* SYSDEFS_H */
