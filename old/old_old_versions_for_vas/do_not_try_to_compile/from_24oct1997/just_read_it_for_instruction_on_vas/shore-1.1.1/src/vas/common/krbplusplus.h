/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __KRB_PLUS_H__
#define __KRB_PLUS_H__

// C++ defn's for krb funcs

extern "C" {
	int 	krb_sendauth(long, int, KTEXT,
		char *, char *, char *,
		u_long, MSG_DAT *,
		CREDENTIALS *,
		Key_schedule,
		struct sockaddr_in *,
		struct sockaddr_in *,
		char *);
	int 	krb_recvauth(long, int, KTEXT,
		char *, char *, 
		struct sockaddr_in *,
		struct sockaddr_in *,
		AUTH_DAT *,
		char *,
		Key_schedule,
		char *);
	int	krb_kntoln(AUTH_DAT *,char *);
}

#endif
