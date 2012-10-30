/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/client/vas.C,v 1.113 1997/10/18 15:23:23 solomon Exp $
 */
#define RPC_CLNT
#define __malloc_h

#ifdef __GNUG__
/* #pragma implementation "w_shmem.h" */
#endif

#ifdef SOLARIS2
#define PORTMAP
#endif

#include <msg.h>

#ifdef SOLARIS2
#include "chng.h"
#endif

#include <msg_stats.h>
#include <vas_internal.h>
#include <vaserr.h>
#include <default_port.h>

#include <uname.h>
#include "batch.h"

#include <xdrmem.h>
#include <sysprops.h>
#include <inet_stuff.h>
#include <netdb.h>
#include "svas_layer.h"

// This definition of null-tid is because 
// the type is defined in msg.h for this file.
// gag
#define IS_NULL_TID(t) ((t.lo==0) && (t.hi==0))
tid_t	__null_tid = { 0, 0};
#define NULL_TID   ((tid_t)__null_tid)
// The following is not actually used, but without it, some linkers give the
// warning "size of symbol `__5tid_tRC5tid_t' changed from 25 to 29 in vas.o"
extern tid_t::tid_t(tid_t const &);

#ifdef USE_KRB
#include <krb.h>
#include <des.h>
#include <krbplusplus.h>
#endif

/*
 * cannot use serial_t::is_null()
 */
#ifdef BITS64
// check only bits other than the lowest

#define ISNULL(x)((x.data._high == 0) && ((x.data._low & ~0x1) == 0)
#define ISEQ(x,y) (x.data._low==y.data._low) && (x.data._high==y.data._high)
#else 
	/* BITS64 */
#define ISNULL(x) ((x.data._low & ~0x1) == 0)
#define ISEQ(x,y) (x.data._low==y.data._low)
#endif

#define ISEQV(x,y) \
	(x.high==y.high) && (x.low==y.low)

#define NULLPARAMCHECK(name,num) \
	if(name==NULL) {\
		VERR( _cat(SVAS_BadParam,num) );\
		FAIL;\
	}

#define OUTPUTVEC(x) _cat(a.x,_limit) = x.size()
	// server had better have respected the limit!
#define VECOUTPUT(value)\
	if(reply->value.opaque_t_val !=0 && reply->value.opaque_t_len != 0) { \
		value.copy_from(reply->value.opaque_t_val, reply->value.opaque_t_len);\
	} \
	_cat(_cat(*,value),_len) = reply->value.opaque_t_len;


#define CONVERTVEC(core) convertvec(core,a.core)
void
convertvec(const vec_t &core, struct opaque_t &opq) 
{
	unsigned int limit;
	if((limit = core.size())>0) {
		opq.opaque_t_val = new char[limit];
		core.copy_to(opq.opaque_t_val);
	} else opq.opaque_t_val = NULL;
	opq.opaque_t_len = limit;
}

#define UNCONVERTVEC(core) unconvertvec(a.core)
void
unconvertvec(struct opaque_t &opq) 
{
	if(opq.opaque_t_val != NULL) {
		delete opq.opaque_t_val;
		opq.opaque_t_val = 0;
	}
}

extern struct msg_info cmsg_names; // from ../common/cmsg_names.C
static _msg_stats msg_stats("Client Requests", 0x00090000,
	client_init, gather_stats,cmsg_names);

static _msg_stats rmsg_stats("Messages Sent", 0x000b0000,
	client_init, gather_stats, cmsg_names);
bool	printusererrors = true;

#include <errlog.h>
#define shorelog ShoreVasLayer.log

static int
CLIENT2socket(
	IN(CLIENT) cl
)
{
	typedef struct ct_data {
		int ct_sock;
	}ct_data;

	// gives warning about alignment requirements
	ct_data *ct = 	(ct_data *) cl.cl_private;
	int 			sock = ct->ct_sock;

	return sock;
}

static void
set_timeout(
	CLIENT *cl, 	// can't be "IN" because it's an arg to "clnt_control"
	int which, 
	int sec, 
	int usec
)
{
	struct timeval 	tv;
	tv.tv_sec = sec;
	tv.tv_usec = usec;
	clnt_control(cl, which, &tv);
}

//
// RETURNS A REF TO A STATIC
// (for purpose of immediate copy)
//
static SysProps    &
convert_sysprops(
	IN(rpcSysProps) r
) 
{
	static SysProps	s;

#define SYSP_CONVERT(t,u) s.t = r.u 

	SYSP_CONVERT(volume,volume);
	SYSP_CONVERT(ref,ref);
	SYSP_CONVERT(type,type);
	SYSP_CONVERT(csize,csize);
	SYSP_CONVERT(hsize,hsize);
	SYSP_CONVERT(tstart,tstart);
	SYSP_CONVERT(nindex,nindex);
	SYSP_CONVERT(tag,rpctag);
	if(s.tag == KindAnonymous) {
		SYSP_CONVERT(anon_pool,rpcanon_pool);
	} else {
		SYSP_CONVERT(reg_nlink,rpcreg_nlink);
		SYSP_CONVERT(reg_mode,rpcreg_mode);
		SYSP_CONVERT(reg_uid,rpcreg_uid);
		SYSP_CONVERT(reg_gid,rpcreg_gid);
		SYSP_CONVERT(reg_atime,rpcreg_atime);
		SYSP_CONVERT(reg_mtime,rpcreg_mtime);
		SYSP_CONVERT(reg_ctime,rpcreg_ctime);
	}
	return s;
}

static bool swapped(c_uid_t x, c_uid_t y) {
	union {
		c_uid_t l;
		unsigned char c[4];
	} xx, yy;
	xx.l = x; yy.l = y;
	for (int i=0; i<4; i++)
		if (xx.c[i] != yy.c[3-i])
			return false;
	return true;
}

svas_client::svas_client(
	const char *_host,
	ErrLog 	   *el
) : 
	svas_base(el),
	cl(0),
	_batch(0),
	// _status are cleared in the body
	_lg_buf(0),
	ht(0)
{
	FUNC(svas_client::svas_client)
	CLEARSTATUS;
	struct 	sockaddr_in me, peer;
	int					sock;
//	char				*chr;
	int					version=0;

	char *host;

	dassert(this!=NULL);

	if(_host == NULL) {
		if( ! ShoreVasLayer.opt_host) {
			// error-- user must not have called setup_options()
	catastrophic(
"Missing option: svas_host; see manual pages options(shore), options(svas), and main(shore)"
	);
		}
		host = (char *) ShoreVasLayer.opt_host->value();
	} else {
		host =  (char *)_host;
	}
	{
		/* override version if version # is given */
		const char *pstr=0;
		if(ShoreVasLayer.opt_port) {
			pstr = ShoreVasLayer.opt_port->value();
		} // else use default version
		if(pstr) {
			version = (int)strtol(pstr, 0, 0);
		}
	}
	if(version==0) {
		version = DEFAULT_PORT;
	}

#ifdef DEBUG
	INITBOOL(do_audit, opt_audit, false);
#endif /*DEBUG*/

	INITBOOL(_pusererrs,opt_print_user_errors,false);
	INITBOOL(use_pmap,opt_use_pmap,true);

	cl = NULL;
	memset(&status, '\0', sizeof(status));
	goto start;
	// This funky goto stuff is to keep the compiler from
	// complaining about assigning to this "after" using this.

bad:
	dassert(status.vasresult != SVAS_OK);
	dassert(status.vasreason != SVAS_OK);

	if(cl) clnt_destroy((CLIENT *)cl);

//	::delete [] chr;
	VRETURN;

start:

	// TODO: make thread-safe by making a malloc-ed
	// and freed copy
	char *name;

	{
		if((name = uid2uname(getuid()))==NULL) {
			perror("uid2uname");
			VERR(SVAS_RpcFailure); // gets OS reason
			goto bad;
		}
	}

	unsigned int client_program = svas_base::_version + CLIENT_PROGRAM;

	struct sockaddr_in addr;
	struct hostent *h = gethostbyname(host);

	if (!h) {
		fprintf(stderr, "%s: ", host);
		catastrophic("host unknown");
	}
	memset(&addr, '\0', sizeof addr); // for purify
	memcpy(&addr.sin_addr, h->h_addr, sizeof addr.sin_addr);
	addr.sin_family = AF_INET;

	if(ShoreVasLayer.use_pmap) {
		DBG(<<"using port mapper for host " << host
			<< " prog " << client_program
			<< " vers " << version
		);
		addr.sin_port = 0;
	} else {
		DBG(<<"bypassing port mapper for host " << host
			<< " prog " << client_program
			<< " vers " << version
			<< " port " << version
			);
		// Now that we're passing in the address, we
		// have to put it into network byte order.
		//
		addr.sin_port = htons(version);
		// version doubles as port when we're trying to bypass
		// the portmapper

	}
	sock = RPC_ANYSOCK;
	cl = clnttcp_create(&addr, client_program, version, &sock, 0, 0);

	if ( !cl ) {
		DBG(<<"can't connect to " << host);
		clnt_pcreateerror(host);
		VERR(SVAS_RpcFailure);
		goto bad;
    } 

#if defined(SOLARIS2) || defined(Linux)
	set_timeout((CLIENT *)cl, CLSET_TIMEOUT, 120, 0); //long
#else
	set_timeout((CLIENT *)cl, CLRMV_TIMEOUT, 0, 0);
#endif
	set_timeout((CLIENT *)cl, CLSET_RETRY_TIMEOUT, 5, 0);

	// tell the other end who we are

	set_sock_opt(sock, SO_KEEPALIVE);

	{   
#ifdef USE_KRB
	{   
		/* specific to RPC 4.0 */ /* grot */
		int				krb_status;
		KTEXT_ST		ticket;
		MSG_DAT			msg_data;
		CREDENTIALS		cred;
		Key_schedule	sched;

		msg_data.app_data = NULL;
		msg_data.app_length = 0;
#ifdef MUTUAL
#define kopt	KOPT_DO_MUTUAL
#else
#define kopt	0
#endif
		krb_status = krb_sendauth(kopt, sock,
			&ticket, "rcmd", host, NULL, 0, &msg_data,
			&cred, sched, &me, &peer, NULL);

		if(krb_status != KSUCCESS) {
			cerr << "Kerberos error " << krb_err_txt[krb_status] << endl;
			VERR(SVAS_AuthenticationFailure); 
			goto bad;
		}
	}
#else
	{
		// just send login name
		int cc;
		cc = strlen(name);
		if( write(sock, name, cc) != cc ) {
			perror("write");
			VERR(SVAS_AuthenticationFailure); 
			goto bad;
		}
	}
#endif
	{
		/* server will send back uid of this guy */
		c_uid_t srvuid;

		errno = 0;
		if(read(sock, (char *)&srvuid, sizeof srvuid) != sizeof srvuid) {
			VERR(SVAS_AuthenticationFailure); 
			perror("read after sendauth: did not get uid back from VAS");
			goto bad;
		}
		if(srvuid !=  getuid()) {
			if (swapped(srvuid, getuid())) {

			shorelog->clog  <<  error_prio 
				<< "The server machine (" << host 
				<< ") has the opposite byte-order from this machine "
				<< flushl;

			} else {
				shorelog->clog  <<  error_prio 
				 	<< "You (" << name 
					<< ") have uid " << getuid()
				 	<<  " on client but server knows you as uid " << srvuid
				 	<< "." 
				<< flushl;
				VERR(SVAS_AuthenticationFailure); // gets OS reason
				goto bad;
			}
		} else {
			// shorelog->clog <<  info_prio 
			// 	<< "Uids match : " << srvuid << flushl;
			((CLIENT *)cl)->cl_auth = authunix_create_default();
		}
	}
	}

	initflags();  // clear all

	_num_page_bufs = 0;
	_num_lg_bufs = 0;
	cstats();

	transid =  NULL_TID;

	/* return ok */
	DBG( << "svas_client::svas_client() returning...");
	if(ShoreVasLayer.do_audit) audit("leave constructor");
}

svas_client::~svas_client() 
{
	FUNC(svas_client::~svas_client);
	if(ShoreVasLayer.do_audit) audit("enter destructor");
	if(ht) {
		// ht could be null due to an error in init
		destroyht();
	}
	if(cl) {
		clnt_destroy((CLIENT *)cl);
		cl = NULL;
	}
	if(_batch) { 
		delete _batch; 
		_batch=0; 
	}
}

static svas_client *_me=0;
static svas_client *
me() 
{
	return _me;
}

static void 
setme(svas_client *v) 
{
	_me = v;
}

VASResult
svas_client::_init( 
	mode_t		mask,
	int		nsmall, 
	int		nlarge 
)
{
	FSTART(svas_client::_init,client_init,init);
	DBG(<<"svas_client::_init");

	a.mode = mask;
	a.num_page_bytes = nsmall;
	a.num_lg_bytes = nlarge;
	a.protocol_version = svas_base::_version;
	DBG(<<"remote init");

	// SVCALL(client_init,init);
	++_stats.server_calls;
	rmsg_stats.inc((int)client_init);
	status.vasresult = SVAS_OK;
	reply = (init_reply *) client_init_1( &a, (CLIENT *)(this->cl)); 
	if( reply==NULL ) { 
		DBG(<<"null reply");
		// we connected already, so 
		// if we got a null reply, most likely
		// we're talking to a server compiled with
		// a protocol that differs

		VERR(SVAS_ConfigMismatch);
		return (status.vasresult = SVAS_FAILURE);
	} else { 
		__CHECK__;
	}

	if(reply->status.vasresult == SVAS_OK) {
		DBG(<<"remote init returned OK");

		_DO_(getRootDir(&this->_cwd));

		// TODO: get rid of _page_size and maybe all these reply params
		_page_size 	= (smsize_t) ShoreVasLayer.sm_page_size();

		if( ShoreVasLayer.sm_page_size() != reply->page_size ) {
			// client config does not match server config
			VERR(SVAS_ConfigMismatch);
			FAIL;
		}

		_num_page_bufs 	= reply->num_page_bufs;
		_num_lg_bufs 	= reply->num_lg_bufs;

		clrflags(0xffffffff);
		if(reply->shmid == 0) {
			// remote: set over_the_wire....
			setflags(vf_wire | vf_no_xfer);
			_lg_buf = 0;

			if(num_page_bufs()>0) {
				(void) createht(num_page_bufs(), 
					(char *) calloc(num_page_bufs(),page_size()));
			}
		} else {
			setflags(vf_shm | vf_no_xfer);

			dassert(num_page_bufs() + num_lg_bufs() > 0);
			dassert(num_page_bufs() >= 0);
			dassert(num_lg_bufs() >= 0);

			if(shm.attach(reply->shmid)) {
				VERR(SVAS_ShmError);
				FAIL;
			}
			DBG(<<"attached shmid " << dec << reply->shmid
				<< " at" << ::hex((unsigned int)shm.base())
				<< " size " << dec << shm.size()
				);

			if(num_page_bufs() > 0) {
				(void) createht(num_page_bufs(), shm.base());
			}

			dassert((page_size() * (num_page_bufs()+num_lg_bufs()))==
				shm.size());

			_lg_buf = shm.base() + (page_size() * num_page_bufs());
		}
		// shorelog->clog << info_prio
		// 	<< (over_the_wire()?"REMOTE-TCP":"LOCAL-SHM")
		// 	<< " page size " << page_size()
		// 	<< " num_lg_bufs " << num_lg_bufs()
		// 	<< " num_page_bufs " << num_page_bufs()
		// 	<< flushl;

		auditempty();
	} else {
		DBG(<<"remote init returned error " << 
		::hex((unsigned int)reply->status.vasreason));
	}

	// set up batching stuff
	{
		int	fd = CLIENT2socket(*((CLIENT *)cl));

		_batch = new batcher(this, shm, _lg_buf, 
					num_lg_bufs() * page_size(), fd,
					reply->sockbufsize);
		if(!_batch) {
			VERR(SVAS_MallocFailure);
			goto failure;
		}
	}
	setme(this);

	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

// called by REPLY macro (vaserr.h)
void	
svas_client::update_txstate(TxStatus t) 
{
	if((t != Active) && (t != Prepared)) {
		if(!IS_NULL_TID(transid)) {
			// abort by the server

			// abort & commit don't use the REPLY macro,
			// which is what calls this func

			invalidate_ht();
			transid = NULL_TID;

			dassert(status.vasresult == SVAS_ABORTED ||
					status.vasresult == SVAS_FAILURE);
			dassert(
			(status.vasresult == SVAS_ABORTED && status.txstate == Aborting)
			||
			(status.vasresult == SVAS_FAILURE && status.txstate != Aborting)
			);
		}
	} else {
		// this will work with begin
		// because it doesn't use the REPLY macro,
		// which is what calls this func
		dassert( !IS_NULL_TID(transid) ); 
	}
}

	V_IMPL( VASResult		rmfs(
						IN(lvid_t)	lvid
					))

VASResult		
svas_client::rmfs(
	IN(lvid_t)	lvid
) 
{
	FSTART(svas_client::rmfs, v_rmfs, void);
	a.lvid =  lvid;
	SVCALL(v_rmfs,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::mkfs(
	const Path 	dev, 
	unsigned int kb,
	IN(lvid_t)	lvid,	// use if not null
	OUT(lvid_t)	lvidp	// result
) 
{
	FSTART(svas_client::mkfs, v_mkfs, lvid_t);

	NULLPARAMCHECK(dev,1);
	STRARG(a.unixdevice,path,dev);
	a.allocate_vid = lvid_t_is_null(lvid);
	a.lvid =  lvid;
	a.kbytes =  kb;

	SVCALL(v_mkfs,lvid_t);

	if(lvidp) {
		*lvidp = reply->result;
	}
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::newvid(
	OUT(lvid_t)	lvid
)
{
	FSTART(svas_client::newvid,v_newvid,lvid_t);
	NULLPARAMCHECK(lvid,1);
	SVCALL(v_newvid,lvid_t);
	if(reply) {
		*lvid = reply->result;
	}
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::volumes(
	const Path 	dev, 	// (local devices only)
	int 		n,
	lvid_t		*list,
	OUT(int)	nentries,
	OUT(int)	total
)
{
	FSTART(svas_client::volumes,getvol,getvol);
	NULLPARAMCHECK(dev,1);
	NULLPARAMCHECK(nentries,4);
	NULLPARAMCHECK(total,5);
	STRARG(a.unixdevice,path,dev);
	a.nentries = n;
	SVCALL(getvol,getvol);
	if(reply) {
		*total = reply->nvols;
		*nentries = reply->nentries;

		lvid_t	*b = (lvid_t *)reply->buf.buf_val;

		dassert(reply->buf.buf_len == reply->nentries);
		dassert(reply->nentries == reply->nvols);
		dassert(reply->nentries <= n);

		for(int j=0; j < reply->nentries && j < n; j++) {
			list[j] = *b;
			b++;
		}
	}
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::devices(
	INOUT(char) buf,  // user-provided buffer
	int 	bufsize,	  // length of buffer in bytes
	INOUT(char *) list, // user-provided char *[]
	INOUT(devid_t) devs,  // user-provided devid_t[]
	INOUT(int) count,		// length of list on input & output
	OUT(bool) more
)
{
	FSTART(svas_client::devices,v_devices,v_devices);
	NULLPARAMCHECK(buf,1);
	NULLPARAMCHECK(list,3);
	NULLPARAMCHECK(count,4);
	NULLPARAMCHECK(more,5);
	a.cookie = 0;
	SVCALL(v_devices,v_devices);
	if(reply) {
		int i,l;
		char *b;
		const char *c;
		struct path_dev_pair *it;
		b = buf;
		for(i=0; i<reply->buf.buf_len; i++) {
			it = &reply->buf.buf_val[i];
			c = it->path;
			l = strlen(c);
			// if the string fits in the buffer...
			if((int)(b - buf) + l +1 < bufsize) {
				// go ahead and copy it to the buffer
				list[i] = b;
				strcpy(b, c);
				b+=l;
				// *b = '\0';
				b++;

				// copy the device id also
				devs[i].id = it->dev.ino;
				devs[i].dev = it->dev.dev;
			} else {
				// stop here
				break;
			}
		}
		*count = i;
		*more = (i<reply->buf.buf_len)?true:false;
	}
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult 	
svas_client::list_mounts(
	IN(lvid_t)	volume, // what volume?
	INOUT(char) buf,  // user-provided buffer
	int bufsize,	  // length of buffer in bytes
	INOUT(serial_t) dirlist,  // user-provided serial_t[]
	INOUT(char *) fnamelist, // user-provided char *[]
	INOUT(lvid_t) targetlist,  // user-provided lvid_t[]
	INOUT(int) 	count,	// length of lists on input & output
	OUT(bool) more
)
{
	FSTART(svas_client::list_mounts,v_listmounts,v_listmounts);
	NULLPARAMCHECK(buf,1);
	NULLPARAMCHECK(dirlist,4);
	NULLPARAMCHECK(fnamelist,5);
	NULLPARAMCHECK(targetlist,6);
	NULLPARAMCHECK(count,7);
	NULLPARAMCHECK(more,8);

	a.cookie = 0;
	a.volume = volume;
	SVCALL(v_listmounts,v_listmounts);
	if(reply) {
		int i,l;
		char *b;
		const char *c;
		struct pmountinfo *it;
		b = buf;
		for(i=0; i<reply->buf.buf_len; i++) {
			it = &reply->buf.buf_val[i];
			c = it->fname;
			l = strlen(c);
			// if the string fits in the buffer...
			if((int)(b - buf) + l +1 < bufsize) {
				// go ahead and copy it to the buffer
				fnamelist[i] = b;
				strcpy(b, c);
				b+=l;
				// *b = '\0';
				b++;

				// copy the device id also
				dirlist[i] = it->dirserial;
				targetlist[i] = it->target;
			} else {
				// stop here
				break;
			}
		}
		*count = i;
		*more = (i<reply->buf.buf_len)?true:false;
	}
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::format(
	const Path dev, 	// (local devices only)
	unsigned int kb,
	bool	force 		// = false
)
{
	FSTART(svas_client::format,v_format,void);
	NULLPARAMCHECK(dev,1);
	STRARG(a.unixdevice,path,dev);
	a.kbytes = kb;
	a.force = force;
	SVCALL(v_format,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::volroot(
	IN(lvid_t)	volume,
	OUT(lrid_t)	rootdir
)
{
	FSTART(svas_client::volroot,v_volroot,lrid_t);
	a.volume = volume;
	SVCALL(v_volroot,lrid_t);
	if(rootdir) *rootdir  = reply->result;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::unserve(
	const Path  devicename	// path in Unix namespace
)
{
	FSTART(svas_client::unserve,v_unserve,void);
	NULLPARAMCHECK(devicename,1);
	STRARG(a.unixdevice,path,devicename);
	SVCALL(v_unserve,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::serve(
	const Path 	devicename
)
{
	FSTART(svas_client::serve,v_serve,void);
	NULLPARAMCHECK(devicename,1);
	STRARG(a.unixdevice,path,devicename);
	a.writable = true;
	SVCALL(v_serve,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::mount(
	lvid_t		lvid,
	const Path 	mntpt,	
	bool		writable
)
{
	FSTART(svas_client::mount,v_mount,void);

	NULLPARAMCHECK(mntpt,2);

	STRARG(a.mountpoint,path,mntpt);
	a.lvid = lvid;
	a.writable = writable;

	SVCALL(v_mount,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::dismount(
	const Path 	mountpoint
)
{
	FSTART(svas_client::dismount,v_dismount,void);
	NULLPARAMCHECK(mountpoint,1);
	STRARG(a.mountpoint,path,mountpoint);
	a.ispatch1 = false;
	a.ispatch2 = false;
#ifdef PURIFY
	memset(&a.volume, '\0', sizeof(a.volume)); 
#endif
	SVCALL(v_dismount,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::punlink(
	const Path	mountpoint
)
{
	FSTART(svas_client::punlink1,v_dismount,void);

	NULLPARAMCHECK(mountpoint,1);
	STRARG(a.mountpoint,path,mountpoint);
	a.ispatch1 = true;
	a.ispatch2 = false;
#ifdef PURIFY
	memset(&a.volume, '\0', sizeof(a.volume)); 
#endif
	SVCALL(v_dismount,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::punlink(
	IN(lvid_t)  vol
)
{
	FSTART(svas_client::punlink2,v_dismount,void);

	a.mountpoint = 0;
	a.ispatch1 = false;
	a.ispatch2 = true;
	a.volume = vol;
	SVCALL(v_dismount,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult
svas_client::disk_usage(
	IN(lrid_t) obj,
	bool		mayberoot,
	OUT(struct sm_du_stats_t) stats
)
{
	FSTART(svas_client::disk_usage,diskusage,diskusage);
	NULLPARAMCHECK(stats,2);
	a.for_volume = false;
	a.oid = obj;
	a.mbroot = mayberoot;
	SVCALL(diskusage,diskusage);
	*stats  = reply->stats;
	REPLY;
failure:
 	VRETURN SVAS_FAILURE;
}

VASResult
svas_client::statfs(
	IN(lvid_t) vol,
	OUT(FSDATA) fsd
)
{
	FSTART(svas_client::statfs,statfs1,statfs1);
	NULLPARAMCHECK(fsd,2);
	a.volume = vol;
	SVCALL(statfs1,statfs1);
	*fsd = reply->fsdata;
	REPLY;
failure:
 	VRETURN SVAS_FAILURE;
}

VASResult
svas_client::disk_usage(
	IN(lvid_t) volid,
	OUT(struct sm_du_stats_t) stats
)
{
	FSTART(svas_client::disk_usage,diskusage,diskusage);
	NULLPARAMCHECK(stats,2);
	a.oid.lvid = volid;
	a.for_volume = true;
	SVCALL(diskusage,diskusage);
	// stats->clear();
	// stats->add(reply->stats);
	*stats  = reply->stats;
	REPLY;
failure:
 	VRETURN SVAS_FAILURE;
}

VASResult
svas_client::getMnt(
	INOUT(FSDATA) 	resultbuf, 
	ObjectSize 		bufbytes, 
	OUT(int)		nresults,
	Cookie			*const cookie	// "INOUT"
)
{
	FSTART(svas_client::getMnt,getmnt,getmnt);
	NULLPARAMCHECK(resultbuf,1);
	NULLPARAMCHECK(nresults,3);
	NULLPARAMCHECK(cookie,4);
	a.nbytes = bufbytes;
	a.cookie = *cookie;

	DBG(<<"getmnt with cookie=" << ::hex((unsigned int)a.cookie));

	SVCALL(getmnt,getmnt);

	bcopy((char *)reply->buf.buf_val, (char *)resultbuf, reply->buf.buf_len);
	*cookie = reply->cookie;
	*nresults = reply->nentries;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
VASResult
svas_client::chDir(
	const	Path	path
)
{
	FSTART(svas_client::chDir,chdir1,lrid_t);
	NULLPARAMCHECK(path,1);
	STRARG(a.path,Path,path);
	a.use_path = true;
	// a.dir = lrid_t::null;
	memset(&a.dir, '\0', sizeof(a.dir));
	SVCALL(chdir1,lrid_t);
	this->_cwd = reply->result;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

// support for gwd()
VASResult		
svas_client::_chDir(
	IN(lrid_t)	dir			// had better be a directory!
)
{
	FSTART(svas_client::chDir,chdir1,lrid_t);
	a.path = ""; // not freed or malloced
	a.use_path = false;
	a.dir = dir;
	SVCALL(chdir1,lrid_t);
	this->_cwd = reply->result;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult
svas_client::lookup(
	const Path  dir,	
	OUT(lrid_t) loid,
	OUT(bool)	found, // cannot be null
	PermOp		perm,	//  default = op_exec
	bool      follow     //  default = true
)
{
	FSTART(svas_client::lookup,lookup1,lookup);
	NULLPARAMCHECK(dir,1);
	NULLPARAMCHECK(loid,2);
	NULLPARAMCHECK(found,3);
	a.absolute = dir;
	a.perm = perm;
	a.follow = follow;

	SVCALL(lookup1,lookup);

	*loid =  reply->result;
	*found = reply->found;
	DBG(<<"found=" << reply->found);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

static count_sysprops=0;
VASResult		
svas_client::sysprops(
	IN(lrid_t)	obj,
	OUT(SysProps) sysp,
	bool				wholepage, 	// =false, 
	LockMode			lock,		// =S	
	OUT(bool)			is_unix_file, // = NULL
	OUT(int)			sysp_size, // = NULL
	OUT(bool)			pagecached // = NULL
)
{
	FSTART(svas_client::sysprops,stat1,rpcSysProps);
	count_sysprops++;

	ObjectSize 	len;
	ccaddr_t	o;
	bool		found_on_page;
	VASResult	x = SVAS_OK;
#ifdef DEBUG
	int 		tries=0;
#endif

	NULLPARAMCHECK(sysp,2);
	audit("svas_client::sysprop1");

	if(is_unix_file) *is_unix_file = false;
#ifdef DEBUG
	if(sysp_size) *sysp_size = 3000;
#endif

	checkflags(false); // not xfer time yet

	// locate the object on the page
	DBG( << "trying to locate object " << obj.serial.data._low 
		<< " wholepage=" << wholepage << "  lock=" << lock
	);

on_page:
#ifdef DEBUG
	if(tries++ > 1) {
		DBG(<< "tries=" << tries);
		assert(0);
	}
	DBG(<< "tries=" << tries);
#endif
	audit("svas_client::sysprop2");
	if(locate_header(obj, o, lock, &len, &found_on_page) != SVAS_OK) {
		dassert(0); // locate_object only  returns SVAS_OK for now
	}
	audit("svas_client::sysprop3");
	if(found_on_page) {
		DBG( << "FOUND len " << len );
		// have to xdr the header

		_sysprops	s;
		int err;
		audit("svas_client::sysprop4");

		if(err = sysp_swap(o, &s)) {
			VERR(err);
			FAIL;
		}

		sysp->volume 	= obj.lvid;
		sysp->ref  		= obj.serial.data; 
		sysp->type 		= s.common.type;
		sysp->csize 	= s.common.csize;
		sysp->hsize 	= s.common.hsize;

		AnonProps 	*ap;
		RegProps 	*rp;
		sysp->tag = sysp_split(s, &ap, &rp, &sysp->tstart,
									&sysp->nindex, sysp_size);
		switch (sysp->tag) {
			case KindAnonymous:
				sysp->anon_pool 	= ap->pool;
				break;
			
			case KindRegistered:
			default: 
				dassert(0);
				break;
		}
		checkflags(false); // not xfer time yet
		DBG(<<"sysprops returns type oid=" << sysp->type._low);
		dassert(sysp->type._low & 0x1);

		if(is_unix_file) {
			*is_unix_file = (sysp->tstart != NoText);
		}
		goto done; // goto not necessary 
	} else {
#ifdef DEBUG
		if(tries > 1) {
			assert(0);
		}
#endif
		DBG( << "NOT FOUND");
		checkflags(false); // not xfer time yet
		// could be large object
		a.obj = obj;
		assert(obj.serial.data._low != 0);
		a.pageoffset = replace_page(); 
		a.lock = lock>SH?lock:SH; // gets a minimum of a share lock on page
		a.copypage = wholepage; 
		SVCALL(stat1,rpcSysProps);

		if((x=reply->status.vasresult) != SVAS_OK)  {
			checkflags(false); // not xfer time 
			DBG(<< "Error from SV CALL");
		} else {
			dassert(reply!=NULL);
			if(reply->commonpart.sent_small_obj_page) {
				DBG(<< "Server sent small object page");
				// small object page only -- breaks the loop

				replaceflags(vf_no_xfer,vf_sm_page);

				if(pagecached) *pagecached = true;

				if(over_the_wire()) {
					// case_a

					DBG( << "PAGE SENT OVER THE WIRE" );
					dassert(shm.base() == NULL);
					dassert(reply->commonpart.data.opaque_t_len == page_size());

					// exchange pages with the page cache
					reply->commonpart.data.opaque_t_val = putpage(a.pageoffset, 
						reply->commonpart.data.opaque_t_val, page_size());
					// places the page in the cache but does
					// not put the objects into the hash table

				} else {
					// case_b
					DBG(<< "over shared memory");

					dassert(_flags & vf_shm);
					dassert(reply->commonpart.data.opaque_t_len == 0);
				}
				// limit is no longer needed
				installpage(obj, a.pageoffset);
				replaceflags(vf_obj_follows|vf_sm_page, vf_no_xfer);
				goto on_page;

			} else  {
				DBG(<< "NO small object page");
				// TODO: what if over the wire 
				dassert(reply->commonpart.sent_small_obj_page == false);
				*sysp =  convert_sysprops(reply->sysprops);
				if(sysp_size) *sysp_size = reply->sysp_size;
				dassert(sysp->type._low & 0x1);
			}

		}
		if(is_unix_file) {
			*is_unix_file = (sysp->tstart != NoText);
		}
	}
done:

	if(reply) {
		REPLY;
	} else {
		audit(_string(__LINE__));
		VRETURN x;
	}

failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::sysprops(
	const Path 			name,	
	OUT(SysProps)		sysprops
)
{
	FSTART(svas_client::sysprops,stat2,rpcSysProps);
	NULLPARAMCHECK(name,1);
	NULLPARAMCHECK(sysprops,2);
	STRARG(a.name,Path,name);
	SVCALL(stat2,rpcSysProps);

	if(reply->status.vasresult == SVAS_OK) {
		*sysprops =  convert_sysprops(reply->sysprops);
		dassert(sysprops->type._low & 0x1);
	} else { 
		DBG(<<"sysprops returns ERROR");
	}
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::getDirEntries(
	IN(lrid_t)      dir,        
	INOUT(char) resultbuf, 
	ObjectSize  bufbytes,      
	OUT(int)    nresults,  
	Cookie  *const cookie     
)
{
	FSTART(svas_client::getDirEntries,getdirentries1,getdirentries1);
	NULLPARAMCHECK(resultbuf,2);
	NULLPARAMCHECK(nresults,4);
	NULLPARAMCHECK(cookie,5);
	a.dir = dir;
	a.numbytes = bufbytes;
	a.cookie = *cookie;

	SVCALL(getdirentries1,getdirentries1);

	bcopy((char *)reply->buf.buf_val, resultbuf, reply->buf.buf_len);
	*cookie = reply->cookie;
	*nresults = reply->numentries;

	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::rmDir(
	const Path name		
)
{
	FSTART(svas_client::rmDir,rmdir1,void);
	NULLPARAMCHECK(name,1);
	STRARG(a.name,Path,name);
	SVCALL(rmdir1,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::mkDir(
	const 	Path 	name,		// 
	mode_t			mode,		// 
	OUT(lrid_t)		result
)
{
	FSTART(svas_client::mkDir,mkdir1,lrid_t);
	NULLPARAMCHECK(name,1);
	NULLPARAMCHECK(result,3);
	STRARG(a.name,Path,name);
	a.mode = mode;
	SVCALL(mkdir1,lrid_t);
	result->lvid = 
		reply->result.lvid;
	result->serial = 
		reply->result.serial;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult
svas_client::abortTrans(int reason)
{
	FSTART(svas_client::abortTrans,abort1,void);
	VRETURN abortTrans(transid,reason);
}

VASResult
svas_client::abortTrans(
	IN(tid_t)     tid,
	int reason 
)
{
	FSTART(svas_client::abortTrans,abort1,void);
	// shorelog->clog << info_prio 
	// << "aborting transid " << this->transid << flushl;

	SVCALL(abort1,void);

	a = tid; // current one
	invalidate_ht();
	status = reply->status;
	dassert(status.txstate != Active);
	switch(status.vasresult) {
	case SVAS_OK:
	case SVAS_ABORTED:
		invalidate_ht();
		transid = NULL_TID;
		status.vasreason = reason;
		// clr_errorinfo();
		break;

	default:
	case SVAS_FAILURE:
		perr(cerr, _fname_debug_, __LINE__, __FILE__,ET_USER);
		break;
	
	}
	if(!CLNT_FREERES((CLIENT *)this->cl, _freeproc, reply)){
		assert(0); /* internal error */
	}
	VRETURN status.vasresult;
}

VASResult
svas_client::commitTrans(bool chain)
{
	FSTART(svas_client::commitTrans,commit,tid_t);
	a.tid = this->transid; // current one
	a.chain = chain;

	// shorelog->clog << info_prio << "committing transid " << this->transid << flushl;

	SVCALL(commit,tid_t);
	status = reply->status;
	if(status.vasresult == SVAS_OK)  {
		invalidate_ht();
		transid = reply->result;
	} else {
		perr(cerr, _fname_debug_, __LINE__, __FILE__,ET_USER);
	}

	if(!CLNT_FREERES((CLIENT *)this->cl, _freeproc, reply)){
		assert(0); /* internal error */
	}
	VRETURN status.vasresult;
}

VASResult
svas_client::beginTrans(
	int 			degree,   // 
	OUT(tid_t)    tid    
)
{
	FSTART(svas_client::beginTrans,begintrans,tid_t);
	a.degree = degree;
	a.wait_for_locks = true;
	a.unix_directory_service = false;

	auditempty();
	SVCALL(begintrans,tid_t);
	status = reply->status;
	if(status.vasresult == SVAS_OK)  {
#ifdef DEBUG
		  if(! IS_NULL_TID(transid)) {
				  dassert(0);
		   }
#endif
		this->transid = reply->result;
		DBG( << "beginTrans returns transid " 
			<< this->transid.hi << "." << this->transid.lo 
		);
		if(tid) *tid = this->transid;
	} else {
		perr(cerr, _fname_debug_, __LINE__, __FILE__,ET_USER);
	}
	if(!CLNT_FREERES((CLIENT *)this->cl, _freeproc, reply)){
		assert(0); /* internal error */
	}
	VRETURN status.vasresult;
}

VASResult
svas_client::commitTrans(
	IN(tid_t)     tid         // 
)
{
	FSTART(svas_client::commitTrans,commit,tid_t);
	// shorelog->clog << info_prio << "committing transid " << tid << flushl;

	a.tid  = tid;
	a.chain = false;
	SVCALL(commit,tid_t);
	status = reply->status;
	if(status.vasresult == SVAS_OK)  {
		invalidate_ht();
		transid = NULL_TID;
	}else {
		perr(cerr, _fname_debug_, __LINE__, __FILE__,ET_USER);
	}

	if(!CLNT_FREERES((CLIENT *)this->cl, _freeproc, reply)){
		assert(0); /* internal error */
	}
	VRETURN status.vasresult;
}

VASResult
svas_client::nfsBegin(
	int 			degree,
	u_int			*result
)
{
	FSTART(svas_client::nfsBegin,nfs_begintrans,u_int);
	a.degree = degree;
	auditempty();
	SVCALL(nfs_begintrans,u_int);
	status = reply->status;
	if(status.vasresult == SVAS_OK)  {
#if 0
		this->transid = reply->result;
		DBG( << "beginTrans returns transid " 
			<< this->transid.hi << "." << this->transid.lo 
		);
		if(tid) *tid = this->transid;
#endif
		*result = reply->result;
	} else {
		perr(cerr, _fname_debug_, __LINE__, __FILE__,ET_USER);
		*result = 0;
	}
	if(!CLNT_FREERES((CLIENT *)this->cl, _freeproc, reply)){
		assert(0); /* internal error */
	}
	VRETURN status.vasresult;
}

VASResult
svas_client::nfsAbort() 
{
	FSTART(svas_client::nfsAbort,nfs_abort1,void);
	SVCALL(nfs_abort1,void);
	status = reply->status;
	if(status.vasresult == SVAS_OK)  {
		invalidate_ht();
		transid = NULL_TID;
	} else {
		perr(cerr, _fname_debug_, __LINE__, __FILE__,ET_USER);
	}
	if(!CLNT_FREERES((CLIENT *)this->cl, _freeproc, reply)){
		assert(0); /* internal error */
	}
	VRETURN status.vasresult;
}

VASResult
svas_client::nfsCommit() {
	FSTART(svas_client::nfsCommit,nfs_commit,void);
	SVCALL(nfs_commit,void);
	status = reply->status;
	if(status.vasresult == SVAS_OK)  {
		invalidate_ht();
		transid = NULL_TID;
	}else {
		perr(cerr, _fname_debug_, __LINE__, __FILE__,ET_USER);
	}

	if(!CLNT_FREERES((CLIENT *)this->cl, _freeproc, reply)){
		assert(0); /* internal error */
	}
	VRETURN status.vasresult;
}

#ifdef notdef

VASResult
svas_client::savepoint(
	OUT(Savepoint)  save    
)
{
	_TODO // savepoint- check arg for null
}
VASResult
svas_client::rollBack(
	IN(Savepoint)   save        // 
)
{
	_TODO // savepoint
}
VASResult
svas_client::enter2PC(
	IN(gtid_t)      gtid,       // 

	OUT(CoordHandle) handle     // OUT
)
{
	_TODO // 2pc
}
VASResult
svas_client::prepare(
	IN(tid_t)     tid,        // 
	OUT(Vote)       vote        // OUT
)
{
	_TODO // 2pc - check fornull vote arg
}
VASResult
svas_client::continue2PC(
	IN(gtid_t)      gtid        // 
)
{
	_TODO // 2pc
}
VASResult
svas_client::recover2PC(
	IN(gtid_t)      gtid,       // 
	IN(CoordHandle) handle      // 
	OUT(gtid_t)     gtid        // OUT
)
{
	_TODO // 2pc
}

#endif /* notdef */

VASResult	
svas_client::mkRegistered(
	const Path 			name,	
	mode_t 				mode,
	IN(lrid_t) 			typeobj,
	IN(vec_t)			core,
	IN(vec_t) 			heap, 
	ObjectOffset 		tstart,	
	int					nindexes,
	OUT(lrid_t)			result	// not snapped
) 	// for user-defined types only
{
	FSTART(svas_client::mkRegistered,mkregistered,lrid_t);
	bool 	warn=false;

	PREVENTER;
	NULLPARAMCHECK(name,1);
	NULLPARAMCHECK(result,8);
	if(ISNULL(typeobj.serial)) {
		VERR( SVAS_BadParam3);
		VRETURN SVAS_FAILURE;
	}

	STRARG(a.name,Path,name);

	{ 	
		int	max = capacity();

		// if there's not enough room to do this
		// in one message...
		if( _batch->preflush(core) ||
			_batch->preflush(heap)) {
			VASResult res;

			// break into multiple requests :
			// create uninitialized, then write in chunks

			ADJUST_STAT(mkregistered2,-1);
			++_stats.mwrite_rpcs; // for the mkRegistered()
			++_stats.mwrite_calls; // for the mwrites()

			res = this->mkRegistered(name,mode,typeobj,
				core.size(), heap.size(),
				tstart, nindexes, result);

			if(res!=SVAS_OK) {
				FAIL;
			}
			res = mwrites(*result, core, 0, max);
			if(res!=SVAS_OK) {
				FAIL;
			}
			res = mwrites(*result, heap, core.size(), max);
			if(res!=SVAS_OK) {
				FAIL;
			}
			VRETURN SVAS_OK;
		}

		// else there is enough room for 1 msg, so 
		// go ahead...
		_batch->push(a.core, core);
		_batch->push(a.heap, heap);
	}
	a.nindexes = nindexes;
	a.mode = mode;
	a.type = typeobj;
	a.tstart = tstart;

	SVCALL(mkregistered,lrid_t);

	result->lvid = reply->result.lvid;
	result->serial = reply->result.serial;

	_batch->pop(a.heap);
	_batch->pop(a.core);
	_batch->svcalled();

	WARNREPLY(warn,SVAS_NotEnoughShm);
failure:
 	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::mkRegistered(
	const Path 			name,	
	mode_t 				mode,	
	IN(lrid_t) 			typeobj,
	ObjectSize			csize,	
	ObjectSize			hsize,	
	ObjectOffset   	    tstart,
	int					nindexes,
	OUT(lrid_t)			result
)
{
	FSTART(svas_client::mkRegistered,mkregistered2,lrid_t);
	NULLPARAMCHECK(name,1);
	NULLPARAMCHECK(result,8);
	if(ISNULL(typeobj.serial)) {
		VERR( SVAS_BadParam2);
		VRETURN SVAS_FAILURE;
	}

	STRARG(a.name,Path,name);
	a.mode = mode;
	a.type = typeobj;

	a.nindexes = nindexes;
	a.csize = csize;
	a.hsize = hsize;
	a.tstart = tstart;

	SVCALL(mkregistered2,lrid_t);

	result->lvid = reply->result.lvid;
	result->serial = reply->result.serial;

	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult	
svas_client::rmLink1(
	const Path 			name,	
	OUT(lrid_t) 		obj,	
	OUT(bool)			must_remove 
)
{
	FSTART(svas_client::rmLink1,rmlink1,rmlink1);
	NULLPARAMCHECK(name,1);
	NULLPARAMCHECK(obj,2);
	NULLPARAMCHECK(must_remove,3);
	STRARG(a.name,Path,name);
	SVCALL(rmlink1,rmlink1);
	*obj		 = reply->obj;
	*must_remove = reply->must_remove;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult	
svas_client::rmLink2(
	IN(lrid_t) 			obj	// 
)
{
	FSTART(svas_client::rmLink2,rmlink2,void);
	a = obj;
	SVCALL(rmlink2,void);
	REPLY;
// failure:
// 	VRETURN SVAS_FAILURE;
}

VASResult	
svas_client::mkPool(
	const Path 			name,	
	mode_t				mode,	
	OUT(lrid_t)			result	// not snapped
)
{
	FSTART(svas_client::mkPool,mkpool,lrid_t);
	NULLPARAMCHECK(name,1);
	NULLPARAMCHECK(result,3);
	STRARG(a.name,Path,name);
	a.mode = mode;

	DBG(
		<< "mkPool... name is " << a.name
	)

	SVCALL(mkpool,lrid_t);
	result->lvid = 
		reply->result.lvid;
	result->serial = 
		reply->result.serial;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult	
svas_client::rmPool(
	const 	Path	 	name	
)
{
	FSTART(svas_client::rmPool,rmpool,void);
	NULLPARAMCHECK(name,1);
	STRARG(a.name,Path,name);
	SVCALL(rmpool,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
#ifdef notdef
/*
 * case 1: uses path name
 *
 * OBSOLETE: user must do lookup on pool first
 */

VASResult	
svas_client::mkAnonymous(
	const	Path		poolname,	
	IN(lrid_t) 			typeobj,
	IN(vec_t)			core, 	
	IN(vec_t) 			heap, 	
	ObjectOffset		tstart,
	int					nindexes,
	INOUT(lrid_t)		result,  // if result->serial == serial_t::null
								// we have to allocate a ref
	INOUT(lrid_t)		pooloid  // 
)
#endif

/*
 * case 2:
 * initialized data,
 * serial number will be allocated
 * if necessary
 */

VASResult
svas_client::mkAnonymous(
	IN(lrid_t) 			pool,	
	IN(lrid_t) 			typeobj,
	IN(vec_t)			core, 	
	IN(vec_t) 			heap, 
	ObjectOffset		tstart,	
	int					nindexes,
	INOUT(lrid_t)		result,  // if result->serial == serial_t::null
								// we have to allocate a ref
	INOUT(void)			phys  //  MUST BE ZERO for client side
	// (for now)
)
{
	FSTART(svas_client::mkAnonymous,mkanonymous2,mkanon);
	bool 	warn=false;

	PREVENTER;
	NULLPARAMCHECK(result,7);

	if(!ISNULL(result->serial)) {
		if( !(ISEQV(result->lvid,pool.lvid))) {
			VERR( SVAS_BadParam9 );
			VRETURN SVAS_FAILURE;
		}
	}
	// for now, anyway, client side doesn't get the phys id back
	if(phys!=NULL) {
		VERR( SVAS_BadParam9 );
		FAIL;
	}
	if(ISNULL(typeobj.serial)) {
		VERR( SVAS_BadParam2);
		VRETURN SVAS_FAILURE;
	}
	{ 	
		int	max = capacity();

		// if there's not enough room to do this
		// in one message...
		if( _batch->preflush(core) ||
			_batch->preflush(heap)) {
			VASResult res;

			// break into multiple requests :
			// create uninitialized, then write in chunks

			ADJUST_STAT(mkanonymous4,-1);
			++_stats.mwrite_rpcs;		// for the mkAnonymous
			++_stats.mwrite_calls; // for the mwrites()

			res = this->mkAnonymous(pool,typeobj,
				core.size(), heap.size(),
				tstart, nindexes, result);

			if(res!=SVAS_OK) {
				FAIL;
			}
			res = mwrites(*result, core, 0, max);
			if(res!=SVAS_OK) {
				FAIL;
			}
			res = mwrites(*result, heap, core.size(), max);
			if(res!=SVAS_OK) {
				FAIL;
			}
			VRETURN SVAS_OK;
		}

		// else there is enough room for 1 msg, so 
		// go ahead...
		_batch->push(a.core, core);
		_batch->push(a.heap, heap);
	}
	a.nindexes = nindexes;
	a.poolobj = pool;
	a.type = typeobj;
	a.tstart = tstart;
	a.ref = result->serial;

	SVCALL(mkanonymous2,mkanon);
	_batch->pop(a.heap);
	_batch->pop(a.core);
	_batch->svcalled();

#ifdef DEBUG
	if(!ISNULL(result->serial)) {
		dassert(ISEQ(reply->result.serial,result->serial));
		DBG(<<"CALLER PROVIDED" << result->serial.data._low);
		dassert(ISEQV(reply->result.lvid,result->lvid));
	}
#endif
	*result = reply->result;
	WARNREPLY(warn,SVAS_NotEnoughShm);
failure:
	VRETURN SVAS_FAILURE;
}

/*
 * case 3
 * initialized data,
 * serial number privided by user
 */

VASResult	
svas_client::mkAnonymous(
	IN(lrid_t) 			pool,	
	IN(lrid_t) 			typeobj,
	IN(vec_t)			core, 	
	IN(vec_t) 			heap, 
	ObjectOffset		tstart,	
	int					nindexes,
	IN(lrid_t)			result // may not be serial_t::null
)
{
	FSTART(svas_client::mkAnonymous,mkanonymous3,void);
	bool 	warn=false;

	if(ISNULL(typeobj.serial)) {
		VERR( SVAS_BadParam2);
		VRETURN SVAS_FAILURE;
	}
	if(ISNULL(result.serial)) {
		VERR( SVAS_BadParam7);
		VRETURN SVAS_FAILURE;
	}

	{
		BATCH1(MkAnon3Req,mkanonymous3,void);

		{ 	
			int	max = capacity();

			// if there's not enough room to do this
			// in one message...
			if( _batch->preflush(core) ||
				_batch->preflush(heap)) {
				VASResult res;

				// break into multiple requests :
				// create uninitialized, then write in chunks

				ADJUST_STAT(mkanonymous5,-1);
				++_stats.mwrite_rpcs;		// for the mkAnonymous()
				++_stats.mwrite_calls; // for the mwrite()

				res = this->mkAnonymous(pool,typeobj,
					core.size(), heap.size(),
					tstart, nindexes, result);

				if(res!=SVAS_OK) {
					FAIL;
				}
				res = mwrites(result, core, 0, max);
				if(res!=SVAS_OK) {
					FAIL;
				}
				res = mwrites(result, heap, core.size(), max);
				if(res!=SVAS_OK) {
					FAIL;
				}
				VRETURN SVAS_OK;
			}

			// else there is enough room for 1 msg, so 
			// go ahead...
		}

		a.tstart = tstart;
		a.ref = result.serial;
		DBG(<<"CALLER PROVIDED" << result.serial.data._low);
		a.nindexes = nindexes;
		a.poolobj = pool;
		a.type = typeobj;

		_batch->push(a.core, core);
		_batch->push(a.heap, heap);
		BATCH2(MkAnon3Req,mkanonymous3,void);
		_batch->pop(a.heap);
		_batch->pop(a.core);
		BATCH3(MkAnon3Req,mkanonymous3,void);

	}
	WARNREPLY(warn,SVAS_NotEnoughShm);
failure:
	VRETURN SVAS_FAILURE;
}

/*
 * case 4:
 * uninitialized data,
 * serial number allocated by server if necessary
 */
VASResult	
svas_client::mkAnonymous(
	IN(lrid_t) 			pool,	
	IN(lrid_t) 			typeobj,
	ObjectSize			csize, 	
	ObjectSize			hsize, 	
	ObjectOffset		tstart,	
	int					nindexes,
	INOUT(lrid_t)		result  // if result->serial == serial_t::null
								// we have to allocate a ref
)
{
	FSTART(svas_client::mkAnonymous,mkanonymous4,mkanon);

	PREVENTER;
	if(ISNULL(typeobj.serial)) {
		VERR( SVAS_BadParam2);
		VRETURN SVAS_FAILURE;
	}
	NULLPARAMCHECK(result,7);
	if(!ISNULL(result->serial)) {
		if( !(ISEQV(result->lvid,pool.lvid))) {
			VERR( SVAS_BadParam9 );
			VRETURN SVAS_FAILURE;
		}
	}
	a.poolobj = pool;
	a.type = typeobj;
	a.csize = csize;
	a.hsize = hsize;
	a.ref = result->serial;
	a.nindexes = nindexes;
	a.tstart = tstart;

	SVCALL(mkanonymous4,mkanon);

#ifdef DEBUG
	if(!ISNULL(result->serial)) {
		dassert(ISEQ(reply->result.serial,result->serial));
		DBG(<<"CALLER PROVIDED" << result->serial.data._low);
		dassert(ISEQV(reply->result.lvid,result->lvid));
	}
#endif
	*result = reply->result;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

/*
 * case 5:
 * uninitialized data,
 * serial number provided by user
 */
VASResult	
svas_client::mkAnonymous(
	IN(lrid_t) 			pool,	
	IN(lrid_t) 			typeobj,
	ObjectSize			csize, 	
	ObjectSize			hsize, 	
	ObjectOffset		tstart,	
	int					nindexes,
	IN(lrid_t)			result // may not be serial_t::null
)
{
	FSTART(svas_client::mkAnonymous,mkanonymous5,void);

	if(ISNULL(typeobj.serial)) {
		VERR( SVAS_BadParam2);
		VRETURN SVAS_FAILURE;
	}
	if(ISNULL(result.serial)) {
		VERR( SVAS_BadParam7);
		VRETURN SVAS_FAILURE;
	}
	BATCH1(MkAnon5Req,mkanonymous5,void);

	a.poolobj = pool;
	a.type = typeobj;
	a.csize = csize;
	a.hsize = hsize;
	a.ref = result.serial;
	DBG(<<"CALLER PROVIDED" << result.serial.data._low);
	a.nindexes = nindexes;
	a.tstart = tstart;

	BATCH2(MkAnon5Req,mkanonymous5,void);
	BATCH3(MkAnon5Req,mkanonymous5,void);

	BREPLY;
failure:
	VRETURN SVAS_FAILURE;
}


VASResult	
svas_client::rmAnonymous(
	IN(lrid_t) 			obj,
	OUT(lrid_t)			pooloid
)
{
	FSTART(svas_client::rmAnonymous,rmanonymous,lrid_t);
	a = obj;
	SVCALL(rmanonymous,lrid_t);
	if(pooloid && reply->status.vasresult == SVAS_OK) {
		*pooloid = reply->result;
	}
	REPLY;
// failure:
// 	VRETURN SVAS_FAILURE;
}

VASResult	
svas_client::mkXref(
	const Path 		name, 
	mode_t			mode,
	IN(lrid_t)		obj,	
	OUT(lrid_t)		result	// not snapped
)
{
	FSTART(svas_client::mkXref,mkxref,lrid_t);
	NULLPARAMCHECK(name,1);
	NULLPARAMCHECK(result,4);
	STRARG(a.name,Path,name);
	a.mode = mode;
	a.object = obj;
	SVCALL(mkxref,lrid_t);
	result->lvid = 
		reply->result.lvid;
	result->serial = 
		reply->result.serial;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::readRef(
	const Path	 name,		
	OUT(lrid_t)	result			// not snapped
)
{
	FSTART(svas_client::readRef,readxref,lrid_t);
	NULLPARAMCHECK(name,1);
	NULLPARAMCHECK(result,2);
	STRARG(a.name,Path,name);
	SVCALL(readxref,lrid_t);
	result->lvid = reply->result.lvid;
	result->serial = reply->result.serial;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::readRef(
	IN(lrid_t)	obj,
	OUT(lrid_t)	result			// not snapped
)
{
	FSTART(svas_client::readRef,readxref2,lrid_t);
	NULLPARAMCHECK(result,2);
	a.object = obj;
	SVCALL(readxref2,lrid_t);
	result->lvid = reply->result.lvid;
	result->serial = reply->result.serial;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
#ifdef notdef
VASResult	
svas_client::rmXref(
	const Path	 name	
)
{
	FSTART(svas_client::rmXref,rmxref,void);
	STRARG(a.name,Path,name);
	SVCALL(rmxref,void);
	REPLY;
// failure:
// 	VRETURN SVAS_FAILURE;
}
#endif

VASResult	
svas_client::mkLink(
	const 	Path	 oldpath,
	const 	Path	 newpath
)
{
	FSTART(svas_client::mkLink,mklink,void);
	NULLPARAMCHECK(oldpath,1);
	NULLPARAMCHECK(newpath,2);
	STRARG(a.oldpath,Path,oldpath);
	STRARG(a.newpath,Path,newpath);
	SVCALL(mklink,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult	
svas_client::reName(
	const 	Path	 oldpath,	
	const 	Path	 newpath
)
{
	FSTART(svas_client::reName,rename1,void);
	NULLPARAMCHECK(oldpath,1);
	NULLPARAMCHECK(newpath,2);
	STRARG(a.oldpath,Path,oldpath);
	STRARG(a.newpath,Path,newpath);
	SVCALL(rename1,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult	
svas_client::mkSymlink(
	const 	Path 	name,	
	const 	Path 	contents,
	mode_t			mode,		// = 0777
	OUT(lrid_t)		result		// =NULL not snapped
)
{
	FSTART(svas_client::mkSymlink,mksymlink,lrid_t);
	NULLPARAMCHECK(name,1);
	NULLPARAMCHECK(contents,3);
	STRARG(a.name,Path,name);
	a.mode = mode;
	STRARG(a.contents,Path,contents);
	SVCALL(mksymlink,lrid_t);
	if(result != 0) {
		result->lvid = reply->result.lvid;
		result->serial = reply->result.serial;
	}
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client:: readLink(
	const	Path	symname,
	IN(vec_t)		buf,	// for the data
	OUT(ObjectSize)	buflen	
)
{
	FSTART(svas_client::readLink,readsymlink,readsymlink);
	NULLPARAMCHECK(symname,1);
	NULLPARAMCHECK(buflen,3);
	STRARG(a.symname,Path,symname);
	a.contents_limit		= buf.size();
	SVCALL(readsymlink,readsymlink);
	if(reply->contents.opaque_t_val !=0 && reply->contents.opaque_t_len != 0) {
		buf.copy_from(reply->contents.opaque_t_val,reply->contents.opaque_t_len);
	}
	*buflen = reply->contents.opaque_t_len;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client:: readLink(
	IN(lrid_t)		symobj, 	// oid of symlink
	IN(vec_t)		buf,	// for the data
	OUT(ObjectSize)	buflen	
)
{
	FSTART(svas_client::readLink,readsymlink2,readsymlink);
	NULLPARAMCHECK(buflen,3);
	a.object = symobj;
	a.contents_limit		= buf.size();
	SVCALL(readsymlink2,readsymlink);
	if(reply->contents.opaque_t_val !=0 && reply->contents.opaque_t_len != 0) {
		buf.copy_from(reply->contents.opaque_t_val,reply->contents.opaque_t_len);
	}
	*buflen = reply->contents.opaque_t_len;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client:: lockObj(
	IN(lrid_t) 			obj,	
	LockMode			lock,	
	RequestMode			ok2block
)
{
	FSTART(svas_client::lockObj,lockobj,void);
	a.obj = obj;
	a.mode = lock;
	a.ok2block = ok2block;

	SVCALL(lockobj,void);
	REPLY;
// failure:
// 	VRETURN SVAS_FAILURE;
}

VASResult	
svas_client::readObj(
	IN(lrid_t) 			obj,	
	ObjectOffset		objoffset,	
	ObjectSize			nbytes,
	LockMode			lock,	
	IN(vec_t)			data,	// vec is in, data are out 
	OUT(ObjectSize)		data_len,// bytes used
	OUT(ObjectSize)		more, // more to read?
	OUT(lrid_t)			snapped, // =NULL snapped ref to obj
	OUT(bool)			pagecached // = NULL
)
{
	// 	gets whatever lock you ask for, + read lock
	smsize_t			limit, bufoff, bytes_this_call;
	bool				found_on_page = false;
	VASResult			x = SVAS_OK;
	bool				contacted_server=false;

	FSTART(svas_client::readObj,readobj,readobj);

	PREVENTER;

	audit();
	reply = NULL;

	NULLPARAMCHECK(data_len,6);
	NULLPARAMCHECK(more,7);

// no need for this check since we return how much left in more
//	if((nbytes!=WholeObject) && (limit < nbytes)) {
//		VERR(SVAS_BadParam3); // buf too small
//		VRETURN SVAS_FAILURE;
//	}
	checkflags(false); // not xfer time yet

on_page:
	{
		// locate the object on the page
		ObjectSize 	len;
		ccaddr_t	o;

		DBG( << "trying to locate object " 
			<< obj.serial.data._low );

		if(locate_object(obj, o, lock, &len, &found_on_page) != SVAS_OK) {
			dassert(0); // locate_object only  returns SVAS_OK for now
		}
		if(found_on_page) {
			DBG( << "FOUND len " << len );

			if(objoffset >= len) {
				VERR(SVAS_BadRange);
				x = SVAS_FAILURE;
				// don't change *more or *data_len
			} else {
				if(nbytes == WholeObject) 
					nbytes = len-objoffset;
				if(objoffset + nbytes > len) {
					VERR(SVAS_BadRange);
					x = SVAS_FAILURE;
					// don't change *more or *data_len
				} else {
					dassert(nbytes != WholeObject);
					// copy_from won't copy more than the
					// vec has room for
					data.copy_from(o+objoffset, nbytes, 0);
					*data_len = lesser(nbytes,data.size());
					*more = nbytes-(*data_len);
				}
			}
			replaceflags(vf_no_xfer,vf_sm_page);
			goto done; 
		}
		DBG( << "NOT FOUND");
		// could be large object
	}

	// I can re-use a because it doesn't get freed.
	a.obj = obj;
	a.lock = lock;
	// figure out what page(s) we have available
	a.pageoffset = replace_page();

	// *more: there is more to read (after first pass, it's # bytes left
	// objoffset: first byte initially requested
	// nbytes: # bytes initially requested
	// *datalen: place to put # bytes read this call
	// *more: place to put # bytes left after this call (to satisfy
	// 	 original request)-i.e., # bytes requested but not read
	//   because of buffer size limitations
	// limit: max # bytes that we want the server to send
	//   in each iteration below (imposed by pagesize)
	// bytes_this_call: # bytes read with this SM call
	// bufoff: offset into user's buffer

	bufoff = 0;
	limit = lesser( ((unsigned int)nbytes), page_size() * num_lg_bufs());
	// "limit" doesn't change

	*more = limit; // (for now)
	bytes_this_call = 0;
	*data_len = 0;

	while( ((*more) >0  && (int)(data.size()-bufoff) > 0) || 
		// make sure you contact the server in order to get
		// permissions errors, etc, even if size of object == 0
		// or #desired bytes == 0
		!contacted_server ){

		// limit amt transferred to the smallest of:
		// pagesize(), amnt originally requested,
		// space left in data vector
		//
		DBG(<<"limit=" << limit
			<< " data.size()=" << data.size()
			<<" bufoff = " << bufoff);

		a.data_limit = lesser(limit, (data.size()-bufoff)); // upper bound
		a.start = objoffset;
		a.end = nbytes; // could be WholeObject
		// a.end is *not* ending byte, rather it's #bytes

		DBG( << "readObj: more=" << *more
			<< " page_size= " << page_size()
			<< " limit= " << limit
		);

		if(reply) {
			DBG(<< "freeing space from prev reply");
			// free the space assoc with the prev reply
			if(!CLNT_FREERES((CLIENT *)this->cl, _freeproc, reply)){
				assert(0); /* internal error */
			}
			reply = NULL;
		}
		DBG(
			<< " a.data_limit= " << a.data_limit
			<< " a.start= " << a.start
			<< " a.end= " << a.end
		);
		SVCALL(readobj,readobj); // returns if no reply
		contacted_server = true;

		if((x=reply->status.vasresult) != SVAS_OK)  {
			dassert(!reply->commonpart.sent_small_obj_page);
			dassert(!reply->commonpart.obj_follows_bytes);
			replaceflags(vf_obj_follows|vf_sm_page, vf_no_xfer);

			checkflags(false); // not xfer time 
			DBG(<< "Error from SV CALL");
			break;
		}

		dassert(reply!=NULL);
		if(reply->commonpart.sent_small_obj_page) {
			DBG(<< "Server sent small object page");
			// small object page only -- breaks the loop
			dassert(reply->commonpart.obj_follows_bytes==0);

			replaceflags(vf_no_xfer,vf_sm_page);

			if(pagecached) *pagecached = true;

			if(over_the_wire()) {
				// case_a

				DBG( << "PAGE SENT OVER THE WIRE" );
				dassert(shm.base() == NULL);
				dassert(reply->commonpart.data.opaque_t_len == page_size());

				// exchange pages with the page cache
				reply->commonpart.data.opaque_t_val = putpage(a.pageoffset, 
					reply->commonpart.data.opaque_t_val, page_size());
				// places the page in the cache but does
				// not put the objects into the hash table

			} else {
				// case_b
				DBG(<< "over shared memory");

				dassert(_flags & vf_shm);
				dassert(reply->commonpart.data.opaque_t_len == 0);
			}

			installpage(obj, a.pageoffset);
			goto on_page;

		} else {

			bytes_this_call = reply->commonpart.obj_follows_bytes;
			if(bytes_this_call > 0) {

				DBG(<<"obj_follows_bytes=" << bytes_this_call);

				// large or registered object 
				// (didn't come with a page)

				dassert(reply->commonpart.sent_small_obj_page == false);
				// dassert(reply->commonpart.data.opaque_t_len >= 0); unsigned now

				replaceflags(vf_no_xfer,vf_obj_follows);

				dassert(bytes_this_call <= a.data_limit);
				dassert(a.data_limit <= (unsigned)limit);

				if(over_the_wire()) {
					// case_c
					dassert(bytes_this_call == reply->commonpart.data.opaque_t_len);
					if(reply->commonpart.data.opaque_t_val !=0 && bytes_this_call != 0) {
						data.copy_from(reply->commonpart.data.opaque_t_val, bytes_this_call, bufoff);
					}
				} else {
					// case_d
					// it's in shm
					dassert(reply->commonpart.data.opaque_t_len == 0);
					dassert(num_lg_bufs() > 0);

					data.copy_from(lg_buf(),bytes_this_call, bufoff);
				}
				*data_len += bytes_this_call;
				bufoff += bytes_this_call;
				objoffset += bytes_this_call;
				*more = reply->commonpart.more;

				if(nbytes == WholeObject) {
					nbytes = reply->commonpart.more; // whatever's left
				} else  {
					nbytes -= bytes_this_call;
				}
			} 
		}
	} 

done:
	// limit should be zero except that what the
	// user gave might have been way larger than the object

	if(x == SVAS_OK) {
		if(contacted_server) {
			checkflags(true); // xfer done 
		}

		if(snapped) {
			if(reply) {
				snapped->lvid = reply->commonpart.snapped.lvid;
				snapped->serial = reply->commonpart.snapped.serial;
			} else  {
				*snapped = obj; 
			}
		}
	}
	// Don't do VECOUTPUT(data) because the copy was done explicitly, above 

	replaceflags(vf_obj_follows|vf_sm_page, vf_no_xfer);
	checkflags(false); // ready for next call

#ifdef DEBUG
	if(!found_on_page) {
		dassert(reply || !contacted_server);
	}  
	if(!reply) {
		dassert(found_on_page || !contacted_server);
	}
#endif

	if(reply) {
		REPLY;
	} else {
		audit();
		VRETURN x;
	}
failure:
	// don't touch *pagecached
	VRETURN SVAS_FAILURE;

}

VASResult		
svas_client::snapRef(
	IN(lrid_t) 		off,		// off-vol ref
	OUT(lrid_t) 	snapped	
)
{
	FSTART(svas_client::snapRef,snapref,lrid_t);
	NULLPARAMCHECK(snapped,2);
	a 	= off;
	SVCALL(snapref,lrid_t);
	*snapped = reply->result;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::mkVolRef(
	IN(lvid_t)		onvol,	// on this volume
	OUT(lrid_t) 	result,	// is snapped
	int				number	// - number of refs to create
)
{
	FSTART(svas_client::mkVolRef,mkvolref,lrid_t);
	NULLPARAMCHECK(result,2);
	a.onvolume 	= onvol;
	a.number 	= number;
	SVCALL(mkvolref,lrid_t);
	dassert(ISEQV(reply->result.lvid,onvol));
	*result = reply->result;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::offVolRef(
	IN(lvid_t)		fromvol,
	IN(lrid_t) 		toobj,
	OUT(lrid_t) 	result		// definitely not snapped
)
{
	FSTART(svas_client::offVolRef,offvolref,lrid_t);
	NULLPARAMCHECK(result,3);

	// shortcut -- server does this anyway:
	// if they're on the same volume, don't 
	// make a new ref, and save an RPC
	if(ISEQV(fromvol,toobj.lvid)) {
		result->lvid = toobj.lvid;
		result->serial = toobj.serial;
	} else {
		a.from_volume 	= fromvol;
		a.to_loid		= toobj;
		SVCALL(offvolref,lrid_t);
		result->lvid = reply->result.lvid;
		result->serial = reply->result.serial;
		REPLY;
	}
	VRETURN SVAS_OK;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::openPoolScan(
	IN(lrid_t)			pool,	
	OUT(Cookie)			cookie
)
{
	FSTART(svas_client::openPoolScan,openpoolscan1,Cookie);
	NULLPARAMCHECK(cookie,2);
	a.pool		= pool;
	SVCALL(openpoolscan1,Cookie);
	*cookie = reply->result;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::openPoolScan(
	const	Path		name,	
	OUT(Cookie)			cookie	
)
{
	FSTART(svas_client::openPoolScan,openpoolscan2,Cookie);
	NULLPARAMCHECK(name,1);
	NULLPARAMCHECK(cookie,2);
	STRARG(a.name,Path,name);
	SVCALL(openpoolscan2,Cookie);
	*cookie = reply->result;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::_nextPoolScan(
	INOUT(Cookie)		cookie,	
	OUT(bool)			eof,	// true if no result
						// if false, result is legit
	OUT(lrid_t)			result,
	bool				nextpage	//=false
)
{
	FSTART(svas_client::_nextPoolScan,nextpoolscan1,nextpoolscan1);
	NULLPARAMCHECK(cookie,1);
	NULLPARAMCHECK(eof,2);
	NULLPARAMCHECK(result,3);

	a.cookie = *cookie;

	// just to be safe
	a.lock = NL;
	a.wantsysprops = false;
	a.pageoffset = 0;
	a.start = 0;
	a.requested = 0;
	a.data_limit = 0;
	a.nextpage= false;

	SVCALL(nextpoolscan1,nextpoolscan1);
	*cookie = reply->cookie;
	*eof	= reply->eof;
	result->lvid = reply->commonpart.snapped.lvid;
	result->serial = reply->commonpart.snapped.serial;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
// public version:
VASResult		
svas_client::nextPoolScan(
	INOUT(Cookie)		cookie,	
	OUT(bool)			eof,	// true if no result
						// if false, result is legit
	OUT(lrid_t)			result
)
{
	return _nextPoolScan(cookie,eof,result,false);
}

VASResult		
svas_client::_nextPoolScan(
	INOUT(Cookie)		cookie,	
	OUT(bool)			eof,	// true if no result
								// if false, result is legit
	OUT(lrid_t)			result, 	// snapped ref
	ObjectOffset		start,
	ObjectSize			requested,	// -- could be WholeObject
	IN(vec_t)			data,	// -- where to put it
	OUT(ObjectSize)		used,	// - amount copied
	OUT(ObjectSize)		more_notused,	// requested amt not copied TODO -- use
	LockMode			lock, //  =NL
	OUT(SysProps)		sysprops,// =NULL
	OUT(int)			sysp_size, // =NULL
	bool				nextpage	//=false
)
{
	FSTART(svas_client::_nextPoolScan,nextpoolscan2,nextpoolscan2);
	int	limit = (int)data.size();
	VASResult x=SVAS_OK;

	NULLPARAMCHECK(cookie,1);
	NULLPARAMCHECK(used,6);
	NULLPARAMCHECK(eof,7);
	NULLPARAMCHECK(result,8);
	if(sysprops) {
		a.wantsysprops = true;
	} else {
		a.wantsysprops = false;
	}
	a.cookie = *cookie;
	a.start	 = start;
	a.requested	= requested;
	a.data_limit	= limit;
	a.lock	= lock;
	a.nextpage= nextpage;
	SVCALL(nextpoolscan2,nextpoolscan2);

	// TODO: read object from SHM or from message
	// TODO: what if object is large?

	if(reply->commonpart.data.opaque_t_val !=0 && limit != 0) {
		data.copy_from(reply->commonpart.data.opaque_t_val, limit);
	}

	*used = reply->commonpart.data.opaque_t_len;
	*cookie = reply->cookie;
	*eof	= reply->eof;
	result->lvid = reply->commonpart.snapped.lvid;
	result->serial = reply->commonpart.snapped.serial;
	if(sysprops) {
		if(reply->commonpart.sent_small_obj_page) {
			// just call sysprops() to get it off the page
			x = this->sysprops(*result,sysprops,true/*irrelevant*/, SH, 0, 0);
			if(x!=SVAS_OK) {
				VERR(SVAS_InternalError);
				FAIL;
			}
		} else {
			// get it from the reply  -- already byte-swapped
			*sysprops =  convert_sysprops(reply->sysprops);
			dassert(sysprops->type._low & 0x1);
		}
	}
	if(sysp_size) {
		*sysp_size = reply->sysp_size;
	}
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

// public version:
VASResult		
svas_client::nextPoolScan(
	INOUT(Cookie)		cookie,	
	OUT(bool)			eof,	// true if no result
						// if false, result is legit
	OUT(lrid_t)			result, 	// snapped ref
	ObjectOffset		offset,
	ObjectSize			requested,	// -- could be WholeObject
	IN(vec_t)			buf,	// -- where to put it
	OUT(ObjectSize)		used,	// - amount copied
	OUT(ObjectSize)		more,	// requested amt not copied
	LockMode			lock,
	OUT(SysProps)		sysprops, // optional
	OUT(int)			sysp_size // optional
)
{
	bool				found_on_page = false;
	VASResult			x = SVAS_OK;

	FUNC(svas_client::nextPoolScan);

	// TODO: implement a scan locally

	return _nextPoolScan(cookie,eof,result,offset, 
		requested,buf,used,more,lock,sysprops,sysp_size,false);
}

VASResult		
svas_client::closePoolScan(
	IN(Cookie)			cookie	
)
{
	FSTART(svas_client::closePoolScan,closepoolscan,void);

	a.cookie = cookie;
	SVCALL(closepoolscan,void);
	REPLY;
//failure:
//	VRETURN SVAS_FAILURE;
}


// inserting, anonymous 
VASResult		
svas_client::insertIndexElem(
	IN(__IID__)			indexobj, // obj representing idx
	IN(vec_t)			key,
	IN(vec_t)			value	 
)
{
	FSTART(svas_client::insertIndexElem,inserta,void);
	a.indexobj = indexobj;

	if(key.size() == 0){
		VERR(SVAS_BadParam2);
		FAIL;
	}
	if(value.size() == 0){
		VERR(SVAS_BadParam3);
		FAIL;
	}
	CONVERTVEC(key);
	CONVERTVEC(value);

	SVCALL(inserta,void);

	UNCONVERTVEC(key);
	UNCONVERTVEC(value);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::removeIndexElem(
	IN(__IID__)			indexobj, // obj representing idx
	IN(vec_t)			key,
	OUT(int)			numremoved	// # entries removed
)
{
	FSTART(svas_client::removeIndexElem,remove2a,int);
	a.indexobj = indexobj;

	if(key.size() == 0){
		VERR(SVAS_BadParam2);
		FAIL;
	}
	NULLPARAMCHECK(numremoved,3);
	CONVERTVEC(key);
	SVCALL(remove2a,int);
	UNCONVERTVEC(key);
	*numremoved = reply->result;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::removeIndexElem(
	IN(__IID__)			indexobj, // obj representing idx
	IN(vec_t)			key,
	IN(vec_t)			value	 
)
{
	FSTART(svas_client::removeIndexElem,remove1a,void);
	a.indexobj = indexobj;

	if(key.size() == 0){
		VERR(SVAS_BadParam2);
		FAIL;
	}
	if(value.size() == 0){
		VERR(SVAS_BadParam3);
		FAIL;
	}
	CONVERTVEC(key);
	CONVERTVEC(value);
	SVCALL(remove1a,void);
	UNCONVERTVEC(key);
	UNCONVERTVEC(value);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::findIndexElem(
	IN(__IID__)			indexobj, // obj representing idx
	IN(vec_t)			key,
	IN(vec_t)			value,	  
	OUT(ObjectSize)		value_len, 
	OUT(bool)			found
)
{
	FSTART(svas_client::findIndexElem,finda,find);
	a.indexobj = indexobj;

	if(key.size() == 0){
		VERR(SVAS_BadParam2);
		FAIL;
	}
	if(value.size() == 0){
		VERR(SVAS_BadParam3);
		FAIL;
	}
	NULLPARAMCHECK(value_len,4);
	NULLPARAMCHECK(found,5);
	CONVERTVEC(key);
	OUTPUTVEC(value);
	SVCALL(finda,find);

	VECOUTPUT(value);
	UNCONVERTVEC(key);
	*found = reply->found;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

#define BOUNDCHECK(c,bound,which) \
	if(bound == vec_t::neg_inf) {\
		c = (CompareOp) ((unsigned int)c | (unsigned int)NegInf);\
	}\
	if(bound == vec_t::pos_inf) {\
		c = (CompareOp) ((unsigned int)c | (unsigned int)PosInf);\
	}\
	if((c==ltNegInf)||(c==leNegInf)||(c==gtPosInf)||(c==gePosInf)) {\
		VERR(which);\
		FAIL;\
	}

VASResult		
svas_client::openIndexScan(
	IN(__IID__) 			idx, 	// -- anon
	CompareOp			lc,		
	IN(vec_t)			lbound,	
	CompareOp			uc,		
	IN(vec_t)			ubound,	
	OUT(Cookie)			cookie
)
{
	FSTART(svas_client::openIndexScan,openindexscan2,Cookie);
	BOUNDCHECK(lc,lbound,SVAS_BadParam2);
	BOUNDCHECK(uc,ubound,SVAS_BadParam4);

	NULLPARAMCHECK(cookie,6);

	a.idx = idx;
	a.lc = lc;
	a.uc = uc;
	CONVERTVEC(lbound);
	CONVERTVEC(ubound);
	SVCALL(openindexscan2,Cookie);

	UNCONVERTVEC(lbound);
	UNCONVERTVEC(ubound);
	*cookie = reply->result;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::nextIndexScan(
	INOUT(Cookie)		cookie,	
	IN(vec_t)			key,	// "INOUT", actually
	OUT(ObjectSize)		keylen, // - bytes of vec used
	IN(vec_t)			value,	// "INOUT", actually
	OUT(ObjectSize)		valuelen, // - bytes of vec used
	INOUT(bool)		eof	// true if no result
						// if false, result is legit
)
{
	FSTART(svas_client::nextIndexScan,nextindexscan,nextindexscan);
	NULLPARAMCHECK(cookie,1);
	if(key.size() == 0){
		VERR(SVAS_BadParam2);
		FAIL;
	}
	NULLPARAMCHECK(keylen,3);
	if(value.size() == 0){
		VERR(SVAS_BadParam4);
		FAIL;
	}
	NULLPARAMCHECK(valuelen,5);
	NULLPARAMCHECK(eof,6);

	a.cookie = *cookie;
	a.key_limit = key.size();
	a.value_limit = value.size();

	SVCALL(nextindexscan,nextindexscan);

	// dts 10/10/94 values seem to be hosed if eof, so return them
	// only if not eof.
	if (reply->status.vasresult == SVAS_OK && !reply->eof) {
			key.copy_from(reply->key.opaque_t_val, a.key_limit);
			value.copy_from(reply->value.opaque_t_val, a.value_limit);
			*keylen = reply->key.opaque_t_len;
			*valuelen = reply->value.opaque_t_len;
	} else {
		*keylen = 0;
		*valuelen = 0;
	}

	*cookie = reply->cookie;
	*eof = reply->eof;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::closeIndexScan(
	IN(Cookie)			cookie
)
{
	FSTART(svas_client::closeIndexScan,closeindexscan,void);
	a.cookie = cookie;
	SVCALL(closeindexscan,void);
	REPLY;
// failure:
// 	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::lock_timeout(
	IN(locktimeout_t)	newtimeout,	
	OUT(locktimeout_t)	oldtimeout // null ptr if you're not interested
								   // in getting the old value
)
{
	FSTART(svas_client::locktimeout,locktimeout,int);
	a.new_timeout = newtimeout;
	SVCALL(locktimeout,int);
	if(oldtimeout) *oldtimeout = reply->result;
	REPLY;
}

VASResult		
svas_client::sdl_test(
	IN(int) ac,
	const Path av[10],
	OUT(int) rc
)
{
	FSTART(svas_client::sdltest,sdltest,int);
	a.argc = ac;
	int i;
	for (i=0; i<ac; i++)
		a.args[i] = av[i];
	for (i=ac; i<10;i++)
		a.args[i]= "";
	SVCALL(sdltest,int);
	if(rc) *rc = reply->result;
	REPLY;
	return SVAS_OK;
}

VASResult		
svas_client::chMod(
	const	Path 	name,		// in:
	mode_t			mode		// in
)
{
	FSTART(svas_client::chMod,chmod1,void);
	STRARG(a.path,Path,name);
	a.mode = mode;
	SVCALL(chmod1,void);
	REPLY;
//failure:
//	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::chOwn(
	const  Path 	name,		// in:
	uid_t			uid			// in
)
{
	FSTART(svas_client::chOwn,chown1,void);
	STRARG(a.path,Path,name);
	// chown uses chown rpc with uid argument
	a.uid = uid;
	// chgrp uses chown rpc with gid argument
	a.gid = (gid_t)-1;
	SVCALL(chown1,void);
	REPLY;
// failure:
// 	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::chGrp(
	const  Path 	name,		// in:
	gid_t			gid			// in
)
{
	FSTART(svas_client::chGrp,chown1,void);
	STRARG(a.path,Path,name);
	// chown uses chown rpc with uid argument
	a.uid = BAD_UID;
	// chgrp uses chown rpc with gid argument
	a.gid = gid;

	DBG(<<"path=" << a.path << " uid=" << a.uid
		<< " gid=" << a.gid);
	SVCALL(chown1,void);
	REPLY;
// failure:
// 	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::utimes(
	const  Path 	name,		// in:
	timeval			*tvpa,
	timeval			*tvpm
	// sets a and m times to this value
)
{
	FSTART(svas_client::utimes,utimes1,void);
	STRARG(a.path,Path,name);
	if(tvpa) {
		a.utimea.tv_sec = tvpa->tv_sec;
		a.utimea.tv_usec = tvpa->tv_usec;
	} else {
		a.utimea.tv_sec = -1;
		a.utimea.tv_usec = -1;
	}
	if(tvpm) {
		a.utimem.tv_sec = tvpm->tv_sec;
		a.utimem.tv_usec = tvpm->tv_usec;
	} else {
		a.utimem.tv_sec = -1;
		a.utimem.tv_usec = -1;
	}
	SVCALL(utimes1,void);
	REPLY;
// failure:
// 	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::utimes(
	IN(lrid_t)		target, // FOR EFSD and OC
	timeval			*tvpa,
	timeval			*tvpm
	// sets a and m times to this value
)
{
	FSTART(svas_client::utimes,utimes2,void);
	a.target = target;
	if(tvpa) {
		a.utimea.tv_sec = tvpa->tv_sec;
		a.utimea.tv_usec = tvpa->tv_usec;
	} else {
		a.utimea.tv_sec = -1;
		a.utimea.tv_usec = -1;
	}
	if(tvpm) {
		a.utimem.tv_sec = tvpm->tv_sec;
		a.utimem.tv_usec = tvpm->tv_usec;
	} else {
		a.utimem.tv_sec = -1;
		a.utimem.tv_usec = -1;
	}
	SVCALL(utimes2,void);
	REPLY;
// failure:
// 	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::setUmask(
	unsigned int	umask		// in
)
{
	FSTART(svas_client::setUmask,setumask,void);
	DBG(<<"setting umask to " << umask);
	a.umask = umask;
	DBG(<<"setting umask to " << a.umask);
	SVCALL(setumask,void);
	REPLY;
// failure:
// 	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::getUmask(
	OUT(unsigned int) result
)
{
	FSTART(svas_client::getUmask,getumask,u_int);
	SVCALL(getumask,u_int);
	*result = reply->result;
	DBG(<<"got umask " << reply->result);
	REPLY;
// failure:
// 	VRETURN SVAS_FAILURE;
}
#ifdef DEBUG
extern "C" void	print_gather_reply(gather_stats_reply *);
#endif

VASResult			
svas_client::gather_remote( 
	w_statistics_t		&where
)
{
	FSTART(svas_client::gatherStats,gather_stats,gather_stats);

	//
	// Since we have to overwrite the values, they
	// had better be dynamic/writable
	//
	if( !where.writable() ) {
		VERR(SVAS_InternalError);
	}
	a.signature = where.signature();

	SVCALL(gather_stats,gather_stats);

	if(reply->signature != where.signature()) {
		DBG(<<"blowing away orig remote stats! reply sig=" 
			<<reply->signature << " where.sig = " << where.signature());
		where.make_empty();
	}

#ifdef DEBUG
	// print_gather_reply(reply);
#endif

	int					i = reply->modules.modules_len;
	stats_module		*m = reply->modules.modules_val;
	const stat_values 	*values;
	const char			*types=0;
	const char			*descr=0;
	const char			**msgs=0;
	bool				d_malloced, t_malloced, s_malloced;


	// convert from the RPC form to the w_statistics_t form:

	// The reply is malloced, but the w_stat_module_t
	// uses malloc/free for the descr, strings, types and values
	// so we can just steal those parts from the reply.

	for(int j=0; j < i; j++, m++) {
		int count = m->count;
		DBG(<<"module with  " << m->count << " parts: " );
		DBG(<<"i=" << i  << " j=" << j);


		{ 	// get types 
			if(m->types.tag == nonnull) {
				dassert(reply->signature != where.signature());
				types = m->types.possibly_null_string_u.str;
				t_malloced = true; 
				// steal types string from reply
				m->types.possibly_null_string_u.str = 0;
				m->types.tag = isnull;
			} else {
				dassert(reply->signature == where.signature());
				types = where.typestring(m->base);
				dassert(types != 0);
				t_malloced = false; 
			}
			DBG(<<"strlen(types)=" << strlen(types) <<" count=" << count);
			dassert(strlen(types)==count);
		}
		{ 	// get descr 
			if(m->descr.tag == nonnull) {
				dassert(reply->signature != where.signature());
				descr = m->descr.possibly_null_string_u.str;
				d_malloced = true; 
				// steal descr string from reply
				m->descr.possibly_null_string_u.str = 0;
				m->descr.tag = isnull;
			} else {
				dassert(reply->signature == where.signature());
				descr = where.module(m->base);
				dassert(descr != 0);
				d_malloced = false; 
			}
		}

		// steal the strings stuff from the reply structure
		{
			if(m->msgs.msgs_len == count) {
				dassert(reply->signature != where.signature());
				s_malloced = true;
				msgs = (const char **)m->msgs.msgs_val;

				//  prevent error when reply is freed
				m->msgs.msgs_val = 0; m->msgs.msgs_len = 0;
			} else {
				dassert(reply->signature == where.signature());
				dassert(m->msgs.msgs_len == 0);
				msgs = (const char **)where.getstrings(m->base,false);
				s_malloced = false;
			}
		}

		// add_module frees old values in module if necessary
		// (but we already made it empty)

		assert(m->values.values_len == count);
		values = m->values.values_val;

		w_stat_t 		*temp = new w_stat_t[count];
		// temp will be stashed in the statistics_t
		// and will be delete[]-ed by that module
		if(!temp) {
			cerr << "malloc failed" << endl;
			assert(0);
		}


		const char		*t = types;
		for(int k=0; k<count; k++,values++,t++) {
			switch(*t) {
				case 'v':
					dassert(values->tag == __v);
					temp[k] = values->stat_values_u._v;
					break;
				case 'l':
					dassert(values->tag == __l);
					temp[k] = values->stat_values_u._l;
					break;
				case 'i':
					dassert(values->tag == __i);
					temp[k] = values->stat_values_u._i;
					break;
				case 'u':
					dassert(values->tag == __u);
					temp[k] = values->stat_values_u._u;
					break;
				case 'f':
					dassert(values->tag == __f);
					temp[k] = values->stat_values_u._f;
					break;
				default:
					assert(0);
					break;
			}
		}
		{
			w_rc_t			e;
			DBG(<<"add module_special for " << descr);
			e = where.add_module_special(
					descr, d_malloced,
					m->base, m->count, 
					msgs,  s_malloced,
					types, t_malloced, temp);
		// temp will be freed by the w_statistics_t
			if(e) {
				cerr << e << endl;
				VERR(SVAS_InternalError);
				goto failure;
			}
		}
	}

failure:
	REPLY;
}

#include "rusage.h"
#ifdef _stats 
#undef _stats
#endif

void			
svas_client::cstats(
) 
{
	memset(&_stats,'\0',sizeof(_stats));
	msg_stats.clear();
	rmsg_stats.clear();
	compute();
	if(_batch) {
		_batch->cstats();
		_batch->compute();
	}
	flat_rusage.clear();
}

// for use by output operator
void			
svas_client::pstats(
	w_statistics_t &s		// for output
) 
{
	compute();
	s << msg_stats;
	s << rmsg_stats;
	s << _stats;
	if(_batch) {
		_batch->pstats(s);
	}
	flat_rusage.compute();
	s << flat_rusage;
}

VASResult		
svas_client::fileOf(
	IN(lrid_t)		obj,
	OUT(lrid_t)		fid	
) 
{ 
	FSTART(svas_client::fileOf,fileof1,lrid_t);

	NULLPARAMCHECK(fid,2);
	a = obj;
	SVCALL(fileof1,lrid_t);
	*fid = reply->result;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::fileOf(
	IN(IndexId)	iid,
	OUT(lrid_t)		fid	
) 
{ 
	FSTART(svas_client::fileOf,fileof2,lrid_t);

	NULLPARAMCHECK(fid,2);
	a = iid;
	SVCALL(fileof2,lrid_t);
	*fid = reply->result;
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}
VASResult		
svas_client::addIndex(
	IN(IndexId)	iid,
	IndexKind			indexKind	//  btree, lhash, etc
)
{
	FSTART(svas_client::addIndex,addindex1,void);
	a.iid = iid;
	a.kind = indexKind;
	SVCALL(addindex1,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::dropIndex(
	IN(IndexId)	iid // index to drop
)
{
	FSTART(svas_client::addIndex,dropindex1,void);
	a.iid = iid;
	SVCALL(dropindex1,void);
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::statIndex(
	IN(IndexId)			iid,
	OUT(indexstatinfo)	j
)
{
	FSTART(svas_client::statIndex,statindex1,statindex);
	a.iid = iid;
	SVCALL(statindex1,statindex);
	if(status.vasresult != SVAS_OK)  {
		perr(cerr, _fname_debug_, __LINE__, __FILE__,ET_USER);
	} else {
		*j = reply->result;
	}
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::setRoot(
	IN(lvid_t)		vid,
	OUT(lrid_t)		dir	
) 
{ 
	FSTART(svas_client::setRoot,setroot,lrid_t);
	a.volume = vid;
	SVCALL(setroot,lrid_t);
	status = reply->status;

	if(status.vasresult != SVAS_OK)  {
		perr(cerr, _fname_debug_, __LINE__, __FILE__,ET_USER);
	} else {
		*dir = reply->result;
	}
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult		
svas_client::getRootDir(
	OUT(lrid_t)		dir	
) 
{ 
	/* 	FSTART(func-name, argument-type, reply-type) */
	FSTART(svas_client::getRootDir,getroot,lrid_t);

	NULLPARAMCHECK(dir,1);
	
	SVCALL(getroot,lrid_t);

	status = reply->status;

	if(status.vasresult != SVAS_OK)  {
		perr(cerr, _fname_debug_, __LINE__, __FILE__,ET_USER);
	} else {
		*dir = reply->result;
	}

	if(!CLNT_FREERES((CLIENT *)this->cl, _freeproc, reply)){
		assert(0); /* internal error */
	}
	VRETURN status.vasresult;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult 
svas_client::enter(bool tx_active) 
{
	FUNC(vas_enter);
	if(tx_active) {
		return tx_required();
	} else {
		// TODO: handle prepared state
		return tx_not_allowed();
	}
}

VASResult		
svas_client::_interruptTrans(int sock) 
{
	FUNC(svas_client::_interrupt);
	CLEARSTATUS;

	VERR(OS_PermissionDenied);
	VRETURN SVAS_FAILURE;
}

/**************************************************/
VASResult		
svas_client::start_batch(int qlen) 
{
	FUNC(svas_client::start_batch);
	CLEARSTATUS;

	_batch->start(qlen);
	VRETURN SVAS_OK;
}

VASResult		
svas_client::send_batch(
	batched_results_list &list // caller provides this
) 
{
	FUNC(svas_client::send_batch);
	CLEARSTATUS;
	if(! _batch->is_active()) { 
		VERR(SVAS_BatchingNotActive);
		VRETURN (status.vasresult = SVAS_FAILURE);
	}
	list = _batch->send(); 
	//
	// status is set by svas_client::_flush_aux(), called by send()
	VRETURN status.vasresult;
}

/*
 * low-down implementation of batch->send()
 */

VASResult		
svas_client::_flush_aux(int _qlen, void *_q, batched_results_list *_results)
{
	FSTART(batch::_flush_aux,batched_req,batched_req);
	CLEARSTATUS;
	ADJUST_STAT(batched_req,-1);

	if(_qlen>0) {
		dassert(_batch->is_active());
		a.list.list_len = _qlen;

		a.list.list_val = new batch_req [_qlen];
		MALLOC_CHECK(a.list.list_val);
		/* memcpy( to, from, len) */
		memcpy(a.list.list_val, _q, _qlen*sizeof(batch_req));
		SVCALL(batched_req,batched_req);
		delete[] a.list.list_val; a.list.list_val = 0;

		DBG(<<"batched_req returned " 
			<< reply->count << " meaningful responses,"
			<< reply->list.list_len << " entries" );

		_batch->append_results(reply->count, reply->list.list_len,
								reply->list.list_val);

		if(!CLNT_FREERES((CLIENT *)this->cl, _freeproc, reply)){
			assert(0); /* internal error */
		}
	}
	VRETURN SVAS_OK;

failure:
	_batch->append_results(0, _qlen, 0);
	VRETURN SVAS_FAILURE;
}
/**************************************************/

VASResult	
svas_client::appendObj(
	IN(lrid_t) 			obj,	
	IN(vec_t)			newdata,
	ObjectOffset		newtstart	
)
{
	FSTART(svas_client::appendObj,appendobj,void);
	bool warn=false;

	if(newdata.size() == 0){
		VERR(SVAS_BadParam3);
		FAIL;
	}
	_DO_(invalidateobj(obj) );

	{
		int 	max;
		BATCH1(AppendReq,appendobj,void);
		if( _batch->preflush(newdata)) {
			//
			// The Object Cache doesn't append
			// anymore-- it uses update requests.
			// We're not implementing this for very
			// large appends because it would conflict
			// with batching.  We'd have to implement
			// this for large vectors by breaking it
			// into trunc();write();write()...
			// but the writes need and OFFSET!!!! which 
			// we don't have.  To get it would require
			// an RPC with a non-null result, which is
			// hard to do if we're batching.
			// So for the time being, we'll not implement
			// this.
			//
			VERR(SVAS_VecTooBig);
			FAIL;
		}

		_batch->push(a.newdata, newdata);

		a.obj = obj;
		a.ctstart = assign;
		a.tstart = newtstart;


		BATCH2(AppendReq,appendobj,void);

		_batch->pop(a.newdata);

		BATCH3(AppendReq,appendobj,void);
	}

	WARNREPLY(warn, SVAS_NotEnoughShm);
failure:
	VRETURN SVAS_FAILURE;
}

VASResult	
svas_client::appendObj(
	IN(lrid_t) 			obj,
	IN(vec_t)			newdata	
)
{
	FSTART(svas_client::appendObj,appendobj,void);
	bool	warn=false;
	if(newdata.size() == 0){
		VERR(SVAS_BadParam3);
		FAIL;
	}
	_DO_(invalidateobj(obj) );

	{
		BATCH1(AppendReq,appendobj,void);
		if(_batch->preflush(newdata)) {
			//
			// see comments for other appendObj()
			// above for reasons why we're not
			// implementing this for large vectors
			//
			VERR(SVAS_VecTooBig);
			FAIL;
		}
		_batch->push(a.newdata, newdata);

		a.obj = obj;
		a.ctstart = nochange;
		a.tstart = 0;

		BATCH2(AppendReq,appendobj,void);

		_batch->pop(a.newdata);

		BATCH3(AppendReq,appendobj,void);
	}
	WARNREPLY(warn, SVAS_NotEnoughShm);
failure:
	VRETURN SVAS_FAILURE;
}

VASResult	
svas_client::updateObj(
	IN(lrid_t) 			obj,	
	ObjectOffset		woffset,	
	IN(vec_t)			wdata,
	ObjectOffset		aoffset,	
	IN(vec_t)			adata,
	ObjectOffset		newtstart	
)
{
	// append+write
	FSTART(svas_client::updateObj,updateobj1,void);
	bool 	warn=false;

	_DO_(invalidateobj(obj) );

	{
		BATCH1(Update1Req,updateobj1,void);
		{ 	
			int	max = capacity();

			// if there's not enough room to do this
			// in one message...
			if( _batch->preflush(wdata) ||
				_batch->preflush(adata)) {
				VASResult res;

				// break into multiple requests :
				// trunc/append + writes

				ADJUST_STAT(truncobj,-1);
				++_stats.mwrite_rpcs; // for the truncObj()
				++_stats.mwrite_calls; // for the mwrites()

				res = this->truncObj(obj,aoffset + adata.size(), 
					newtstart, false);
				if(res!=SVAS_OK) {
					FAIL;
				}
				res = mwrites(obj, wdata, woffset, max);
				if(res!=SVAS_OK) {
					FAIL;
				}
				res = mwrites(obj, adata, aoffset, max);
				if(res!=SVAS_OK) {
					FAIL;
				}
				VRETURN SVAS_OK;
			}

			// else there is enough room for 1 msg, so 
			// go ahead...
			_batch->push(a.wdata, wdata);
			_batch->push(a.adata, adata);
		}

		a.obj = obj;
		a.objoffset = woffset;
		a.newtstart = newtstart;

		BATCH2(Update1Req,updateobj1,void);

		/* reverse the order */
		_batch->pop(a.adata);
		_batch->pop(a.wdata);

		BATCH3(Update1Req,updateobj1,void);
	}
	WARNREPLY(warn,SVAS_NotEnoughShm);
failure:
	VRETURN SVAS_FAILURE;
}

VASResult	
svas_client::updateObj(
	IN(lrid_t) 			obj,	
	ObjectOffset		offset,	
	IN(vec_t)			wdata,
	ObjectSize			newlen,	
	ObjectOffset		newtstart	
)
{
	// trunc+write
	FSTART(svas_client::updateObj,updateobj2,void);
	bool 	warn=false;

	_DO_(invalidateobj(obj) );

	{
		BATCH1(Update2Req,updateobj2,void);
		{ 	
			int	max = capacity();

			// if there's not enough room to do this
			// in one message...
			if( _batch->preflush(wdata)) {
				VASResult res;

				// break into multiple requests :
				ADJUST_STAT(truncobj,-1);
				_stats.mwrite_rpcs++; // for the truncObj()
				_stats.mwrite_calls++; // for the mwrites()

				res = this->truncObj(obj, newlen, newtstart, false);
				if(res!=SVAS_OK) {
					FAIL;
				}

				res = mwrites(obj, wdata, offset, max);
				if(res!=SVAS_OK) {
					FAIL;
				}
				VRETURN SVAS_OK;
			}

			// else there is enough room for 1 msg, so 
			// go ahead...
			_batch->push(a.wdata, wdata);
		}

		a.obj = obj;
		a.objoffset = offset;
		a.newlen = newlen;
		a.newtstart = newtstart;

		BATCH2(Update2Req,updateobj2,void);

		_batch->pop(a.wdata);

		BATCH3(Update2Req,updateobj2,void);
	}
	WARNREPLY(warn,SVAS_NotEnoughShm);
failure:
	VRETURN SVAS_FAILURE;
}

VASResult	
svas_client::writeObj(
	IN(lrid_t) 			obj,	
	ObjectOffset		offset,	
	IN(vec_t)			newdata
)
{
	FSTART(svas_client::writeObj,writeobj,void);
	bool 	warn=false;

	if(newdata.size() == 0){
		VERR(SVAS_BadParam3);
		FAIL;
	}
	_DO_(invalidateobj(obj) );

	{
		BATCH1(WriteReq,writeobj,void);
		{ 	
			int	max = capacity();

			// if there's not enough room to do this
			// in one message...
			if( _batch->preflush(newdata)) {
				VASResult res;

				// break into multiple requests :
				// trunc/append + writes

				++_stats.mwrite_calls; // for the mwrites()
				res = mwrites(obj, newdata, offset, max);
				if(res!=SVAS_OK) {
					FAIL;
				}
				VRETURN SVAS_OK;
			}

			// else there is enough room for 1 msg, so 
			// go ahead...
			_batch->push(a.newdata, newdata);
		}

		a.obj = obj;
		a.objoffset = offset;

		BATCH2(WriteReq,writeobj,void);
		_batch->pop(a.newdata);

		BATCH3(WriteReq,writeobj,void);
	}
	WARNREPLY(warn,SVAS_NotEnoughShm);
failure:
	VRETURN SVAS_FAILURE;
}

VASResult	
svas_client::truncObj(
	IN(lrid_t) 			obj,	
	ObjectSize			newlen	
)
{
	FSTART(svas_client::truncObj,truncobj,void);

	_DO_(invalidateobj(obj) );

	{
		BATCH1(TruncReq,truncobj,void);

		a.obj = obj;
		a.to_length = newlen;
		a.ctstart = nochange;
		a.tstart = 0;
		a.zeroed = true;

		BATCH2(TruncReq,truncobj,void);

		BATCH3(TruncReq,truncobj,void);
	}

	BREPLY;
failure:
	VRETURN SVAS_FAILURE;
}

VASResult
svas_client::truncObj(
    IN(lrid_t)          obj,   
    ObjectSize          newlen, 
    ObjectOffset        newtstart,
    //                  set size to "end-of-heap"
	bool				zeroed // =true
							// if zeroed == false, it won't zero the 
							// extended part when it expands the object
)
{
	FSTART(svas_client::truncObj,truncobj,void);

	_DO_(invalidateobj(obj) );

	{
		BATCH1(TruncReq,truncobj,void);

		a.obj = obj;
		a.to_length = newlen;
		a.ctstart = assign;
		a.tstart = newtstart;
		a.zeroed = zeroed;

		BATCH2(TruncReq,truncobj,void);

		BATCH3(TruncReq,truncobj,void);
	}
	BREPLY;
failure:
	VRETURN SVAS_FAILURE;
}

#ifdef NOTDEF
static void
vectors(
	IN(vec_t)			vec,
	IN(int)				maxsize,
	IN(int)				offset,
	vec_t				&result // provided by the caller
)
{
	FUNC(vectors);
	int i;

	DBG(<<"offset " << offset << " in vector :");
	for(i=0; i<vec.count(); i++) {
		DBG(<<"vec["<<i<<"]=("
			<<vec.ptr(i) <<"," <<vec.len(i) <<")");
	}
	result.reset();

	// return a vector representing the next
	// maxsize bytes starting at the given offset
	// from the data represented by the input vector
	int		first_chunk=0, first_chunk_offset=0, first_chunk_len=0;
	{
		// find first_chunk
		int skipped, skipping, diff;

		for(i=0; i<vec.count(); i++) {
			skipping = vec.len(i);
			if(skipped + skipping > offset) {
				// found
				first_chunk = i;
				diff = offset - (skipped + skipping);
				first_chunk_offset = skipping - diff;
				if(skipping> maxsize) {
					first_chunk_len = maxsize;
				} else {
					first_chunk_len = skipping;
				}
				result.put(vec.ptr(i)+first_chunk_offset,first_chunk_len);
				break;
			}
			skipped += skipping;
		}
	}

	DBG(<<"FIRST *** at offset = " << first_chunk_offset
		<<" chunk " << first_chunk << " len=" <<vec.len(first_chunk));

	if(first_chunk_len < maxsize) {
		// find next chunks up to the last
		int used, using, leftover;

		used = first_chunk_len;
		for(i=first_chunk+1; i<vec.count(); i++) {
			using = vec.len(i);
			if(used + using <= maxsize) {
				// use the whole thing
				used += using;
				result.put(vec.ptr(i),using);
			} else {
				// gotta use part
				result.put(vec.ptr(i),maxsize-used);
				used = maxsize;
				break;
			}
		}
	}
	DBG(<<"returning  :");
	for(i=0; i<result.count(); i++) {
		DBG(<<"result["<<i<<"]=("
			<<result.ptr(i) <<"," <<result.len(i) <<")");
	}
}
#endif

VASResult	
svas_client::mwrites(
	IN(lrid_t) 			obj,	
	IN(vec_t)			vec,
	ObjectSize			_offset,
	ObjectSize			max
)
{
	FUNC(svas_client::mwrite);
	VASResult	res;

	vec_t		item;

	DBG(<<"mwrites: write max="<<max<<" _offset=" << _offset 
		<<" vec.size=" << vec.size()
		<<" vec.count=" << vec.count());

	while (_offset < vec.size()) {
			vec.mkchunk(max, _offset, item);

			ADJUST_STAT(writeobj,-1);
			_stats.mwrite_rpcs++;

			res = writeObj(obj, _offset, item);
			_offset += max;
			if(res != SVAS_OK) {
				FAIL;
			}
	}
	VRETURN SVAS_OK;
failure:
	VRETURN SVAS_FAILURE;
}

int 	svas_client::capacity() const { return _batch->capacity(); }

void
svas_client::compute()
{
	if(_stats.page_installs>0) {
		_stats.avghits = (float)(_stats.page_hits/_stats.page_installs);
	} else {
		_stats.avghits = 0.0;
	}

	if(_stats.mwrite_calls>0) {
		_stats.avgrpcspercall = (float)(_stats.mwrite_rpcs/_stats.mwrite_calls);
	} else {
		_stats.avgrpcspercall = 0.0;
	}

#ifdef DEBUG
	// total rpcs == sum of those counted
	int tot=0;
	for(int i= client_init; i<=gather_stats; i++) {
		tot += rmsg_stats.count(i);
	}
	dassert(_stats.server_calls == tot);

	// total broken into multiple writes < sum of
	// breakable requests

	// total resulting from mwrites <= sum of 
	// breakable messages sent
#endif
}

VASResult
svas_client::quota(
	IN(lvid_t)  lvid,
	OUT(smksize_t) q,
	OUT(smksize_t) u
)
{	
	FSTART(svas_client::quota, v_quota, v_quota);
	a.volume =  lvid;
	SVCALL(v_quota,v_quota);
	if(reply) {
		if (q) *q = reply->kbquota;
		if (u) *u = reply->kbused;
	}
	REPLY;
failure:
	VRETURN SVAS_FAILURE;
}

bool 	
svas_client::connected() const {
	if(cl 
#ifndef SOLARIS2
	&& 
	(exception_tcp((CLIENT *)cl)==0)
#endif
	) return 1;
	return 0;
}

////////////////////////////////////////////////////////////
// static, so it's really a member of svas_base::
////////////////////////////////////////////////////////////
OC 		*
svas_base::get_oc() {
	svas_client *v =  me();
	return v->_oc_ptr;
}
