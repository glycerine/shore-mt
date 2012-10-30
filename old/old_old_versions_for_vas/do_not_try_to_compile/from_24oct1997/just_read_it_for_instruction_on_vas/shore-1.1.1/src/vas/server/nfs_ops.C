/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#define __malloc_h
#include <sys/types.h>
#include <rpc/types.h>
#include <rpc/rpc.h>
#include <stdlib.h>
#include <assert.h>
#include <debug.h>
#include <sys/stat.h>
#include "string.h"
#include "efs.h"
#include <errlog.h>

#include "svas_service.h"
#include "svas_layer.h"


static char *rcsid = "$Header: /p/shore/shore_cvs/src/vas/server/nfs_ops.C,v 1.31 1996/07/23 22:42:46 nhall Exp $";


#if !defined(SOLARIS2)
EXTERNC void svcerr_weakauth(SVCXPRT *xprt);
EXTERNC void svcerr_auth(SVCXPRT *xprt, enum auth_stat);
#else
EXTERNC void svcerr_weakauth(const SVCXPRT *xprt);
EXTERNC void svcerr_auth(const SVCXPRT *xprt, enum auth_stat);
#endif

struct authunix_parms *
credentials(svas_nfs *v, struct svc_req *rqstp) 
{
	// warning about alignment requirements
	struct authunix_parms *c =
		(struct authunix_parms *)rqstp->rq_clntcred;

	if (rqstp->rq_proc != NFSPROC_NULL &&
		(rqstp->rq_cred.oa_flavor != AUTH_UNIX || c == 0)) 
		{
			fprintf(stderr,"bad credentials(%d,0x%x)\n",
				rqstp->rq_cred.oa_flavor, credentials);

			v->errlog->log(log_error, "AUTH FAILED ");
			svcerr_weakauth(rqstp->rq_xprt);
			// return 0 means nfs_svc.c won't send any reply,
			// and the one svcerr_weakauth sent will be
			// the reply.
			return 0;
		}
	return c;
}
svas_nfs *
ARGN2nfs(void *v) 
{
	svas_server *s = ARGN2vas(v);
	return (svas_nfs *)s;
}

/*
// The following is to get a handle on the thread and vas structure.
// The way it was originally written, that was done only in efs.C,
// but we need to do so here to get the reply buf.
*/
#   undef FSTART
#	define FSTART(rtyp,fn,fn2)\
	char			*_fname_debug_ = #fn;\
	how_to_end		detachp=notset;\
	bool			ongoing = false;\
	svas_nfs		*server = ARGN2nfs(svcreq);\
	rtyp			*reply = &(server->get_nfs_replybuf()->_##rtyp);\
	nmsg_stats.inc((int)fn2);\
	DUMP(fn);\
	server->errlog->log(log_info, _fname_debug_); \
	errno = 0;

	
// This procedure is called at the start of each request.  It sets up
// the authorization context for the request and starts or resumes an xct.



//
// return 0 means nfs_svc.c won't send any reply, 
// We use it when
// svcerr_*() replied for us (authentication error)

#ifdef SOLARIS2
#define svcerr_rejectcred(xprt)  svcerr_auth(xprt, AUTH_REJECTEDCRED)
#endif

#define TRY(Stat,Op,Msg) \
	if ((Stat = (Op)) != NFS_OK) { \
		server->errlog->log(log_error, \
			Msg " FAILED - %s (%d)", strerror(Stat), Stat); \
		server->nfs_end(abortit, ongoing); \
		/* returning NULL causes no reply to be sent*/ \
		if(Stat == ETIMEDOUT) {\
			svcerr_rejectcred(svcreq->rq_xprt); \
			return 0;\
		} else {return reply;} \
	} else

/*********************** NFS *****************************************/
/********** nfs stats *******************************/

static const int nmsg_values[] = {
#include "nfs_stats.i"
	-1
};
static const char *nmsg_strings[] = {
#include "nfs_names.i"
	(char *)0
};

struct msg_info nmsg_names = {
	nmsg_values,
	nmsg_strings
};
// nmsg_stats is external so that vas can collect the stats
_msg_stats nmsg_stats("NFS RPCs", 0x000b0000,
	(int)NFSPROC_NULL , (int)NFSPROC_STATFS, nmsg_names);


void
cnmsg_stats()
{
    nmsg_stats.clear();
}
/*		void nfsproc_NULL_2(void) = 0; */
EXTERNC void 
*nfsproc_null_2(void *, svc_req *svcreq)
{
	FSTART(void,nfsproc_null_2,NFSPROC_NULL);
	server->errlog->log(log_info, "NULL\n");
	return (void *)1;
} /* nfsproc_null_2 */

#define CRED_CK\
	authunix_parms *cred = credentials(server, svcreq);\
	if(!cred) { return 0;}/*reply sent already*/ \
	detachp = server->nfs_begin(svcreq,cred); \
	ongoing = detachp == suspendit? true: false;

/*		attrstat nfsproc_GETATTR_2(nfs_fh) =	1; */
EXTERNC attrstat *
nfsproc_getattr_2(nfs_fh *argp, svc_req *svcreq)
{
	FSTART(attrstat,nfsproc_getattr_2,NFSPROC_GETATTR);
	server->errlog->log(log_info, "GETATTR(%s)...",server->efs_string(*argp));

	CRED_CK;

	dassert(detachp == abortit || detachp == commitit || detachp==suspendit);

	TRY(reply->status, server->efs_attrs(*argp, reply->attrstat_u.attributes),
		"efs_attrs(get)");

	server->errlog->log(log_info,"SUCCEEDED: %s\n", server->efs_string(*reply));
	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_getattr_2 */

/*		attrstat nfsproc_SETATTR_2(sattrargs) = 2; */
EXTERNC attrstat *
nfsproc_setattr_2(sattrargs *argp, svc_req *svcreq)
{
	FSTART(attrstat,nfsproc_setattr_2,NFSPROC_SETATTR);
	server->errlog->log(log_info,"SETATTR(%s)...",server->efs_string(argp->file));

	CRED_CK;
	dassert(detachp == abortit || detachp == commitit || detachp==suspendit);

	TRY(reply->status, server->efs_attrs(argp->file, argp->attributes),
		"efs_attrs(set)");
	TRY(reply->status, server->efs_attrs(argp->file, reply->attrstat_u.attributes),
		"efs_attrs(get)");

	server->errlog->log(log_info,"SUCCEEDED: %s\n", server->efs_string(*reply));
	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_setattr_2 */

/*		void nfsproc_ROOT_2(void) = 3; */
EXTERNC void *
nfsproc_root_2(void *, svc_req *svcreq)
{
	FSTART(void,nfsproc_root_2,NFSPROC_ROOT);
	server->errlog->log(log_info,"ROOT called!?!\n");
	return (void *)0;
} /* nfsproc_root_2 */

/*		diropres nfsproc_LOOKUP_2(diropargs) = 4; */
EXTERNC diropres *
nfsproc_lookup_2(diropargs *argp, svc_req *svcreq)
{
	FSTART(diropres,nfsproc_lookup_2,NFSPROC_LOOKUP);
	server->errlog->log(log_info,"LOOKUP(%s/%s)...",
		server->efs_string(argp->dir),argp->name);

	CRED_CK;
	dassert(detachp == abortit || detachp == commitit || detachp==suspendit);

	nfs_fh &child = reply->diropres_u.diropres.file;

#define QTRY(Stat,Op,Msg) \
	if ((Stat = (Op)) != NFS_OK) { \
		server->errlog->log(log_info, Msg " FAILED - %s (%d)", strerror(Stat), Stat); \
		server->errlog->log(log_info, "\n"); \
		server->nfs_end(abortit, ongoing); \
		/* returning NULL causes no reply to be sent */ \
		if(Stat == ETIMEDOUT) { \
			svcerr_rejectcred(svcreq->rq_xprt); \
			return 0;\
		} else return reply; \
	}
	QTRY(reply->status, server->efs_lookup(argp->dir, argp->name, child),"efs_lookup");
	// CHECKFH(child);

	DBG(<<"nfsproc_lookup_2: found ... get attrs now");

	//
	// Object was found but could be anonymous
	// or something that doesn't translate
	//
	TRY(reply->status, server->efs_attrs(child, reply->diropres_u.diropres.attributes),
		"efs_attrs(get)");

	DBG(<<"nfsproc_lookup_2");

	server->errlog->log(log_info,"SUCCEEDED: %s\n\t%s\n\t[%s]\n", server->efs_string(child),
 		server->efs_string(reply->diropres_u.diropres.attributes),
 		server->efs_string(reply->diropres_u.diropres.file)
		);
	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_lookup_2 */

/*		readlinkres nfsproc_READLINK_2(nfs_fh) = 5; */
EXTERNC readlinkres *
nfsproc_readlink_2(nfs_fh *argp, svc_req *svcreq)
{
	FSTART(readlinkres,nfsproc_readlink_2,NFSPROC_READLINK);
	server->errlog->log(log_info, "READLINK(%s)...", server->efs_string(*argp));

	CRED_CK;

	char *resbuf = (char *)malloc(NFS_MAXPATHLEN+1);
	if(!resbuf) {
		server->errlog->log(log_info, "READLINK_2 malloc failure");
		server->nfs_end(abortit, ongoing); 
	}
	ObjectSize bufsize = NFS_MAXPATHLEN;

	TRY(reply->status, server->efs_readlink(*argp, bufsize, resbuf),
		"efs_readlink");
	reply->readlinkres_u.data = resbuf;
	resbuf[bufsize] = 0;

	server->errlog->log(log_info,"SUCCEEDED: \"%s\"\n", resbuf);
	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_getattr_2 */

/*		readres nfsproc_READ_2(readargs) = 6; */
EXTERNC readres *
nfsproc_read_2(readargs *argp, svc_req *svcreq)
{
	FSTART(readres,nfsproc_read_2,NFSPROC_READ);
	server->errlog->log(log_info, "READ(%s, count %d, offset %d (0x%x)...",
		server->efs_string(argp->file), argp->count, argp->offset, argp->offset);

	CRED_CK;

	char *resbuf = (char *)malloc(NFS_MAXDATA);
	if(!resbuf) {
		server->errlog->log(log_info, "READ_2 malloc failure");
		server->nfs_end(abortit, ongoing); 
	}
	ObjectSize bufsize;


	bufsize = argp->count;
	if (bufsize > NFS_MAXDATA)
		bufsize = NFS_MAXDATA;
	TRY(reply->status, server->efs_read(argp->file, argp->offset, bufsize, resbuf),
		"efs_read");
	TRY(reply->status, server->efs_attrs(argp->file, reply->readres_u.reply.attributes),
		"efs_attrs(get)");
	reply->readres_u.reply.data.data_len = bufsize;
	reply->readres_u.reply.data.data_val = resbuf;

 	server->errlog->log(log_info,"SUCCEEDED (%d)\n", bufsize);

	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_read_2 */

/*		void nfsproc_WRITECACHE_2(void) = 7; */
EXTERNC void *
nfsproc_writecache_2(void *, svc_req *svcreq)
{
	FSTART(void,nfsproc_writecache_2,NFSPROC_WRITECACHE);
	server->errlog->log(log_info,"WRITECACHE called!?!\n");
	return (void *)0;
} /* nfsproc_writecache_2 */

/*		attrstat nfsproc_WRITE_2(writeargs) = 8; */
EXTERNC attrstat 
*nfsproc_write_2(writeargs *argp, svc_req *svcreq)
{
	FSTART(attrstat,nfsproc_write_2,NFSPROC_WRITE);

	server->errlog->log(log_info, "WRITE(%s, count %d, offset %d (0x%x)...",
		server->efs_string(argp->file), argp->data.data_len,
			argp->offset, argp->offset);

	CRED_CK;

	ObjectSize bufsize;

	bufsize = argp->data.data_len;
	if (bufsize > NFS_MAXDATA)
		bufsize = NFS_MAXDATA;

	TRY(reply->status, server->efs_write(argp->file, argp->offset, bufsize,
								argp->data.data_val),
		"efs_write");
	TRY(reply->status, server->efs_attrs(argp->file, reply->attrstat_u.attributes),
		"efs_attrs(get)");

 	server->errlog->log(log_info,"SUCCEEDED (%d)\n", bufsize);

	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_write_2 */

/*		diropres nfsproc_CREATE_2(createargs) = 9; */
EXTERNC diropres *
nfsproc_create_2(createargs *argp, svc_req *svcreq)
{
	FSTART(diropres,nfsproc_create_2,NFSPROC_CREATE);

	server->errlog->log(log_info, "CREATE(%s/%s)...",server->efs_string(argp->where.dir),
		argp->where.name);

	CRED_CK;

	ftype newtype;
	nfs_fh newfile;

	// This NFS protocol request is also generated by the mknod system
	// call, which creates special files (devices, named pipes, etc.)
	// and passes the device code in the size parameter.
	newtype = server->S_IF2NF(argp->attributes.mode);
	if(newtype == NFBAD) {
			server->errlog->log(log_error, "CREATE, weird mode 0%o\n",
				argp->attributes.mode);
			reply->status = NFSERR_IO; // ?? other error code ??
			server->nfs_end(abortit, ongoing); 
			return reply;
	}
	TRY(reply->status, server->efs_create(argp->where.dir, argp->where.name, newtype,
								argp->attributes, newfile),
		"efs_create");

	memcpy(&reply->diropres_u.diropres.file, &newfile, sizeof newfile);

	TRY(reply->status, server->efs_attrs(newfile, reply->diropres_u.diropres.attributes),
		"efs_attrs(get)");

 	server->errlog->log(log_info,"SUCCEEDED: %s\n\t%s\n\t[%s]\n",
 		server->efs_string(newfile),
 		server->efs_string(reply->diropres_u.diropres.attributes),
 		server->efs_string(reply->diropres_u.diropres.file));
	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_create_2 */

/*		nfsstat nfsproc_REMOVE_2(diropargs) = 10; */
EXTERNC nfsstat *
nfsproc_remove_2(diropargs *argp, svc_req *svcreq)
{
	FSTART(nfsstat,nfsproc_remove_2,NFSPROC_REMOVE);
	server->errlog->log(log_info,
		"REMOVE(%s/%s)...",server->efs_string(argp->dir),argp->name);

	CRED_CK;

	TRY(*reply, server->efs_remove(argp->dir, argp->name), "efs_remove");

 	server->errlog->log(log_info,"SUCCEEDED\n");
	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_remove_2 */

/*		nfsstat nfsproc_RENAME_2(renameargs) = 11; */
EXTERNC nfsstat *
nfsproc_rename_2(renameargs *argp, svc_req *svcreq)
{
	FSTART(nfsstat,nfsproc_rename_2,NFSPROC_RENAME);

	server->errlog->log(log_info,
		"RENAME(%s/%s",server->efs_string(argp->from.dir),argp->from.name);
	server->errlog->log(log_info,
		",%s/%s)\n",server->efs_string(argp->to.dir),argp->to.name);

	CRED_CK;

	TRY(*reply, server->efs_rename(argp->from.dir, argp->from.name,
			argp->to.dir, argp->to.name),
		"efs_rename");

 	server->errlog->log(log_info,"SUCCEEDED\n");

	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_rename_2 */

/*		nfsstat nfsproc_LINK_2(linkargs) = 12; */
EXTERNC nfsstat *
nfsproc_link_2(linkargs *argp, svc_req *svcreq)
{
	FSTART(nfsstat,nfsproc_link_2,NFSPROC_LINK);

	server->errlog->log(log_info,"LINK(%s/%s,",
		server->efs_string(argp->to.dir),argp->to.name);
	server->errlog->log(log_info,"%s)", server->efs_string(argp->from));

	CRED_CK;

	TRY(*reply, server->efs_link(argp->from, argp->to.dir, argp->to.name),
		"efs_link");

 	server->errlog->log(log_info,"SUCCEEDED\n");
	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_link_2 */

/*		nfsstat nfsproc_SYMLINK_2(symlinkargs) = 13; */
EXTERNC nfsstat *
nfsproc_symlink_2(symlinkargs * argp, svc_req *svcreq)
{
	FSTART(nfsstat,nfsproc_symlink_2,NFSPROC_SYMLINK);

	server->errlog->log(log_info,"SYMLINK(%s/%s,%s)",
		server->efs_string(argp->from.dir),argp->from.name, argp->to);

	CRED_CK;

	TRY(*reply, server->efs_symlink(argp->from.dir, argp->from.name,
						argp->attributes, argp->to),
		"efs_create");

 	server->errlog->log(log_info,"SUCCEEDED\n");
	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_symlink_2 */

/*		diropres nfsproc_MKDIR_2(createargs) = 14; */
EXTERNC diropres *
nfsproc_mkdir_2(createargs *argp, svc_req *svcreq)
{
	FSTART(diropres,nfsproc_mkdir_2,NFSPROC_MKDIR);

 	server->errlog->log(log_info, 
		"MKDIR(%s/%s)...",server->efs_string(argp->where.dir),
 		argp->where.name);

	CRED_CK;

	nfs_fh newdir;
	TRY(reply->status, server->efs_mkdir(argp->where.dir, argp->where.name,
								argp->attributes, newdir),
		"efs_mkdir");
	TRY(reply->status, server->efs_attrs(newdir, reply->diropres_u.diropres.attributes),
		"efs_attrs(get)");

	memcpy(&reply->diropres_u.diropres.file, &newdir, sizeof newdir);
	server->errlog->log(log_info,"SUCCEEDED: %s\n\t%s\n\t[%s]\n", server->efs_string(newdir),
	 	server->efs_string(reply->diropres_u.diropres.attributes),
	 	server->efs_string(reply->diropres_u.diropres.file));
	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_mkdir_2 */

/*		nfsstat nfsproc_RMDIR_2(diropargs) = 15; */
EXTERNC nfsstat *
nfsproc_rmdir_2(diropargs *argp, svc_req *svcreq)
{
	FSTART(nfsstat,nfsproc_rmdir_2,NFSPROC_RMDIR);
	server->errlog->log(log_info,
		"RMDIR(%s/%s)...",server->efs_string(argp->dir),argp->name);

	CRED_CK;

	TRY(*reply, server->efs_rmdir(argp->dir, argp->name), "efs_rmdir");

 	server->errlog->log(log_info,"SUCCEEDED\n");

	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_rmdir_2 */

/*		readdirres nfsproc_READDIR_2(readdirargs) = 16; */
EXTERNC readdirres *
nfsproc_readdir_2(readdirargs *argp, svc_req *svcreq)
{
	FSTART(readdirres,nfsproc_readdir_2,NFSPROC_READDIR);
	u_int cookie;
	memcpy(&cookie, argp->cookie, sizeof cookie);
	server->errlog->log(log_info, "READDIR(%s,cookie %d,count %d)...",
		server->efs_string(argp->dir), cookie, argp->count);

	CRED_CK;
	u_int count;
	count = argp->count;
	bool	eof;
	TRY(reply->status,
		server->efs_readdir(argp->dir, cookie, count,
			reply->readdirres_u.reply.entries, eof),
		"efs_readdir");
	reply->readdirres_u.reply.eof = eof;

	dassert(sizeof(reply->readdirres_u.reply.eof) == 1);
 	server->errlog->log(log_info,"SUCCEEDED (%seof)\n",
 		reply->readdirres_u.reply.eof ? "" : "not ");
	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_readdir_2 */

/*		statfsres nfsproc_STATFS_2(nfs_fh) = 17; */
EXTERNC statfsres *
nfsproc_statfs_2(nfs_fh *argp, svc_req *svcreq)
{
	FSTART(statfsres,nfsproc_statfs_2,NFSPROC_STATFS);
	server->errlog->log(log_info, "STATFS(%s)...",server->efs_string(*argp));

	CRED_CK;

	TRY(reply->status, server->efs_getfsattrs(*argp,reply->statfsres_u.reply),
		"efs_getfsattrs");
 	server->errlog->log(log_info, "SUCCEEDED:(tsize = %d bsize = %d, blocks = %d,"
 		" bfree = %d, bavail = %d)\n",
 		reply->statfsres_u.reply.tsize,
 		reply->statfsres_u.reply.bsize,
 		reply->statfsres_u.reply.blocks,
 		reply->statfsres_u.reply.bfree,
 		reply->statfsres_u.reply.bavail);
	server->nfs_end(detachp, ongoing);
	return reply;
} /* nfsproc_statfs_2 */

/*************************** MOUNT ***************************/

static const int mmsg_values[] = {
#include "mount_stats.i"
	-1
};
static const char *mmsg_strings[] = {
#include "mount_names.i"
	(char *)0
};
struct msg_info mmsg_names = {
	mmsg_values,
	mmsg_strings
};
// mmsg_stats is exported so that vas can collect the stats
_msg_stats mmsg_stats("MOUNTD RPCs", 0x000c0000,
	(int)MOUNTPROC_NULL , (int)MOUNTPROC_EXPORTALL,
mmsg_names);

void
cmmsg_stats()
{
    mmsg_stats.clear();
}

svas_mount *
ARGN2mount(void *v) 
{
	svas_server *s = ARGN2vas(v);
	return (svas_mount *)s;
}

#	undef FSTART
#	define FSTART(rtyp,fn,fn2)\
	char			*_fname_debug_ = #fn;\
	how_to_end		detachp;\
	bool			ongoing = false;\
	svas_mount		*server = ARGN2mount(svcreq);\
	rtyp			*reply = &(server->get_nfs_replybuf()->_##rtyp);\
	mmsg_stats.inc((int)fn2);\
	DUMP(fn);\
	server->errlog->log(log_info, _fname_debug_); \
	errno = 0;


/*		void mountproc_null(void) = 0; */
EXTERNC void *
mountproc_null_1(void *, svc_req *svcreq) 
{
	FSTART(void,mountproc_null_1,MOUNTPROC_NULL);
	server->errlog->log(log_info, "MOUNTD NULL\n");
	return (void *)1;
} /* mountproc_null_1 */

/*		fhstatus mountproc_mnt(dirpath) = 1; */
EXTERNC fhstatus *
mountproc_mnt_1(dirpath *argp, svc_req *svcreq)
{
	FSTART(fhstatus,mountproc_mnt_1,MOUNTPROC_MNT);
	server->errlog->log(log_info, "MNT(%s)...", *argp);

	CRED_CK;

	TRY(reply->fhs_status, 
		server->mnt(*argp, cred, reply->fhstatus_u.fhs_fhandle),
		"mnt");

 	server->errlog->log(log_info, "SUCCEEDED: %s\n",
 		server->efs_string(reply->fhstatus_u.fhs_fhandle));
	server->commitTrans();
	return reply;
} /* mountproc_mnt_1 */

/*	mountlist *mountproc_dump(void) = 2; */
EXTERNC mountlist *
mountproc_dump_1(void *, svc_req *svcreq)
{
	FSTART(mountlist,mountproc_dump_1,MOUNTPROC_DUMP);
	server->errlog->log(log_info, "DUMP()...");

	CRED_CK;

	mountlist result;
	result = server->mounted();
 	server->errlog->log(log_info, "SUCCEEDED:");

	if (result) {
		server->errlog->log(log_info,"\n");
		for (mountlist m=result; m; m = m->ml_next)
			server->errlog->log(log_info,"\t%s:%s\n",m->ml_hostname,m->ml_directory);
	} else {
		server->errlog->log(log_info, " (none)\n");
	}
	server->commitTrans();

	return reply;
} /* mountproc_dump_1 */

/*	void *mountproc_umnt_1(dirpath) = 3; */
EXTERNC void *
mountproc_umnt_1(dirpath *argp, svc_req *svcreq)
{
	FSTART(void,mountproc_umnt_1,MOUNTPROC_UMNT);
	server->errlog->log(log_info, "UMNT(%s)...", *argp);

	CRED_CK;

	server->unmount(cred, *argp);

 	server->errlog->log(log_info, "SUCCEEDED\n");
	server->commitTrans();
	return (void *)1;
} /* mountproc_umnt_1 */

/*	void *mountproc_umntall_1(void) = 4; */
EXTERNC void *
mountproc_umntall_1(void *, svc_req *svcreq)
{
	FSTART(void,mountproc_umntall_1,MOUNTPROC_UMNTALL);

	// CRED_CK;
	// want to log after cred check but before begin, here
	//
	authunix_parms *cred = credentials(server, svcreq);
	if(!cred) { return 0;}
	server->errlog->log(log_info, 
		"UMNTALL(%s,%d)...", cred->aup_machname, cred->aup_uid);
	detachp = server->mount_begin(svcreq,cred);
	ongoing = detachp == suspendit? true: false;

	server->unmount(cred, 0);

 	server->errlog->log(log_info, "SUCCEEDED\n");
	server->commitTrans();
	return (void *)1;
} /* mountproc_umntall_1 */

/*	exports *mountproc_export_1(void)  = 5; */
EXTERNC exports 
*mountproc_export_1(void *, svc_req *svcreq)
{
	FSTART(exports,mountproc_export_1,MOUNTPROC_EXPORT);
	server->errlog->log(log_info, "EXPORT()...");

	CRED_CK;

	*reply = server->export_info();

 	server->errlog->log(log_info, "SUCCEEDED\n");
	if (*reply) for (exports p = *reply; p; p = p->ex_next) {
		server->errlog->log(log_info, "%s exported to", p->ex_dir);
		if (p->ex_groups) for (groups g = p->ex_groups; g; g = g->gr_next)
			server->errlog->log(log_info, " %s", g->gr_name);
		else
			server->errlog->log(log_info, " everybody");
		server->errlog->log(log_info, "\n");
	} else {
		server->errlog->log(log_info, "no exports");
	}

	server->commitTrans();
	return reply;
} /* mountproc_export_1 */

/*	exports *mountproc_exportall_1(void) = 6; */
/*  same as mountproc_export_1 */
EXTERNC exports *
mountproc_exportall_1(void *, svc_req *svcreq)
{
	FSTART(exports,mountproc_exportall_1,MOUNTPROC_EXPORTALL);
	server->errlog->log(log_info, "EXPORTALL()...");

	CRED_CK;

	*reply = server->export_info();

 	server->errlog->log(log_info, "SUCCEEDED\n");
	if (*reply) for (exports p = *reply; p; p = p->ex_next) {
		server->errlog->log(log_info, "%s exported to", p->ex_dir);
		if (p->ex_groups) for (groups g = p->ex_groups; g; g = g->gr_next)
			server->errlog->log(log_info, " %s", g->gr_name);
		else
			server->errlog->log(log_info, " everybody");
		server->errlog->log(log_info, "\n");
	} else {
		server->errlog->log(log_info, "no exports");
	}

	server->commitTrans();
	return reply;
} /* mountproc_exportall_1 */
