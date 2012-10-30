/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/common/uname.h,v 1.5 1995/08/14 22:50:49 nhall Exp $
 */
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#ifndef BEGIN_EXTERNCLIST
#include <externc.h>
#endif

#ifdef __cplusplus
#define DEFAULT_IS_NULL =0
#else
#define DEFAULT_IS_NULL 
#endif
BEGIN_EXTERNCLIST
	char 	*uid2uname(uid_t  uid, gid_t *gout DEFAULT_IS_NULL);
	char 	*gid2gname(gid_t  uid);
	uid_t 	uname2uid(char *, gid_t *gout DEFAULT_IS_NULL);
	gid_t 	gname2gid(char *);
	int	 	get_client_groups(const char *, gid_t *);

	const 	char *canon_hostname(const char *h); /* returns ptr to static area */
	void cleanup_pwstuff();
	void cleanup_netdb();
END_EXTERNCLIST
