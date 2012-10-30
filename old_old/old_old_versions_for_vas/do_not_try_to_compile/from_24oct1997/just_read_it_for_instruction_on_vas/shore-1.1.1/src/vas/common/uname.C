/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/common/uname.C,v 1.17 1997/01/23 22:37:24 nhall Exp $
 */


#include <stream.h>
#include <pwd.h>
#include <inet_stuff.h>
#include <netdb.h>
#include <assert.h>
#include <string.h>
#include <debug.h>

#ifdef OSF1AD
#include <grp.hack.h>
#else
#include <grp.h>
#endif

#include "uname.h"
#include <vas_types.h>

BEGIN_EXTERNCLIST
#if defined(Ultrix42) || defined(SUNOS41) || defined(OSF1AD)
	void	endpwent();
	void	endgrent();
	void	endhostent();
#endif
END_EXTERNCLIST

const	spacesize = 100 + sizeof(struct passwd) + sizeof(struct group);

static char space[ spacesize ];

char *uid2uname(
	uid_t	cuid,
	gid_t	*gout // =0
)
{
	uid_t	uid = (uid_t)(cuid);
	
	DBG( <<"uid2uname for uid " << uid );
#ifdef OBSOLETE
	struct passwd _p, *p = getpwuid_r(uid, &_p, space, spacesize);
#else
	struct passwd *p = getpwuid(uid);
#endif
	DBG( <<"returned from getpwuid");
	if(p == NULL) return NULL;

	assert(p->pw_uid == uid);
	DBG( <<"uid2uname got " << p->pw_name );
	strncpy(space,p->pw_name,spacesize); 
	if(gout) *gout = p->pw_gid;
	DBG( <<"endpwent()");
	endpwent();
	return space;
}

char *gid2gname(
	gid_t	cgid
)
{
	gid_t	gid = (gid_t)(cgid);
	DBG( <<"gid2gname for gid " << gid );
#ifdef OBSOLETE
	struct group _g, *g = getgrgid_r(gid, &_g, space, spacesize);
#else
	struct group *g = getgrgid(gid);
#endif
	if(g==NULL) {
		DBG( <<"no such group for gid " << gid );
		return NULL;
	}
	assert(gid == g->gr_gid);

	strncpy(space,g->gr_name,spacesize); 
	endgrent();
	return space;
}

uid_t uname2uid( char *name, 
	gid_t	*gout // =0
)
{
	uid_t	result = (uid_t)BAD_UID;

#ifdef OBSOLETE
	struct passwd _pwent, *pwent = getpwnam_r(name, &_pwent, 
		space, spacesize);
#else
	struct passwd *pwent = getpwnam(name);
#endif
	if(pwent == NULL) {
		perror("getpwnam");
		return result;
	}
	result = pwent->pw_uid;
	if(gout) *gout = pwent->pw_gid;
	endpwent();
	return result;
}


gid_t gname2gid(char *name)
{
#ifdef OBSOLETE
	struct group _g, *g = getgrnam_r(name, &_g, space, spacesize);
#else
	struct group *g = getgrnam(name);
#endif
	if(g!=NULL) {
		endgrent();
		return g->gr_gid;
	} else {
		return (gid_t) -1;
	}
}

void cleanup_pwstuff()
{
#ifndef OBSOLETE
	endgrent();
	endpwent();
#endif
}

void cleanup_netdb()
{
#ifndef OBSOLETE
	endhostent();
#endif
}

const   char *
canon_hostname(
	const char *host
)
{
	// cannonicalize the host name
#ifdef OBSOLETE
	int		_errno;
	struct hostent _h, *h = gethostbyname_r(host, &_h, space, spacesize, &_errno);
	if(!h) errno = _errno;
#else
	struct hostent *h = gethostbyname(host);
#endif
	if (!h) {
		return 0;
	}
	return h->h_name;
}

int
get_client_groups(const char *username, gid_t *groups)
{
	int ngroups;

	// like 
	// ngroups = getgroups(NGROUPS_MAX, groups);
	// bud for the given user name

	{
	   struct group *gr;
	   char    **mem;

	   ngroups = 0;
	   while( (gr = getgrent()) != NULL) {
		  for(mem = gr->gr_mem; (*mem) != NULL; mem++) {
			   if(strcmp(username, *mem)==0) {
				   // add this group id
				   groups[ngroups++] = gr->gr_gid;

				   DBG(
					   << username 
					   << " is in grp " << gr->gr_name << ", " << gr->gr_gid
					   )
			   }
			   dassert(ngroups <= NGROUPS_MAX);

		   }
	   }
	   endgrent();
	}

	return ngroups;
}
