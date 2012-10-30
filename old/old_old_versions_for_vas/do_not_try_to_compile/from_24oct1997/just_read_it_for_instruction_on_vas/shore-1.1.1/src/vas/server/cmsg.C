/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/cmsg.C,v 1.77 1997/01/24 16:47:47 nhall Exp $
 */
#include <copyright.h>

// GROT __MSG_C__-ism
typedef int sm_save_point_t;
typedef int	stid_t;

#define __MSG_C__
#define __malloc_h
#include <msg.h>
#include <vas_internal.h>
#include "vaserr.h"
#define __log__ ShoreVasLayer.client_service->log
#define MY_MSG_STATS cmsg_stats
#include "funcproto.h"

#define MALLOC_CHECK(xxx) if(((xxx)==0) && \
		(reply->status.vasresult = SVAS_FAILURE) && \
		(reply->status.vasreason = SVAS_MallocFailure))goto bad;

extern struct msg_info cmsg_names;
_msg_stats MY_MSG_STATS("Client RPCs", 0x00090000,
	(int)client_init , (int)gather_stats, cmsg_names);
extern u_int nfs_alloc_xct(); // defined in efs.C
extern void nfs_free_xct(); // defined in efs.C

void
ccmsg_stats() 
{
	MY_MSG_STATS.clear();
}

#define SHMOPAQUE2VEC(name) \
	vec_t 	name( ((argp->name.shmlen > 0)?\
		(unsigned char *)(vas->shm.base() + argp->name.shmoffset):\
		(unsigned char *)argp->name.opq.opaque_t_val)\
		,\
		((argp->name.shmlen > 0)?\
			argp->name.shmlen:\
			argp->name.opq.opaque_t_len)\
		);

#define OPAQUE2VEC(name)\
	vec_t	name(argp->name.opaque_t_val, (int) argp->name.opaque_t_len)

// data new-ed by NEWVEC gets deleted by rpc
#define NEWVEC(core) \
	char  *core##_ptr = new char[argp->core##_limit];\
	vec_t	core(core##_ptr, (int) argp->core##_limit);\
	reply->##core##.opaque_t_val = core##_ptr; \
	MALLOC_CHECK(core##_ptr);
	
static rpcSysProps   
convert_sysprops(
	IN(SysProps) s
) 
{
	rpcSysProps	r; // expect bogus warning: r used before set
#ifdef PURIFY
	if(purify_is_running()) {
		memset(&r, '\0', sizeof(r));
	}
#endif

#ifdef DEBUG
	int slen = 0; int rlen=0;
#	define ADDIT(x,y)\
	slen+=sizeof(s.x); rlen+=sizeof(r.y);

#else
#	define ADDIT(x,y)  
#endif

#define SYSP_CONVERT(_s,_r) ADDIT(_s,_r) r._r = s._s

	SYSP_CONVERT(tag,rpctag);
	switch(s.tag) {
	case KindAnonymous:
		SYSP_CONVERT(anon_pool,rpcanon_pool);
		break;
	case KindRegistered:
		SYSP_CONVERT(reg_nlink,rpcreg_nlink);
		SYSP_CONVERT(reg_mode,rpcreg_mode);
		SYSP_CONVERT(reg_uid,rpcreg_uid);
		SYSP_CONVERT(reg_gid,rpcreg_gid);
		SYSP_CONVERT(reg_atime,rpcreg_atime);
		SYSP_CONVERT(reg_mtime,rpcreg_mtime);
		SYSP_CONVERT(reg_ctime,rpcreg_ctime);
		break;

	case KindTransient:
		// server is returning an error
		// skip all the rest
		return r;

	default:
		dassert(0);
		break;
	}
	SYSP_CONVERT(volume,volume);
	SYSP_CONVERT(ref,ref);
	SYSP_CONVERT(type,type);
	SYSP_CONVERT(csize,csize);
	SYSP_CONVERT(hsize,hsize);
	SYSP_CONVERT(tstart,tstart);
	SYSP_CONVERT(nindex,nindex);
	return r;
}

void_reply *
czero_1(
	czero_arg *argp,
	void *clnt
)
{ 
	FSTART(void,czero);
	REPLY;
}

init_reply *
client_init_1(
	client_init_arg *argp,
	void *clnt)
{  
	VASResult x;
	FSTART(init,client_init); 

	__log__->log(log_info, "client_init  %d %d",
		argp->num_page_bytes, argp->num_lg_bytes);

	if( (x = vas->version_match(argp->protocol_version)) == SVAS_OK ) {
		x = vas->_init( argp->mode, argp->num_page_bytes, argp->num_lg_bytes);
	}
	reply->page_size = vas->page_size();
	reply->sockbufsize = vas->sockbufsize();

#ifdef JUNK
	dassert( reply->page_size == sm_page_size); 
	reply->lg_data_size = sm_lg_data_size;
	reply->sm_data_size = sm_sm_data_size;
#endif

	reply->num_page_bufs = vas->num_page_bufs();
	reply->num_lg_bufs = vas->num_lg_bufs();
	reply->shmid = (int) vas->shm.id();

	DBG(<< "sending shmid " << dec << reply->shmid
		<< " attached at " << ::hex ((unsigned int)vas->shm.base() )
		<< " size " << dec<<  vas->shm.size()
	);

#ifdef DEBUG
	vas->checkflags(false);
#endif

	DBG(<<""<<flushl);
	REPLY;
}

lvid_t_reply *
v_mkfs_1(
	v_mkfs_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(lvid_t,v_mkfs);
	lvid_t lvid;
	if(!argp->allocate_vid) {
		lvid = argp->lvid;
	} else {
		lvid.high = 0; lvid.low = 0;
		dassert(lvid_t_is_null(lvid));
	}

	__log__->log(log_info, 
		"mkfs  device len=%d, value=%s KB=%d allocvid=%d vid=%d.%d\n", 
		strlen(argp->unixdevice), argp->unixdevice, 
		argp->kbytes,
		argp->allocate_vid, lvid.high, lvid.low);

	x =  vas->mkfs(argp->unixdevice, argp->kbytes, lvid, &lvid);
	if(x==SVAS_OK) {
		reply->result = lvid;
	}
	REPLY;
}

void_reply *
v_rmfs_1(
	v_rmfs_arg *argp,
	void *clnt
)
{
	FSTART(void,v_rmfs);

	__log__->log(log_info, "v_rmfs  lvid=%d.%d\n", 
	argp->lvid.high, argp->lvid.low);

	vas->rmfs(argp->lvid);
	REPLY;
}

lvid_t_reply *
v_newvid_1( 
	v_newvid_arg *argp, 
	void *clnt
)
{
	FSTART(lvid_t,v_newvid); 

	lvid_t	n;

	__log__->log(log_info, "newvid\n" );
	(void) vas->newvid(&n);
	reply->result = n;
	REPLY;
}

getvol_reply *
getvol_1( 
	getvol_arg *argp, 
	void *clnt
)
{
	FSTART(getvol,getvol); 

	__log__->log(log_info, "getvol \n");

	/*
	// have to use malloc because
	// rpcgen frees this, and rpcgen uses free()
	*/
	if(argp->nentries > 0) {
		int			sz = sizeof(lvid_t) * argp->nentries;
		lvid_t		*list = (lvid_t *) malloc(sz);

		MALLOC_CHECK(list); 
#ifdef PURIFY
		if(purify_is_running()) {
			memset(list,'\0',sz);
		}
#endif

		{

			if(vas->volumes(argp->unixdevice, argp->nentries,
				list, &reply->nentries, &reply->nvols)==SVAS_OK) {

				dassert(argp->nentries > reply->nvols);
				// copy the results
				reply->buf.buf_val = list; // freed by rpcgen
				reply->buf.buf_len = reply->nvols;
			} else {
				reply->buf.buf_val = 0;
				reply->buf.buf_len = 0;
			}
		}
	} else {
		vas->status.vasresult = SVAS_FAILURE;
		vas->status.vasreason = SVAS_BadParam2;
	}

bad:
	REPLY;
}

lrid_t_reply *
v_volroot_1( 
	v_volroot_arg *argp, 
	void *clnt
)
{
	FSTART(lrid_t,v_volroot); 

	lrid_t	rootdir;

	__log__->log(log_info, "volroot  %d.%d\n", 
		argp->volume.high , argp->volume.low 
		);
	(void) vas->volroot(argp->volume, &rootdir);
	reply->result = rootdir;
	REPLY;
}

void_reply *
v_serve_1( 
	v_serve_arg *argp, 
	void *clnt
)
{
	FSTART(void,v_serve); 

	__log__->log(log_info, "v_serve  %s %s\n", 
		argp->unixdevice, argp->writable?"wr":"ro");
	(void) vas->serve(argp->unixdevice);
	REPLY;

}
void_reply *
v_unserve_1( 
	v_unserve_arg *argp, 
	void *clnt
)
{
	FSTART(void,v_unserve); 
	__log__->log(log_info, "v_unserve  %s\n", argp->unixdevice);
	(void) vas->unserve( argp->unixdevice);
	REPLY;
}

v_devices_reply *
v_devices_1( 
	v_devices_arg *argp, 
	void *clnt
)
{
	FSTART(v_devices,v_devices); 
	VASResult res;

	__log__->log(log_info, "devices  %d\n", argp->cookie);
	devid_t *devlist;
	Path *list;
	int	 listlen;
	Path p;
	struct path_dev_pair *it;
	int	 i,j,k;
	int  start = (int)argp->cookie;
	int	 totalsize=0;

	res = vas->_devices(&list, &devlist, &listlen);
	if(res==SVAS_OK) {
		totalsize = sizeof(*reply);
		reply->cookie = (Cookie)listlen;
		reply->buf.buf_val = (struct path_dev_pair *)malloc( 
			(listlen-start) * sizeof(struct path_dev_pair));

		// clear it so that reply free works ok
		// if we run into a malloc error below
		memset(reply->buf.buf_val,'\0',(listlen-start) 
				* sizeof(struct path_dev_pair));

		reply->buf.buf_len = (listlen-start);
		for(i=start,k=0; i<listlen; i++,k++) {
			// copy the string and device id

			p = list[i]; // p points to the source string
			j = strlen(p)+1;
			totalsize += j;

			// it is the destination path,devid pair
			it = &reply->buf.buf_val[k];

			// make a complete copy of the string
			it->path = (Path)malloc(j);
			MALLOC_CHECK(it->path); 
			strcpy((char *)it->path,p);
			//reply->buf.buf_val[k][j] = '\0';

			// copy the device id
			it->dev.dev = devlist[i].dev;
			it->dev.ino = devlist[i].id;
		//
		// TODO: check for size of result, stop and set more=true
		// if necessary
		}
	}
bad:
	REPLY;
}

v_listmounts_reply *
v_listmounts_1( 
	v_listmounts_arg *argp, 
	void *clnt
)
{
	FSTART(v_listmounts,v_listmounts); 
	VASResult res;

	__log__->log(log_info, "listmounts  %d\n", argp->cookie);

	lvid_t *targetlist;
	serial_t *dirlist;
	Path *fnamelist;
	int	 listlen;

	Path p;
	struct pmountinfo *it;
	int	 i,j,k;
	int  start = (int)argp->cookie;
	int	 totalsize=0;

	res = vas->_list_mounts(argp->volume, &dirlist, &fnamelist, &targetlist, &listlen );
	if(res==SVAS_OK) {
		totalsize = sizeof(*reply);
		reply->cookie = (Cookie)listlen;
		reply->buf.buf_val = (struct pmountinfo *)malloc( 
			(listlen-start) * sizeof(struct pmountinfo));

		// clear it so that reply free works ok
		// if we run into a malloc error below
		memset(reply->buf.buf_val,'\0',(listlen-start) 
				* sizeof(struct pmountinfo));

		reply->buf.buf_len = (listlen-start);
		for(i=start,k=0; i<listlen; i++,k++) {
			// copy the string fname

			p = fnamelist[i]; // p points to the source string
			j = strlen(p)+1;
			totalsize += j;

			// it is the destination pmountinfo
			it = &reply->buf.buf_val[k];

			// make a complete copy of the string
			it->fname = (Path)malloc(j);
			MALLOC_CHECK(it->fname); 
			strcpy((char *)it->fname,p);
			//reply->buf.buf_val[k][j] = '\0';

			// copy the target and the dir
			it->target = targetlist[i];
			it->dirserial = dirlist[i];
		//
		// TODO: check for size of result, stop and set more=true
		// if necessary
		}
	}
bad:
	REPLY;
}

void_reply *
v_format_1( 
	v_format_arg *argp, 
	void *clnt
)
{
	FSTART(void,v_format); 

	lvid_t	volid;

	__log__->log(log_info, "format  %s %d %d\n", 
		argp->unixdevice, argp->kbytes, argp->force);

	(void) vas->format(
		argp->unixdevice,
		argp->kbytes,
		argp->force
		);

	REPLY;
}

void_reply *
v_mount_1( 
	v_mount_arg *argp, 
	void *clnt
)
{
	/* PERSISTENT MOUNT */
	FSTART(void,v_mount); 

	lvid_t	volid;

	__log__->log(log_info, "v_mount  %d.%d on %s %s\n", 
		volid.high, volid.low,
		argp->mountpoint,
		argp->writable?"wr":"ro");

	(void) vas->mount(
		volid,
		argp->mountpoint,
		argp->writable
		);

	REPLY;
}

void_reply *
v_dismount_1( 
	v_dismount_arg *argp, 
	void *clnt
)
{ 
	FSTART(void,v_dismount); 

	if(argp->ispatch1) {
		__log__->log(log_info, "patch1 %s \n", argp->mountpoint);
		(void) vas->punlink(argp->mountpoint);

	} else if(argp->ispatch2) {
		__log__->log(log_info, "patch2 %d.%d \n", 
			argp->volume.high, argp->volume.low);
		(void) vas->punlink(argp->volume);

	} else {
		__log__->log(log_info, "dismount %s \n", argp->mountpoint);
		(void) vas->dismount(argp->mountpoint);

	}
	REPLY;
}

v_quota_reply *
v_quota_1( 
	v_quota_arg *argp, 
	void *clnt
)
{ 
	FSTART(v_quota,v_quota); 

	__log__->log(log_info, "quota %d.%d \n", 
			argp->volume.high, argp->volume.low);

	(void) vas->quota(argp->volume, &reply->kbquota, &reply->kbused);
	REPLY;
}

diskusage_reply *
diskusage_1(
	diskusage_arg *argp,
	void *clnt
)
{
	FSTART(diskusage,diskusage); 
	VASResult res;

	__log__->log(log_info, "diskusage_1  %d.%d \n", 
		argp->oid.lvid.high,
		argp->oid.lvid.low);

	memset(&reply->stats, '\0', sizeof(reply->stats));
	if(argp->for_volume) {
		res = vas->disk_usage(argp->oid.lvid, &reply->stats);
	} else {
		res = vas->disk_usage(argp->oid, argp->mbroot, &reply->stats);
	}
	if(res != SVAS_OK) {
		__log__->log(log_internal, "disk_usage\n");
	}
	REPLY;
}

statfs1_reply *
statfs1_1(
	statfs1_arg *argp,
	void *clnt
)
{ 
	FSTART(statfs1,statfs1); 

	__log__->log(log_info, "statfs_1  %d \n", 
		argp->volume.high, argp->volume.low);

	// Return file-system parameters
	reply->volume = argp->volume;
	FSDATA fsd;

	if(vas->statfs(argp->volume, &fsd) != SVAS_OK) {
		__log__->log(log_internal, "statfs1:\n");
	} else {
		reply->fsdata.tsize = fsd.bsize; // TODO: fix this
		reply->fsdata.bsize = fsd.bsize;

		// TODO: make sure this volume info is maintained.
		reply->fsdata.blocks = fsd.blocks; 
		reply->fsdata.bfree = fsd.bfree;
		reply->fsdata.bavail = fsd.bavail;
	}
	REPLY;
}

getmnt_reply *
getmnt_1(
	getmnt_arg *argp,
	void *clnt
)
{ 
	VASResult x=SVAS_OK;
	FSDATA	*space;
	Cookie	cookie;

	FSTART(getmnt,getmnt); 

	__log__->log(log_info, "getmnt %d\n",  argp->nbytes);

	// malloc instead of new because it gets freed instead of deleted
	assert(((int)argp->nbytes) >= 0);
	space = (FSDATA *)malloc(argp->nbytes); 
	MALLOC_CHECK(space); 
	cookie = argp->cookie;

	MALLOC_CHECK(space);
	{
		x= vas->getMnt( space, argp->nbytes, &reply->nentries, &cookie);
		reply->cookie = cookie;
		if(x==SVAS_OK) {
			// copy the results
			reply->buf.buf_val = space; // freed by rpcgen
			// it would be nice not to the whole thing...
			// but we have only an entry count, not a byte count
			reply->buf.buf_len = argp->nbytes;
		} else {
			reply->buf.buf_val = 0;
			reply->buf.buf_len = 0;
		}
	}
bad:
	DBG(<<"getMnt returned " << x
		<<" space=" << ::hex((unsigned int) reply->buf.buf_val)
		<<" nbytes=" << reply->buf.buf_len
		<<" cookie=" << reply->cookie
	);
	REPLY;
}

lookup_reply *
lookup1_1(
	lookup1_arg *argp,
	void *clnt
)
{	
	FSTART(lookup,lookup1); 
	__log__->log(log_info, "lookup1 %s\n",  argp->absolute);
	bool found=false;

	(void) vas->lookup(argp->absolute, &reply->result, &found,
		argp->perm, argp->follow);
	reply->found = found;
	__log__->log(log_info, "lookup1 found=%d\n",  reply->found);
	REPLY;
}

lookup_reply *
lookup2_1(
	lookup2_arg *argp,
	void *clnt
)
{	
	FSTART(lookup,lookup2); 
	__log__->log(log_info, "lookup2 %s\n",  argp->name);
	bool found=false;
	(void) vas->lookup(argp->name, &reply->result, &found, argp->perm);
	reply->found=found;
	__log__->log(log_info, "lookup1 found=%d\n",  reply->found);
	REPLY;
}

#define USE_COMMON_OBJMSG

common_objmsg_reply *
common_objmsg(
	enum		LgReadRequest reqtype,
	svas_server  	*vas,
	VASResult x,
	int		what,
	char 	*data_ptr,
	int		opaque_t_len,
	ObjectSize more,
	lrid_t snapped,
	SysProps *s
)
{
/* replacement for FSTART */
	char		*_fname_debug_ = "common_objmsg";
	/* reply can be static because we never block in this function */
	static common_objmsg_reply	_reply;
	common_objmsg_reply	*reply = &_reply;
	DUMP("common_objmsg_1");
	errno 		= 0;
/* end of FSTART */


#ifdef DEBUG
	assert( 
		((what & cor_read)==0 && (data_ptr == 0))
		||
		((what & cor_read)!=0 && (data_ptr != 0))
	);
	assert( 
		((what & cor_sysp)==0 && (s == 0))
		||
		((what & cor_sysp)!=0 && (s != 0))
	);
#endif
	// First of all, make sure the entire thing can be xdr-ed:
	reply->status = vas->status;
	reply->_u.tag = reqtype;
	reply->_u.repu_u._any.commonpart.data.opaque_t_len = 0;
	reply->_u.repu_u._any.commonpart.data.opaque_t_val = NULL;
	reply->_u.repu_u._any.commonpart.sent_small_obj_page = false;
	reply->_u.repu_u._any.commonpart.obj_follows_bytes =  0;
	reply->_u.repu_u._any.commonpart.more =  more;

	reply->_u.repu_u._any.commonpart.snapped = snapped;

	reply->_u.repu_u._stat1.sysprops.rpctag = KindTransient; // for rpc

	if(x!=SVAS_OK) {
#ifdef DEBUG
		if(reply->_u.repu_u._any.commonpart.data.opaque_t_len!=0)
		dassert(reply->_u.repu_u._any.commonpart.data.opaque_t_val!=NULL);
#endif
		return reply;
	}

	// readObj copies into data vector if large, into
	// small-page_buf if small anonymous

	switch(vas->xfercase()) {
	case svas_base::case_a: // case_a = vf_sm_page|vf_wire,
		assert(what & cor_page);

#		ifdef DEBUG
		vas->checkflags(true);
		dassert(vas->num_page_bufs() > 0);
#		endif

		reply->_u.repu_u._any.commonpart.sent_small_obj_page = true; // only one
		reply->_u.repu_u._any.commonpart.obj_follows_bytes = 0; 

		assert(vas->shm.base() == NULL);

		// sending exactly one page
		reply->_u.repu_u._any.commonpart.data.opaque_t_len = vas->page_size();
		reply->_u.repu_u._any.commonpart.data.opaque_t_val = vas->replace_page_buf();

		DBG( << "COPYING WHOLE PAGE to " << 
			::hex((unsigned int)reply->_u.repu_u._any.commonpart.data.opaque_t_val ));

		DBG( << "new buf is " << ::hex((unsigned int) 
				vas->page_buf()));
		break;

	case svas_base::case_b: // case_b = vf_sm_page|vf_shm,
		assert(what & cor_page);
#		ifdef DEBUG
		vas->checkflags(true);
		dassert(vas->num_page_bufs() > 0);
#		endif

		reply->_u.repu_u._any.commonpart.sent_small_obj_page = true; // only one
		reply->_u.repu_u._any.commonpart.obj_follows_bytes = 0; 

		reply->_u.repu_u._any.commonpart.data.opaque_t_len = 0;
		reply->_u.repu_u._any.commonpart.data.opaque_t_val = NULL;

		// don't need to do anything because the page
		// is already in shm
		assert(vas->shm.base() != NULL);
		break;

	case svas_base::case_c: // case_c = vf_obj_follows|vf_wire,
		assert(what & cor_read);
#		ifdef DEBUG
		vas->checkflags(true);
#		endif

		// data were copied to the given vector,
		// whatever that was

		reply->_u.repu_u._any.commonpart.sent_small_obj_page = false;
		reply->_u.repu_u._any.commonpart.obj_follows_bytes = opaque_t_len;
		// opaque_t_len was set by readObj

		reply->_u.repu_u._any.commonpart.data.opaque_t_val = data_ptr;
		reply->_u.repu_u._any.commonpart.data.opaque_t_len = opaque_t_len;
			// data ptr was dynamically allocated
		break;

	case svas_base::case_d: // case_d = vf_obj_follows|vf_shm,
		assert(what & cor_read);
#		ifdef DEBUG
		vas->checkflags(true);
#		endif

		// data were copied to the given vector,
		// and the object length was written  opaque_t_len

		reply->_u.repu_u._any.commonpart.sent_small_obj_page = false;
		reply->_u.repu_u._any.commonpart.obj_follows_bytes = opaque_t_len;

		reply->_u.repu_u._any.commonpart.data.opaque_t_len = 0;
		reply->_u.repu_u._any.commonpart.data.opaque_t_val = NULL;
			break;

	case svas_base::case_e: // case_e = vf_pseudo -- should not get here
		assert(0);
		break;
		
	default:
#ifdef DEBUG
		vas->checkflags(false);
#endif
		assert(what & cor_sysp);
		// legitimate case for sysprops()
		reply->_u.repu_u._any.commonpart.sent_small_obj_page = false; // only one
		reply->_u.repu_u._any.commonpart.obj_follows_bytes = 0;
		reply->_u.repu_u._any.commonpart.data.opaque_t_len=0;
		reply->_u.repu_u._any.commonpart.data.opaque_t_val=NULL;
	} /* end switch */

#ifdef DEBUG
	if(what&cor_read) vas->checkflags(true);
#endif

	vas->clrflags(svas_base::vf_sm_page|svas_base::vf_obj_follows);
	vas->setflags(svas_base::vf_no_xfer);

#ifdef DEBUG
	vas->checkflags(false);
    if(reply->_u.repu_u._any.commonpart.data.opaque_t_len!=0)
                dassert(reply->_u.repu_u._any.commonpart.data.opaque_t_val!=NULL);
#endif

	if(what & cor_sysp) {
		dassert(s);
		reply->_u.repu_u._stat1.sysprops = convert_sysprops(*s);
	}
	reply->status = vas->status;
	return reply;
}

// for anonymous objects
rpcSysProps_reply *
stat1_1(
	stat1_arg *argp,
	void *clnt
)
{
	FSTART(rpcSysProps,stat1); 
	SysProps	s;
	VASResult x;
	unsigned int what = cor_sysp;
	lrid_t	dummy;

	memset(reply,'\0',sizeof(*reply));

#	ifdef DEBUG
	vas->checkflags(false);
#	endif

	if(argp->copypage) what |= cor_page;
	// stat1 never tries to read the page

#	ifdef DEBUG
	vas->checkflags(false);
#	endif

	s.tag = KindTransient;

#ifdef DEBUG
	vas->checkflags(false);
#endif

	// tell vas what set of pages to use.
	if((x = vas->use_page(argp->pageoffset))== SVAS_OK) {

#		ifdef DEBUG
		vas->checkflags(false);
#		endif

		x =  vas->sysprops(argp->obj, &s, argp->copypage, argp->lock,
			/* don't care if it's a unixfile */ 0, &reply->sysp_size);
	}
#ifndef USE_COMMON_OBJMSG
	// TODO: remove this after testing common_objmsg()
	if(x==SVAS_OK) {

		switch(vas->xfercase()) {
		case svas_base::case_a: // case_a = vf_sm_page|vf_wire,
#ifdef DEBUG
			vas->checkflags(true);
#endif
			dassert(vas->num_page_bufs() > 0);
			reply->sent_small_obj_page = true; // only one
			assert(vas->shm.base() == NULL);

			// sending exactly one page
			reply->data.opaque_t_len = vas->page_size();
			reply->data.opaque_t_val = vas->replace_page_buf();

			if(reply->data.opaque_t_len!=0)
				dassert(reply->data.opaque_t_val!=NULL);

			DBG( << "COPYING WHOLE PAGE to " << 
			::hex((unsigned int)reply->data.opaque_t_val ));

			DBG( << "new buf is " << ::hex((unsigned int) 
				vas->page_buf()));
			break;

		case svas_base::case_b: // case_b = vf_sm_page|vf_shm,

#ifdef DEBUG
			vas->checkflags(true);
#endif
			dassert(vas->num_page_bufs() > 0);
			reply->sent_small_obj_page = true; // only one
			reply->data.opaque_t_len = 0;
			reply->data.opaque_t_val = NULL;

			// don't need to do anything because the page
			// is already in shm
			assert(vas->shm.base() != NULL);
			break;

		case svas_base::case_c: // not legit
		case svas_base::case_d: // not legit
			assert(0);
			break;

		default: // legit case for sysprops() is none of the "case_?"s
			// nothing to do yet.
#ifdef DEBUG
			vas->checkflags(false);
#endif
			reply->sent_small_obj_page = false; // only one
			reply->sysprops = convert_sysprops(s);
			reply->data.opaque_t_len=0;
			reply->data.opaque_t_val=NULL;
			break;

		}
		vas->clrflags(svas_base::vf_sm_page|svas_base::vf_obj_follows);
		vas->setflags(svas_base::vf_no_xfer);
#ifdef DEBUG
		vas->checkflags(false);
#endif
	} else {
		reply->data.opaque_t_len = 0;
		reply->data.opaque_t_val = NULL;
		reply->sent_small_obj_page = false;
		s.tag = KindTransient; // for rpc 
	}
	if(reply->data.opaque_t_len!=0)
				dassert(reply->data.opaque_t_val!=NULL);
	reply->sysprops = convert_sysprops(s);
#else
	*reply = (common_objmsg(Stat1Req, vas,x,what,0,
		0,0,dummy,&s))->_u.repu_u._stat1;
#endif

	REPLY;
}

// for anonymous objects
rpcSysProps_reply *
stat2_1(
	stat2_arg *argp,
	void *clnt
)
{
	FSTART(rpcSysProps,stat2); 
	SysProps	s;

	memset(reply, '\0', sizeof(*reply));
	s.tag = KindTransient;
	(void) vas->sysprops(argp->name, &s);
	reply->sysprops = convert_sysprops(s);
	REPLY;
}

getdirentries1_reply *
getdirentries1_1(
	getdirentries1_arg *argp,
	void *clnt
)
{  
	VASResult x;
	char	*space;
	FSTART(getdirentries1,getdirentries1); 

	// malloc instead of new because it gets freed instead of deleted
	assert(((int)argp->numbytes) >= 0);
	space = (char *)malloc(argp->numbytes);
	MALLOC_CHECK(space);

#ifdef PURIFY
	if(purify_is_running()) {
		memset(space, '\0', argp->numbytes);
	}
#endif
	{
		reply->cookie = argp->cookie;

		x= vas->getDirEntries( argp->dir, space, argp->numbytes, 
				&reply->numentries, &reply->cookie);
		if(x==SVAS_OK) {
			// copy the results
			reply->buf.buf_val = space;  // gets freed by rpc
			reply->buf.buf_len = (u_int) argp->numbytes;
		} else {
			reply->buf.buf_val = 0;
			reply->buf.buf_len = 0;
		}
	}
bad:
	REPLY;
}

void_reply *
rmdir1_1(
	rmdir1_arg *argp,
	void *clnt
)
{  
	VASResult x;
	FSTART(void,rmdir1); 

	x = vas->rmDir( argp->name );
	REPLY;
}

lrid_t_reply *
mkdir1_1(
	mkdir1_arg *argp,
	void *clnt
)
{  
	VASResult x;
	FSTART(lrid_t,mkdir1); 

	x = vas->mkDir( argp->name, argp->mode, &reply->result );
	REPLY;
}

lrid_t_reply *
mkpool_1(
	mkpool_arg *argp,
	void *clnt)
{  
	VASResult x;
	FSTART(lrid_t,mkpool); 

	DBG(
		<< "mkpool_1 name is " << argp->name
	)
	x = vas->mkPool( argp->name, argp->mode, &reply->result);
	REPLY;
}

void_reply *
rmpool_1(
	rmpool_arg *argp,
	void *clnt)
{  
	VASResult x;
	FSTART(void,rmpool); 

	x = vas->rmPool( argp->name );
	REPLY;
}


lrid_t_reply *
chdir1_1(
	chdir1_arg *argp,
	void *clnt)
{  
	VASResult x;
	FSTART(lrid_t,chdir1); 
	if(argp->use_path) {
		x = vas->_chDir(argp->path, &reply->result);
	} else {
		x = vas->_chDir(argp->dir);
	}
	REPLY;
}

tid_t_reply *
begintrans_1(
	begintrans_arg *argp,
	void *clnt
)
{ 
	VASResult x;
	tid_t   tid;
	tid.lo = 0; tid.hi = 0;
	FSTART(tid_t,begintrans); 

	/*
	// do wait for locks (default)
	// (no-wait mode isn't implemented)
	*/
	if(argp->unix_directory_service) {
		vas->use_unix_directory_service();
		// directory service reverts
		// to default on abort or commit
	} else {
		vas->no_unix_directory_service();
	}
	x = vas->beginTrans(argp->degree, &tid);
	reply->result = tid;
	REPLY;
}

void_reply *
abort1_1(
	abort1_arg *argp,
	void *clnt
)
{ 
	VASResult x;
	FSTART(void,abort1); 

	x = vas->abortTrans(*argp,SVAS_UserAbort);
	REPLY;
}

tid_t_reply *
commit_1(
	commit_arg *argp,
	void *clnt
)
{ 
	VASResult x;
	FSTART(tid_t,commit); 

	x = vas->commitTrans(argp->chain);
	(void) vas->trans(&reply->result);
	REPLY;
}

u_int_reply *
nfs_begintrans_1(
	nfs_begintrans_arg *argp,
	void *clnt
)
{ 
	tid_t   tid;
	FSTART(u_int,nfs_begintrans); 

#if 0
	/*
	// 
	// use unix directory service
	// don't wait for locks 
	//
	*/
	vas->beginTrans(3, &tid);
	reply->result = nfs_alloc_xct();
	if (reply->result==0) // no space?
		vas->abortTrans(tid);
#else
	cerr << "NFS-begin no longer supported." <<endl;
	reply->result = 0;
#endif
	REPLY;
}

void_reply *
nfs_abort1_1(
	nfs_abort1_arg *argp,
	void *clnt
)
{ 
	VASResult x;
	FSTART(void,nfs_abort1); 

	// TODO: use arg?
	x = vas->abortTrans();
	nfs_free_xct();
	REPLY;
}

void_reply *
nfs_commit_1(
	nfs_commit_arg *argp,
	void *clnt
)
{ 
	VASResult x;
	FSTART(void,nfs_commit); 

	// TODO: use arg?
	x = vas->commitTrans();
	nfs_free_xct();
	REPLY;
}

#ifdef notdef
void_reply *
resume_1(
	resume_arg *argp,
	void *clnt
)
{ 
	VASResult x;
	FSTART(void,resume); 

	x = vas->resumeAction(*argp);
	REPLY;
}
void_reply *
suspend_1(
	suspend_arg *argp,
	void *clnt
)
{ 
	VASResult x;
	FSTART(void,suspend); 

	x = vas->suspendAction(*argp);
	REPLY;
}
#endif

readobj_reply *
readobj_1(
	readobj_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(readobj,readobj); 

	unsigned int what = cor_read | cor_page;
	// readobj always tries to send the whole page

	// NEWVEC(data);
			// allocates space the size of the limit the user
			// sent; client never sends more than page_size()
			// at a time (for now)
			// data new-ed by NEWVEC gets deleted by rpc

	char  *data_ptr = vas->replace_lg_buf();
	vec_t	data(data_ptr, (int) argp->data_limit);

#ifdef DEBUG
	vas->checkflags(false);
#endif
	reply->commonpart.data.opaque_t_len = 0; // in case of error

	// tell vas what set of pages to use.
	if((x = vas->use_page(argp->pageoffset))== SVAS_OK) {
		x = vas->readObj(argp->obj, argp->start, argp->end, argp->lock,
			data, 
			(ObjectSize *)&reply->commonpart.data.opaque_t_len, 
			&reply->commonpart.more, &reply->commonpart.snapped);
	}

#ifndef USE_COMMON_OBJMSG
	// TODO: remove this after testing common_objmsg()
	if(x==SVAS_OK) {

		// readObj copies into data vector if large, into
		// small-page_buf if small anonymous

#ifdef DEBUG
		vas->checkflags(true);
#endif

		switch(vas->xfercase()) {
		case svas_base::case_a: // case_a = vf_sm_page|vf_wire,

			dassert(vas->num_page_bufs() > 0);
			reply->sent_small_obj_page = true; // only one
			reply->obj_follows_bytes = 0; 

			assert(vas->shm.base() == NULL);

			// sending exactly one page
			reply->data.opaque_t_len = vas->page_size();
			reply->data.opaque_t_val = vas->replace_page_buf();

			DBG( << "COPYING WHOLE PAGE to " << 
			::hex((unsigned int)reply->data.opaque_t_val ));

			DBG( << "new buf is " << ::hex((unsigned int) 
				vas->page_buf()));
			break;

		case svas_base::case_b: // case_b = vf_sm_page|vf_shm,
			dassert(vas->num_page_bufs() > 0);
			reply->sent_small_obj_page = true; // only one
			reply->obj_follows_bytes = 0; 

			reply->data.opaque_t_len = 0;
			reply->data.opaque_t_val = NULL;

			// don't need to do anything because the page
			// is already in shm
			assert(vas->shm.base() != NULL);
			break;

		case svas_base::case_c: // case_c = vf_obj_follows|vf_wire,
			// data were copied to the given vector,
			// whatever that was
			reply->sent_small_obj_page = false;
			reply->obj_follows_bytes = reply->data.opaque_t_len;
			// reply->data.opaque_t_len was set by readObj

			reply->data.opaque_t_val = data_ptr;
				// data ptr was dynamically allocated
			break;

		case svas_base::case_d: // case_d = vf_obj_follows|vf_shm,
			// data were copied to the given vector,
			// and the object length was written  to data.opaque_t_len

			reply->sent_small_obj_page = false;
			reply->obj_follows_bytes = 
				reply->data.opaque_t_len;

			reply->data.opaque_t_len = 0;
			reply->data.opaque_t_val = NULL;
				break;

		case svas_base::case_e: // case_e = vf_pseudo -- should not get here
		default:
			assert(0);
		}
#ifdef DEBUG
		vas->checkflags(true);
#endif
		vas->clrflags(svas_base::vf_sm_page|svas_base::vf_obj_follows);
		vas->setflags(svas_base::vf_no_xfer);
#ifdef DEBUG
		vas->checkflags(false);
#endif
	} else {
		reply->data.opaque_t_len = 0;
		reply->data.opaque_t_val = NULL;
		reply->sent_small_obj_page = false;
		reply->obj_follows_bytes =  0;
	}
#else
	*reply = (common_objmsg(ReadReq, 
		vas, x, what, data_ptr,
		(ObjectSize)reply->commonpart.data.opaque_t_len, 
		reply->commonpart.more, 
		reply->commonpart.snapped,
		0))->_u.repu_u._read; 
#endif

bad:
	REPLY;
}

void_reply *
appendobj_1(
	appendobj_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(void,appendobj); 
	SHMOPAQUE2VEC(newdata);
	if(argp->ctstart == assign) {
		x = vas->appendObj(argp->obj, newdata, argp->tstart);
	} else {
		x = vas->appendObj(argp->obj, newdata);
	}
	REPLY;
}

void_reply *
writeobj_1(
	writeobj_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(void,writeobj); 
	DBG(
		<<"offset="<<argp->newdata.shmoffset
		<<"len="<<argp->newdata.shmlen
		<<"addr="<<
			::hex((unsigned int)(argp->newdata.shmoffset + vas->shm.base()))
		);

	SHMOPAQUE2VEC(newdata);
	x = vas->writeObj(argp->obj, argp->objoffset, newdata);
	REPLY;
}

void_reply *
updateobj1_1(
	updateobj1_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(void,updateobj1); 
	SHMOPAQUE2VEC(wdata);
	SHMOPAQUE2VEC(adata);
	x = vas->updateObj(argp->obj, argp->objoffset, wdata, 
		 0, // ignored on server side
		 adata, argp->newtstart);
	REPLY;
}
void_reply *
updateobj2_1(
	updateobj2_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(void,updateobj2); 
	SHMOPAQUE2VEC(wdata);
	x = vas->updateObj(argp->obj, argp->objoffset, wdata, 
		argp->newlen, argp->newtstart);
	REPLY;
}

void_reply *
truncobj_1(
	truncobj_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(void,truncobj); 

	if(argp->ctstart == assign) {
		x = vas->truncObj(argp->obj, argp->to_length,
			argp->tstart);
	} else {
		x = vas->truncObj(argp->obj, argp->to_length);
	}
	REPLY;
}

batched_req_reply *
batched_req_1 (
	batched_req_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(batched_req,batched_req); 

	reply->count = 0;
	reply->list.list_len = argp->list.list_len;

	int i;
	batch_req *b = argp->list.list_val;

	reply->list.list_len = argp->list.list_len;
	reply->list.list_val = new batch_reply[reply->list.list_len];

	batch_reply *res = reply->list.list_val;

	MALLOC_CHECK(res);
	{
		void_reply *r=0;
		for(i = 0; i< (int)argp->list.list_len; 
			i++, b++, res++) {

			DBG(<<"tag is " << b->tag);
			switch(b->tag) {
			case MkAnon3Req: {
				mkanonymous3_arg *arg = &b->batch_req_u._mkanonymous3;
				r= mkanonymous3_1(arg,clnt);
				res->oid.lvid = arg->poolobj.lvid;
				res->oid.serial = arg->ref;
				} break;
			case MkAnon5Req: {
				mkanonymous5_arg *arg = &b->batch_req_u._mkanonymous5;
				r= mkanonymous5_1(arg,clnt);
				res->oid.lvid = arg->poolobj.lvid;
				res->oid.serial = arg->ref;
				} break;
			case Update1Req: {
				updateobj1_arg *arg = &b->batch_req_u._updateobj1;
				r= updateobj1_1(arg,clnt);
				res->oid = arg->obj;
				} break;
			case Update2Req: {
				updateobj2_arg *arg = &b->batch_req_u._updateobj2;
				r= updateobj2_1(arg,clnt);
				res->oid = arg->obj;
				} break;
			case TruncReq: {
				truncobj_arg *arg = &b->batch_req_u._truncobj;
				r= truncobj_1(arg,clnt);
				res->oid = arg->obj;
				} break;
			case AppendReq: {
				appendobj_arg *arg = &b->batch_req_u._appendobj;
				r= appendobj_1(arg,clnt);
				res->oid = arg->obj;
				} break;
			case WriteReq: {
				writeobj_arg *arg = &b->batch_req_u._writeobj;
				r= writeobj_1(arg,clnt);
				res->oid = arg->obj;
				} break;
			default:
				assert(0);
			}
			res->req=b->tag;
			res->status=r->status;
			DBG(<<"request="<<res->req);
			DBG(<<"status="<<res->status.vasresult);
			DBG(<<"oid.serial="<<res->oid.serial.data._low);

			reply->count++; // do this here because we
					// want it incremented even if error

			if(res->status.vasresult != SVAS_OK) {
				DBG("found error--stop processing batch");
				break;
			}
		}
	}
bad:
	REPLY;
}

void_reply *
mklink_1(
	mklink_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(void,mklink); 
	x = vas->mkLink(argp->oldpath, argp->newpath);
	REPLY;
}

void_reply *
rename1_1(
	rename1_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(void,rename1); 
	x = vas->reName(argp->oldpath, argp->newpath);
	REPLY;
}

lrid_t_reply *
mksymlink_1(
	mksymlink_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(lrid_t,mksymlink); 
	x = vas->mkSymlink(argp->name, argp->contents,
		argp->mode, &reply->result);
	REPLY;
}

lrid_t_reply *
mkxref_1(
	mkxref_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(lrid_t,mkxref); 
	x = vas->mkXref(argp->name, argp->mode, argp->object,
		&reply->result);
	REPLY;
}

lrid_t_reply *
readxref_1(
	readxref_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(lrid_t,readxref); 
	x = vas->readRef(argp->name, &reply->result);
	REPLY;
}
lrid_t_reply *
readxref2_1(
	readxref2_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(lrid_t,readxref2); 
	x = vas->readRef(argp->object, &reply->result);
	REPLY;
}

#ifdef notdef
void_reply *
rmxref_1(
	rmxref_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(void,rmxref); 
	x = vas->rmXref(argp->name);
	REPLY;
}
#endif
lrid_t_reply *
mkregistered2_1(
	mkregistered2_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(lrid_t,mkregistered2); 
	x = vas->mkRegistered(argp->name, argp->mode, argp->type,
		argp->csize, argp->hsize, argp->tstart, argp->nindexes, &reply->result);

	REPLY;
}
lrid_t_reply *
mkregistered_1(
	mkregistered_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(lrid_t,mkregistered); 
	SHMOPAQUE2VEC(core);
	SHMOPAQUE2VEC(heap);

	x = vas->mkRegistered(argp->name, argp->mode, argp->type,
		core, heap, argp->tstart, argp->nindexes, &reply->result);

	REPLY;
}

#ifdef notdef
mkanon_reply *
mkanonymous1_1(
	mkanonymous1_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(mkanon,mkanonymous1); 
	SHMOPAQUE2VEC(core);
	SHMOPAQUE2VEC(heap);

	reply->result.serial = argp->ref;
	x = vas->mkAnonymous(argp->poolname, argp->type,
		core, heap, argp->tstart, argp->nindexes, &reply->result,
		&reply->pooloid);

	REPLY;
}
#endif /* notdef */

mkanon_reply *
mkanonymous2_1(
	mkanonymous2_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(mkanon,mkanonymous2); 
	SHMOPAQUE2VEC(core);
	SHMOPAQUE2VEC(heap);

	reply->result.serial = argp->ref;
	// get lvid for result from the pool lvid
	reply->result.lvid = argp->poolobj.lvid;

	x = vas->mkAnonymous(argp->poolobj, argp->type,
		core, heap, argp->tstart, argp->nindexes, &reply->result);

#ifdef DEBUG
	if( (argp->ref.data._low != 0x1) && (argp->ref.data._low != 0)){
		dassert(reply->result.serial.data._low == argp->ref.data._low);
	}
#endif
	REPLY;
}

mkanon_reply *
mkanonymous4_1(
	mkanonymous4_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(mkanon,mkanonymous4); 

	reply->result.serial = argp->ref;
	// get lvid for result from the pool lvid
	reply->result.lvid = argp->poolobj.lvid;

	x = vas->mkAnonymous(argp->poolobj, argp->type,
		argp->csize, argp->hsize, argp->tstart, argp->nindexes, &reply->result);

	REPLY;
}

void_reply *
mkanonymous5_1(
	mkanonymous5_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(void,mkanonymous5); 

	lrid_t			oid;
	oid.serial = argp->ref;
	oid.lvid = argp->poolobj.lvid;

	x = vas->mkAnonymous(argp->poolobj, argp->type,
		argp->csize, argp->hsize, argp->tstart, argp->nindexes, oid);

	REPLY;
}

void_reply *
mkanonymous3_1(
	mkanonymous3_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(void,mkanonymous3); 
	SHMOPAQUE2VEC(core);
	SHMOPAQUE2VEC(heap);

	lrid_t			oid;
	oid.serial = argp->ref;
	oid.lvid = argp->poolobj.lvid;

	x = vas->mkAnonymous(argp->poolobj, argp->type,
		core, heap, argp->tstart, argp->nindexes, oid);

	REPLY;
}

#ifdef notdef
void_reply *
rmsymlink_1(
	rmsymlink_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(void,rmsymlink); 
	x = vas->rmSymlink(argp->name);
	REPLY;
}
#endif

rmlink1_reply *
rmlink1_1(
	rmlink1_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(rmlink1,rmlink1); 
	bool	must_remove=false;
	x = vas->rmLink1(argp->name, &reply->obj, &must_remove);
	reply->must_remove = must_remove;
	REPLY;
}

void_reply *
rmlink2_1(
	rmlink2_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(void,rmlink2); 
	x = vas->rmLink2(*argp);
	REPLY;
}

lrid_t_reply *
rmanonymous_1(
	rmanonymous_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(lrid_t,rmanonymous); 
	x = vas->rmAnonymous(*argp, &reply->result);
	REPLY;
}

void_reply *
addindex1_1(
	addindex1_arg *argp, 
	void *clnt
)
{
	FSTART(void,addindex1); 
	VASResult x;
	x = vas->addIndex(argp->iid, argp->kind);
	REPLY;
}
void_reply *
dropindex1_1(
	dropindex1_arg *argp, 
	void *clnt
)
{
	FSTART(void,dropindex1); 
	VASResult x;
	x = vas->dropIndex(argp->iid);
	REPLY;
}
statindex_reply *
statindex1_1(
	statindex1_arg *argp, 
	void *clnt
)
{
	FSTART(statindex,statindex1); 
	VASResult x;
	x = vas->statIndex(argp->iid, &reply->result);
	REPLY;
}


void_reply *
inserta_1(
	inserta_arg *argp, 
	void *clnt
)
{
	FSTART(void,inserta); 
	VASResult x;
	OPAQUE2VEC(key);
	OPAQUE2VEC(value);

	x = vas->insertIndexElem(argp->indexobj, key, value);
	REPLY;
}
void_reply *
remove1a_1(
	remove1a_arg *argp, 
	void *clnt
)
{
	FSTART(void,remove1a); 
	VASResult x;
	OPAQUE2VEC(key);
	OPAQUE2VEC(value);

	x = vas->removeIndexElem(argp->indexobj, key, value);
	REPLY;
}
int_reply *
remove2a_1(
	remove2a_arg *argp, 
	void *clnt
)
{
	FSTART(int,remove2a); 
	VASResult x;
	OPAQUE2VEC(key);

	x = vas->removeIndexElem(argp->indexobj, key, &reply->result);
	REPLY;
}

find_reply *
finda_1(
	finda_arg *argp, 
	void *clnt
)
{
	FSTART(find,finda); 
	VASResult x;
	OPAQUE2VEC(key);
	char 	*d;

	// malloc instead of new because it gets freed instead of deleted
	assert(((int)argp->value_limit) >= 0);
	d = 	(char *)malloc(argp->value_limit);
	MALLOC_CHECK(d);
	{
		vec_t	value(d, argp->value_limit);
		reply->value.opaque_t_val = d; // gets freed by rpcgen

		bool found=false;


		x = vas->findIndexElem(argp->indexobj, key, value, 
			(ObjectSize *)&reply->value.opaque_t_len,
			&found);
		reply->found =found;

		if(!reply->found) {
			// value len is not set 
			// if not found meaningless
			reply->value.opaque_t_len = 0;
			reply->value.opaque_t_val = 0; 
			free(d);
		}
	}
bad:
	REPLY;
}

lrid_t_reply *
snapref_1(	
	snapref_arg *argp, 
	void *clnt
)
{
	FSTART(lrid_t,snapref); 
	VASResult x;

	x = vas->snapRef(*argp, &reply->result);
	REPLY;
}

lrid_t_reply *
offvolref_1(
	offvolref_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(lrid_t,offvolref); 
	x = vas->offVolRef(argp->from_volume, argp->to_loid, &reply->result);
	REPLY;
}

lrid_t_reply *
mkvolref_1(
	mkvolref_arg *argp,
	void *clnt)
{
	VASResult x;
	FSTART(lrid_t,mkvolref); 
	x = vas->mkVolRef(argp->onvolume, &reply->result, argp->number);
	REPLY;
}


Cookie_reply *
openindexscan2_1(
	openindexscan2_arg *argp, 
	void *clnt
)
{
	VASResult x;
	FSTART(Cookie,openindexscan2); 
	OPAQUE2VEC(lbound);
	OPAQUE2VEC(ubound);

	x = vas->openIndexScan(argp->idx, argp->lc,
		lbound, argp->uc, ubound, &reply->result);

	REPLY;
}

nextindexscan_reply *
nextindexscan_1(
	nextindexscan_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(nextindexscan,nextindexscan); 
	{
	NEWVEC(key);
	{
	NEWVEC(value);

	reply->cookie = argp->cookie;
	bool eof=false;

	// in case of error...
	reply->value.opaque_t_len = 0;
	reply->key.opaque_t_len = 0;

	x = vas->nextIndexScan(&reply->cookie, key, 
		(ObjectSize *)&reply->key.opaque_t_len,
		value, 
		(ObjectSize *)&reply->value.opaque_t_len, &eof);
	reply->eof = eof;
	}

	}
bad:
	REPLY;
}

void_reply *
closeindexscan_1(
	closeindexscan_arg *argp,
	void	*clnt
)
{
	VASResult x;
	FSTART(void,closeindexscan); 

	x = vas->closeIndexScan(argp->cookie);

	REPLY;
}

int_reply *
locktimeout_1(
	locktimeout_arg *argp,
	void	*clnt
)
{
	VASResult x;
	FSTART(int,locktimeout); 
	x = vas->lock_timeout(argp->new_timeout, &reply->result);
	REPLY;
}

#ifndef notdef
int_reply *
sdltest_1(
	sdltest_arg *argp,
	void *clnt
)
{
	VASResult x;
	FSTART(int,sdltest); 
	x = vas->sdl_test(argp->argc,argp->args, &reply->result);
	REPLY;
}
#endif

readsymlink_reply *
readsymlink_1(
	readsymlink_arg *argp,
	void	*clnt
)
{
	VASResult x;
	FSTART(readsymlink,readsymlink); 
	NEWVEC(contents);

	// in case of error...
	reply->contents.opaque_t_len = 0;

	x = vas->readLink(argp->symname, contents, 
		(ObjectSize *)&reply->contents.opaque_t_len);

#	ifdef DEBUG
	vas->checkflags(true);
#	endif
	vas->clrflags(svas_base::vf_sm_page|svas_base::vf_obj_follows);
	vas->setflags(svas_base::vf_no_xfer);
bad:
	REPLY;
}
readsymlink_reply *
readsymlink2_1(
	readsymlink2_arg *argp,
	void	*clnt
)
{
	VASResult x;
	FSTART(readsymlink,readsymlink2); 
	NEWVEC(contents);

	x = vas->readLink(argp->object, contents, 
		(ObjectSize *)&reply->contents.opaque_t_len);
#	ifdef DEBUG
	vas->checkflags(true);
#	endif
	vas->clrflags(svas_base::vf_sm_page|svas_base::vf_obj_follows);
	vas->setflags(svas_base::vf_no_xfer);
bad:
	REPLY;
}
Cookie_reply *
openpoolscan2_1(
	openpoolscan2_arg *argp,
	void	*clnt
)
{
	VASResult x;
	FSTART(Cookie,openpoolscan2); 

	x = vas->openPoolScan(argp->name, &reply->result);
	REPLY;
}

Cookie_reply *
openpoolscan1_1(
	openpoolscan1_arg *argp,
	void	*clnt
)
{
	VASResult x;
	FSTART(Cookie,openpoolscan1); 

	x = vas->openPoolScan(argp->pool, &reply->result);
	REPLY;
}

nextpoolscan1_reply *
nextpoolscan1_1(
	nextpoolscan1_arg *argp,
	void	*clnt
)
{
	VASResult x;
	FSTART(nextpoolscan1,nextpoolscan1); 

	unsigned int what = cor_scan;

	// MLM: temp hack - otherwise, xdr will fail
	memset(reply, '\0', sizeof(*reply)); 
	reply->sysprops.specific_u.tag = KindTransient;

	reply->cookie = argp->cookie;

	dassert( !argp->wantsysprops  );

	// nextpoolscan1 never reads the object, never gets
	// sysprops

#ifdef DEBUG
	vas->checkflags(false);
#endif

	// tell vas what set of pages to use.
	bool eof=false;
	x = vas->nextPoolScan(&reply->cookie, &eof, &reply->commonpart.snapped);
	reply->eof=eof;

	REPLY;
}

nextpoolscan2_reply *
nextpoolscan2_1(
	nextpoolscan2_arg *argp,
	void	*clnt
)
{
	VASResult x;
	FSTART(nextpoolscan2,nextpoolscan2); 
	SysProps s, *sp;
	// NEWVEC(data);

	memset(reply, '\0', sizeof(*reply));

	// scan always tries to send the page
	unsigned int what = cor_scan | cor_page;

	reply->cookie = argp->cookie;
	if(argp->wantsysprops) {
		sp = &s;
		s.tag = KindTransient;
		what |= cor_sysp;
	} else {
		sp = 0;
	}

	// nextpoolscan2 always reads the object
	// if user gives vector of size 0, this
	// should still work
	what |= cor_read;

	char  *data_ptr = vas->replace_lg_buf();
	vec_t	data(data_ptr, (int) argp->data_limit);

#ifdef DEBUG
	vas->checkflags(false);
#endif

	bool eof=false;

	// in case of error...
	reply->commonpart.data.opaque_t_len = 0;

	// tell vas what set of pages to use.
	if((x = vas->use_page(argp->pageoffset))== SVAS_OK) {
		x = vas->nextPoolScan(&reply->cookie, 
			&eof, 
			&reply->commonpart.snapped,
			argp->start, argp->requested, 
			data, 
			(ObjectSize *)&reply->commonpart.data.opaque_t_len,
			&reply->commonpart.more,
			argp->lock,
			sp, &reply->sysp_size
		);
		DBG( << "data.size=" << data.size() );
		DBG( << "opaque_t_len= " << reply->commonpart.data.opaque_t_len);
		DBG( << "opaque_t_val= " << ::hex((u_int)(reply->commonpart.data.opaque_t_val)) );
	}
	reply->eof = eof;

#ifndef USE_COMMON_OBJMSG
	// TODO: remove this after testing common_objmsg()
	if(x==SVAS_OK) {
		// readObj copies into data vector if large, into
		// small-page_buf if small anonymous

#ifdef DEBUG
		vas->checkflags(true);
#endif

		switch(vas->xfercase()) {
		case svas_base::case_a: // case_a = vf_sm_page|vf_wire,

			dassert(vas->num_page_bufs() > 0);
			reply->sent_small_obj_page = true; // only one
			reply->obj_follows_bytes = 0; 

			assert(vas->shm.base() == NULL);

			// sending exactly one page
			reply->data.opaque_t_len = vas->page_size();
			reply->data.opaque_t_val = vas->replace_page_buf();

			DBG( << "COPYING WHOLE PAGE to " << 
			::hex((unsigned int)reply->data.opaque_t_val ));

			DBG( << "new buf is " << ::hex((unsigned int) 
				vas->page_buf()));
			break;

		case svas_base::case_b: // case_b = vf_sm_page|vf_shm,
			dassert(vas->num_page_bufs() > 0);
			reply->sent_small_obj_page = true; // only one
			reply->obj_follows_bytes = 0; 

			reply->data.opaque_t_len = 0;
			reply->data.opaque_t_val = NULL;

			// don't need to do anything because the page
			// is already in shm
			assert(vas->shm.base() != NULL);
			break;

		case svas_base::case_c: // case_c = vf_obj_follows|vf_wire,
			// data were copied to the given vector,
			// whatever that was
			reply->sent_small_obj_page = false;
			reply->obj_follows_bytes = 
				reply->data.opaque_t_len;
			// reply->data.opaque_t_len was set by readObj
			reply->data.opaque_t_val = data_ptr;
				// data ptr was dynamically allocated
			break;

		case svas_base::case_d: // case_d = vf_obj_follows|vf_shm,
			// data were copied to the given vector,
			// and the object length was written  to data.opaque_t_len

			reply->sent_small_obj_page = false;
			reply->obj_follows_bytes = 
				reply->data.opaque_t_len;

			reply->data.opaque_t_len = 0;
			reply->data.opaque_t_val = NULL;
				break;

		case svas_base::case_e: // case_e = vf_pseudo -- should not get here
		default:
			assert(0);
		}
#ifdef DEBUG
		vas->checkflags(true);
#endif
		vas->clrflags(svas_base::vf_sm_page|svas_base::vf_obj_follows);
		vas->setflags(svas_base::vf_no_xfer);
#ifdef DEBUG
		vas->checkflags(false);
#endif
	} else {
		reply->data.opaque_t_len = 0;
		reply->data.opaque_t_val = NULL;
		reply->sent_small_obj_page = false;
		reply->obj_follows_bytes =  0;
		s.tag = KindTransient; // for rpc 
	}
	reply->sysprops = convert_sysprops(s);
#else
	*reply = (common_objmsg(NextPoolScan2Req,
		vas,x,what,data_ptr,
		(ObjectSize)reply->commonpart.data.opaque_t_len,
		reply->commonpart.more,
		reply->commonpart.snapped,
		&s))->_u.repu_u._scan2;
#endif /*notdef*/
	REPLY;
}
void_reply *
closepoolscan_1(
	closepoolscan_arg *argp,
	void	*clnt
)
{
	VASResult x;
	FSTART(void,closepoolscan); 

	x = vas->closePoolScan(argp->cookie);
	REPLY;
}

lrid_t_reply *
setroot_1(
	setroot_arg *argp, 
	void *clnt
)
{  
	FSTART(lrid_t,setroot); 
	VASResult x;

	x = vas->setRoot(argp->volume, &reply->result);
	REPLY;
}
lrid_t_reply *
getroot_1(
	getroot_arg *argp, /* void_arg */
	void *clnt
)
{  
	FSTART(lrid_t,getroot); 
	VASResult x;

	x = vas->getRootDir(&reply->result);
	REPLY;
}

void_reply *
setumask_1(
	setumask_arg *argp,
	void *clnt
)
{  
	FSTART(void,setumask); 
	VASResult x;

	DBG(<<"setting umask to " << argp->umask);
	x = vas->setUmask(argp->umask);
	REPLY;
}

u_int_reply *
getumask_1(
	getumask_arg *argp,
	void *clnt
)
{  
	FSTART(u_int,getumask); 
	VASResult x;

	x = vas->getUmask(&reply->result);
	DBG(<<"getumask returns " << reply->result);
	REPLY;
}

void_reply *
chmod1_1(
	chmod1_arg *argp,
	void *clnt)
{ 
	FSTART(void,chmod1); 
	VASResult x;

	DBG(<<" chMod to " << argp->mode);
	x = vas->chMod(argp->path, argp->mode);
	REPLY;
}

void_reply *
chown1_1(
	chown1_arg *argp,
	void *clnt)
{ 
	FSTART(void,chown1); 
	VASResult x;

	DBG(<<"path=" << argp->path << "uid= " << argp->uid << " gid=" << argp->gid);
	if(argp->uid == BAD_UID) {
		x = vas->chGrp(argp->path, (gid_t) argp->gid);
	} else {
		x = vas->chOwn(argp->path, (uid_t)argp->uid);
	}
	REPLY;
}

void_reply *
utimes1_1(
	utimes1_arg *argp,
	void *clnt)
{ 
	FSTART(void,utimes1); 
	VASResult x;
	struct timeval_t *a,*m;

	if((argp->utimea.tv_sec == -1) && (argp->utimea.tv_usec == -1)) {
		a = NULL;
	} else {
		a = &argp->utimea;
	}
	if((argp->utimem.tv_sec == -1) && (argp->utimem.tv_usec == -1)) {
		m = NULL;
	} else {
		m = &argp->utimem;
	}
	x = vas->utimes(argp->path, (struct timeval *)a, (struct timeval *)m);
	REPLY;
}
void_reply *
utimes2_1(
	utimes2_arg *argp,
	void *clnt)
{ 
	FSTART(void,utimes2); 
	VASResult x;
	struct timeval_t *a,*m;

	if((argp->utimea.tv_sec == -1) && (argp->utimea.tv_usec == -1)) {
		a = NULL;
	} else {
		a = &argp->utimea;
	}
	if((argp->utimem.tv_sec == -1) && (argp->utimem.tv_usec == -1)) {
		m = NULL;
	} else {
		m = &argp->utimem;
	}
	x = vas->utimes(argp->target, (struct timeval *)a, (struct timeval *)m);
	REPLY;
}

lrid_t_reply *
fileof1_1(
	fileof1_arg *argp,
	void *clnt
)
{
	FSTART(lrid_t,fileof1); 
	VASResult x;
	x = vas->fileOf(*argp, &reply->result);
	REPLY;
}

lrid_t_reply *
fileof2_1(
	fileof2_arg *argp,
	void *clnt
)
{
	FSTART(lrid_t,fileof2); 
	VASResult x;
	x = vas->fileOf(*argp, &reply->result);
	REPLY;
}

/* HERE ******************************************************************/

void_reply *
chroot1_1(
	chroot1_arg *argp,
	void *clnt)
{  _TODO_; return NULL; }

lrid_t_reply *
getdir_1(
	getdir_arg *argp,
	void *clnt)
	// _TODO_: if we don't use this, get rid of it (in msg.x)
{  _TODO_; return NULL; }

void_reply *
setreuid1_1(
	setreuid1_arg *argp,
	void *clnt)
{  _TODO_; return NULL; }

getreuid1_reply *
getreuid1_1(
	getreuid1_arg *argp,
	void *clnt)
{  _TODO_; return NULL; }

void_reply *
setregid1_1(
	setregid1_arg *argp,
	void *clnt)
{  _TODO_; return NULL; }

getregid1_reply *
getregid1_1(
	getregid1_arg *argp,
	void *clnt)
{  _TODO_; return NULL; }
/* AUTOMATIC INDEXES */

voidref_reply *
fetchelem_1(
	fetchelem_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

void_reply *
insertelem_1(
	insertelem_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

void_reply *
removeelem_1(
	removeelem_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

voidref_reply *
incelem_1(
	incelem_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

voidref_reply *
decelem_1(
	decelem_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

Cookie_reply *
scanindex_1(
	scanindex_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

nextelem_reply *
nextelem_1(
	nextelem_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

void_reply *
closescan_1(
	closescan_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

/* TRANSACTIONS */

tid_t_reply *
enter2pc_1(
	enter2pc_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

Vote_reply *
prepare_1(
	prepare_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

void_reply *
continue2pc_1(
	continue2pc_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

tid_t_reply *
recover2pc_1(
	recover2pc_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

/* LOCKING */

void_reply *
lockobj_1(
	lockobj_arg *argp,
	void *clnt
)
{ 
	VASResult x;
	FSTART(void,lockobj); 

	x = vas->lockObj(argp->obj, argp->mode, argp->ok2block);
	REPLY;
}

stat_values *
gather_values(int count, const char *t, const w_stat_t *v)
{
	DBG(<<"gather_values(" << count << " " << t);
	stat_values *res = 
		(stat_values *)malloc(count * sizeof(stat_values));
	if(res) {
		stat_values *r = res;
		for(int i=0; i<count; i++,r++,v++,t++) {
			DBG(<<"type= " << *t);
			switch(*t) {
				case 'v':
					r->tag = __v;
					r->stat_values_u._v = (unsigned long) *v;
					break;
				case 'l':
					r->tag = __l;
					r->stat_values_u._l = (long) *v;
					break;
				case 'i':
					r->tag = __i;
					r->stat_values_u._i = (int) *v;
					break;
				case 'u':
					r->tag = __u;
					r->stat_values_u._u = (unsigned int) *v;
					break;
				case 'f':
					r->stat_values_u._f = (float) *v;
					r->tag = __f;
					break;
				default:
					assert(0);
			}
		}
	} 
	return res;
}
#ifdef DEBUG
extern "C" void	print_gather_reply(gather_stats_reply *);
#endif

gather_stats_reply *
gather_stats_1(
	gather_stats_arg *argp,
	void *clnt
)
{ 
	VASResult 				x;
	FSTART(gather_stats,gather_stats); 
	stats_module			*sm;
	const char				*typestring=0;
	int						nmodules=0;
	bool					send_values_only;

	w_statistics_t 			dummy;

	dummy << *vas;

	reply->signature=dummy.signature();
	send_values_only = (argp->signature == dummy.signature())? true: false;

	if(dummy.empty()) {
		DBG(<<"empty");
		reply->modules.modules_len = 0;
		reply->modules.modules_val = 0;
	} else {
		DBG(<<"not empty");
		const w_stat_module_t  	*m;
		w_statistics_i			iter(dummy);

		// count the modules
		for(nmodules=0, m = iter.curr(); m!=0; nmodules++, m = iter.next());
		iter.reset();

		DBG(<< nmodules << " modules");

		reply->modules.modules_len = nmodules;
		sm = (stats_module *) malloc(nmodules * sizeof(stats_module));
		MALLOC_CHECK(sm);
		memset(sm, '\0', nmodules * sizeof(stats_module));
		reply->modules.modules_val = sm;


		for(m = iter.curr(); m!=0; m = iter.next(), sm++) {
			sm->base = m->base;
			sm->count = m->count;
			sm->longest = m->longest;
			typestring = m->types;

			// steal everything
			if( send_values_only ) {
				sm->descr.tag = isnull;
				sm->descr.possibly_null_string_u.str = 0;
				sm->types.tag = isnull;
				sm->types.possibly_null_string_u.str = 0;
				sm->msgs.msgs_len = 0;
				sm->msgs.msgs_val = 0;
			} else {
				// it's safe to say that all the stats
				// gathered in the server are static forms
				dassert(! m->m_d);
				sm->descr.tag = nonnull;
				sm->descr.possibly_null_string_u.str = 
					(char *) m->strcpy(m->d);
				if(sm->descr.possibly_null_string_u.str == 0) goto bad;

				
				dassert(! m->m_types);
				sm->types.tag = nonnull;
				sm->types.possibly_null_string_u.str =
									 m->strcpy(m->types);
				if(sm->types.possibly_null_string_u.str == 0) goto bad;
				
				sm->msgs.msgs_len = sm->count;
				dassert(! m->m_strings);
				sm->msgs.msgs_val = (char **) m->getstrings(true);
				if( sm->msgs.msgs_val == 0) goto bad;
				
			}
			sm->values.values_len = sm->count;
			sm->values.values_val = 
				gather_values(sm->count, typestring, m->values);
			if( sm->values.values_val == 0 ) goto bad;
		} 
	}
#ifdef DEBUG
	// print_gather_reply(reply);
#endif
	REPLY;

bad:
	DBG(<< "oops");
	if(sm = reply->modules.modules_val) {
		// free everything in reply
		int i;
		for(i=0; i<nmodules; i++,sm++) {
			if(sm->descr.possibly_null_string_u.str) { 
				free(sm->descr.possibly_null_string_u.str); 
				sm->descr.possibly_null_string_u.str=0; 
			}
			if(sm->types.possibly_null_string_u.str) { 
				free(sm->types.possibly_null_string_u.str); 
				sm->types.possibly_null_string_u.str=0; 
			}
			if(sm->msgs.msgs_val) {
				// free the strings themselves
				for(int j=0; j<sm->msgs.msgs_len; j++) {
					free((char *)sm->msgs.msgs_val[j]);
				}
				free(sm->msgs.msgs_val);
				sm->msgs.msgs_val = 0;
				sm->msgs.msgs_len = 0;
			}
			if(sm->values.values_val) {
				free(sm->values.values_val);
				sm->values.values_val = 0;
				sm->values.values_len = 0;
			}
		}
		free(sm);
		reply->modules.modules_len = 0;
		reply->modules.modules_val = 0;
	}
	REPLY;
}

void_reply *
upgrade_1(
	upgrade_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

void_reply *
unlockobj_1(
	unlockobj_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

/* SELECT */

void_reply *
notifylock_1(
	notifylock_arg *argp,
	void *clnt)
{ _TODO_; _TODO_; return NULL; }

void_reply *
notifyandlock_1(
	notifyandlock_arg *argp,
	void *clnt)
{ _TODO_; _TODO_; return NULL; }

void_reply *
selectnotifications_1(
	selectnotifications_arg *argp,
	void *clnt)
{ _TODO_; _TODO_; return NULL; }

/* REFS */

bool_reply *
validateref_1(
	validateref_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

POID_reply *
physicaloid_1(
	physicaloid_arg *argp,
	void *clnt)
{ _TODO_; return NULL; }

void_reply *
transferref_1(
	transferref_arg *argp,
	void *clnt)
{ 
	_TODO_
	return NULL; 
}

