/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#define __EFS_C__
static char *rcsid="$Header: /p/shore/shore_cvs/src/vas/server/mountd.C,v 1.22 1995/09/15 03:45:35 zwilling Exp $";

// mountd.C -- procedures to implement the NFS-compatible mount 

#include <copyright.h>

#define __malloc_h

#include "efs.h"
#include <uname.h>

//*************************** MOUNT ***************************/

svas_mount::mountent *svas_mount::mounts=0;

nfsstat 
svas_mount::mnt(
	const char *path, 
	authunix_parms *cred, 
	nfs_fh &result
) 
{
	FUNC(svas_mount::mnt);
	const char *host = cred->aup_machname;
	efs_handle hand;
	bool	found;

	VASResult vasres = this->_lookup1(path, &found, &hand.lrid,
		true, Permissions::op_none, &hand.reg_file);

	if (vasres != SVAS_OK) {
		// assume not found.
		errlog->log(log_error, "mount: file system %s not found\n",path);
		// but just tell the caller it wasn't exported
		return errno2nfs(vasstatus2errno());
		// return NFSERR_ACCES;
	}
	CHECK(hand);

	// TODO: make thread-safe
	host = canon_hostname(host);
	if(!host) {
		errlog->log(log_error, "mount: host %s unknown\n",host);
		RETURN NFSERR_ACCES;
	}

	// TODO: check authorization

	// record the mount
	// check for duplicate entries
	mountent* m;
	for (m = mounts; m; m = m->next) {
		if (strcmp(host, m->host)==0 && m->lrid==hand.lrid) {
			errlog->log(log_info, "mount warning: %s is already mounted by %s\n",
				path, host);
			break;
		}
	}
	if (!m) {
		m = new mountent;
		m->host = copy_string(host);
		m->fsname = copy_string(path);
		m->lrid = hand.lrid;
		m->next = mounts;
		mounts = m;
	}
	efs_s2h(hand, result);

	RETURN NFS_OK;
} /* mnt */

/*
//TODO: if a large number of file systems are mounted,
// the result could get large and overrun the stack
// with xdr_pointer()!!! Unfortunately, the protocol
// doesn't have a way to limit the number of entries returned
// in a result!
// I guess they figured it couldn't get huge!
*/

mountlist 
svas_mount::mounted() 
{
	FUNC(svas_mount::mounted);
// 	static mountbody *result;
	mountbody *result=0;
	mountbody *p, *q;
	mountent *m;

	// Delete leftover result from previous call
	// for (p = result; p; p = q) {
// 		q = p->ml_next;
// 		delete p->ml_hostname;
// 		delete p->ml_directory;
// 		delete p;
// 	}
	mountbody **tail;
	for (m = mounts, tail = &result;
			m;
			m = m->next, tail = &(*tail)->ml_next)
	{
		// must use malloc because it's part of an rpc result
		q = (mountbody *)malloc(sizeof(mountbody));

		*tail = q;
		if(q) {
			q->ml_hostname = copy_string(m->host);
			q->ml_directory = copy_string(m->fsname);
		} else {
			// TODO return error
			break;
		}
	}
	*tail = 0;
	RETURN result;
} /* mounted */

void 
svas_mount::unmount(
	authunix_parms *who, 
	const char *what
) 
{
	FUNC(svas_mount::unmount);
	mountent *m, *prev, *next;
	const char *host = who->aup_machname;

	host = canon_hostname(host);
	if(!host) {
		errlog->log(log_error, "unmount: host %s unknown\n",host);
		RETURN;
	}

	for (m = mounts, prev = 0; m; m = next) {
		next = m->next;
		if (strcmp(host, m->host) == 0
				&& (what == 0 || strcmp(what, m->fsname)==0)) {
			if (prev) prev->next = next;
			else mounts = next;
			delete m->host;
			delete m;
		}
		else prev = m;
	}
} /* unmount */

exports 
svas_mount::export_info() 
{
	FUNC(svas_mount::export_info);
	errlog->log(log_error,"everything is exported\n");
	RETURN 0;
} /* export_info */

// A simplified version for mount ops, which alway run in a separate xct.
how_to_end
svas_mount::mount_begin(svc_req *svcreq,struct authunix_parms *p)
{
	FUNC(svas_mount::begin);
	svas_mount		*server = ARGN2mount(svcreq);

	server->euid = server->_uid = p->aup_uid;
	server->egid = server->_gid = p->aup_gid;
	int ngroups = p->aup_len;
	int j = 0;
	for (int i=0; i<ngroups; i++)
		if (!group_to_xct(p->aup_gids[i]))
			server->groups[j++] = p->aup_gids[i];
	server->ngroups = ngroups;
	if(server->beginTrans() != SVAS_OK) {
		return abortit;
	} else {
		return commitit;
	}
}
svas_mount::~svas_mount()
{ 
	FUNC(svas_mount::~svas_mount);
	mountent *nxt;
	for (mountent *m = mounts; m; m = nxt) {
		nxt = m->next;
		delete [] m->host;
		delete [] m->fsname;
		delete m;
	}

}
