/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/vas.C,v 1.78 1997/10/13 11:49:49 solomon Exp $
 */
#include <copyright.h>

#ifdef __GNUG__
#pragma implementation "svas_server.h"
#endif

#include <string.h>
#include "mount_m.h"
#include "sysp.h"
#include <vas_internal.h>
#include "vaserr.h"
#if !defined(SOLARIS2)
#include <rpc/svc_stats.h>
#endif

#include "uname.h"

// 
// NB: ALL THESE FUNC return VASResult, status in this->status
//

BEGIN_EXTERNCLIST
	char 	*_svas_replybuf(); // cannot be a member of any svas class.
#ifndef SOLARIS2
	int		gettimeofday( struct timeval *, struct timezone *);
#endif
END_EXTERNCLIST

extern void ccmsg_stats();
extern void cvmsg_stats();
extern void cnmsg_stats();
extern void cmmsg_stats();
extern void efs_pstats(w_statistics_t &s);
extern void efs_cstats();

bool          
svas_server::privileged() 
{
	VFPROLOGUE(svas_server::privileged);
	if ( ((svas_layer_init::RootUid == uid()) || 
		(ShoreVasLayer.ShoreUid== uid())) ) {
		return true;
	}

	VERR(OS_PermissionDenied);
	return false;
}

#define DEFAULT_DIRECTORY_SERVICE ds_degree2 // degree 2

void		
svas_server::restore_default_service() { 
	(void)set_service(DEFAULT_DIRECTORY_SERVICE);
}

svas_server::svas_server(client_t *client, ErrLog *el) : 
	svas_base(el), 
	cl(client), 
	reply_buf(0), // set in body
	// username set in init
	// ngroups, groups set in init
	ngroups(0),
	objects_destroyed(0),
	_umask(0),
	_uid(0),
	euid(0),
	_gid(0),
	egid(0),
	iscan(0),
	fscan(0),
	_page_buf(0),
	_lg_buf(0),
	_xact(0), 
	_dirservice(DEFAULT_DIRECTORY_SERVICE), 
	_context(client_op),
	_dirxct(0), 
#ifdef DEBUG
	objs_pinned(0),
	// last_pinned initialized in body
	tx_rq_count(0), tx_na_count(0), enter_count(0),
#endif
	sysp_cache(0) // set in body

{
	VFPROLOGUE(svas_server::svas_server);

#ifdef DEBUG
	_suppress_p_user_errors = false;
#else
	_suppress_p_user_errors = true;
#endif

#ifdef DEBUG
	last_pinned.lrid = lrid_t::null;
	last_pinned.store = 0;
	last_pinned.page = 0;
	last_pinned.vol =  0;
#endif

	{
		int i;
		if(ShoreVasLayer.opt_sysp_cache_size != 0) {
			i = strtol(ShoreVasLayer.opt_sysp_cache_size->value(),0,0);
		} else { 
			i = 1;
		}
		sysp_cache = new SyspCache(i);
	}

	// clear status
	memset(&status, '\0', sizeof(status)); // clear status 

	_cstats(false);

	if((reply_buf = _svas_replybuf())==NULL) {
		VERR(SVAS_MallocFailure);
		return;
	}

	_cwd = ReservedOid::_RootDir; 

	initflags(); // clear all -- flags are set in startSession
	
	status.txstate = txstate(0); // no tx yet

	dassert(status.vasresult == SVAS_OK);
	dassert(status.vasreason == SVAS_OK);
}

#include "client.h"

svas_server::~svas_server() 
{
	VFPROLOGUE(~vas)
	w_rc_t	e;

	if(_cwd.serial != ReservedSerial::_RootDir) {
		// change use counts
		// TODO: if we do away with transient mounts,
		// we can do away with use counts also
		//
		mount_table->cd(_cwd.lvid, mount_m::From);
	} else {
		// should happen *only* if there's no '/' mounted
	}
	if(_xact) {
		// should be attached
		assert(me()->xct()==_xact);
		//	me()->attach_xct(_xact);

		abortTrans();
		assert(me()->xct() == 0);
		assert(this->_xact == 0);
		// abortTrans blows away xact

		// now detach 
		// me()->detach_xct(_xact);
	}

	if(over_the_wire()) {
		dassert(shm.base()==NULL);
		// _page_buf can be null if client
		// dies before doing _init
		if(_page_buf != NULL) {
			delete[] _page_buf;
			_page_buf = NULL;
		}
		_num_page_bufs = 0;

		if(num_lg_bufs() > 0) {
			dassert(_lg_buf != NULL);
			delete[] _lg_buf;
			_lg_buf = NULL;
		}
		dassert(_lg_buf == NULL);

	} else if(pseudo_client()) {
		dassert(shm.base()==NULL);
		dassert(_page_buf == NULL);
	} else {
		// remove assert...
		// shm might never have been allocated
		// dassert(shm.base()!=NULL);
#ifdef 	DEBUG
		if(_page_buf != NULL) {
			dassert(_page_buf >= shm.base());
			dassert(_lg_buf == 0 || _lg_buf > _page_buf);
		} else {
			if(_lg_buf != NULL) {
				dassert(_lg_buf >= shm.base() );
			}
		}
#endif
		// ok to destroy if never alloced
		if( e = shm.destroy()) {
			ShoreVasLayer.logmessage("Error in shmem.destroy()", e);
		}
		_page_buf = _lg_buf = NULL;
	}
	if(reply_buf != NULL) {
		delete[] reply_buf;
		reply_buf = NULL;
	}
	if(cl != NULL) {
		client_t::disconnect_server(cl); // *NOT* shutdown!
	}
	delete sysp_cache;

	if(_dirxct) {
		delete _dirxct;
		_dirxct = 0;
	}
	DBG(
		<< "~vas for uid " << uid() << " gid " << gid() 
	)
}

VASResult 
svas_server::_init(
	mode_t	mask,		// IN
	int	nsmbytes,
	int	nlgbytes
) 
{
	VFPROLOGUE(_init); 

	DBG(
		<< "svas_server::_init gets umask" <<  mask
	)
	this->_umask = mask;

	_page_size = ss_m::page_sz;

	bool redo=true;
// redo:
	while(redo) {
		redo = false;
		if(over_the_wire()) {
			DBG(<<"over-the-wire-- no shm");
			_lg_buf = _page_buf = NULL;

			_num_page_bufs = (greater(nsmbytes,nlgbytes))*1024/page_size();
			_num_page_bufs = greater(num_page_bufs(),1);

			// we'll return the max size of a page that we'll write
			// over TCP
			// the null shmid will tell the client that there's no shm

			_num_lg_bufs = num_page_bufs();
			(void) replace_page_buf(); // allocate a new one
			(void) replace_lg_buf(); // allocate a new one
		} else if(pseudo_client()) {
			_num_page_bufs = 0;
			_num_lg_bufs = 0;
			_page_buf = NULL;
			_lg_buf = NULL;
		} else {
			// allow both # bufs to be zero 
			// or only the lgbufs to be zero 
			// or neither to be zero
			// approximate the amount the user gave us

			dassert(_flags & vf_shm);
			_num_page_bufs = (nsmbytes*1024) / page_size();
			_num_lg_bufs = (nlgbytes*1024) / page_size();

			//
			// see if we need to
			// override shared memory requested by client
			//
			if(ShoreVasLayer.no_shm) {
				_num_page_bufs = 0;
				_num_lg_bufs = 0;
			}

			if(num_page_bufs() > 0 || num_lg_bufs() > 0) {
		
			DBG(<< "sm bufs " << num_page_bufs()
				<< " lg bufs " << num_lg_bufs()
				<< " open max " << ShoreVasLayer.OpenMax
				);

				if( num_page_bufs() <= 0 ) {
					VERR(SVAS_BadParam2);
					return SVAS_FAILURE;
				}

#ifdef HPUX8
			// have to limit # pages for small objects to
			// 32 bits's worth 
				if(num_page_bufs() > (sizeof(int)*8)) {
					WARN(SVAS_BadParam2);
					_num_page_bufs = sizeof(int)*8;
				}
#endif
				if( num_page_bufs() > ShoreVasLayer.OpenMax ) {
					WARN(SVAS_BadParam2);
					_num_page_bufs = ShoreVasLayer.OpenMax;
					// VERR(SVAS_BadParam2;
					// return SVAS_FAILURE;
				}
				if( num_lg_bufs() <= 0 ) {
					VERR(SVAS_BadParam3);
					return SVAS_FAILURE;
				}
				if( num_lg_bufs() > ShoreVasLayer.OpenMax ) {
					WARN(SVAS_BadParam3);
					_num_lg_bufs = ShoreVasLayer.OpenMax;
					// VERR(SVAS_BadParam3);
					// return SVAS_FAILURE;
				}

				DBG(<<"allocating shm");
				// get shared memory segment here

				if(shm.create( 
				(num_page_bufs()+num_lg_bufs()) * page_size(),  //size
				// page_size(), 			// alignment - was in common/shmem.c
				0640				// mode
				) != RCOK) {
					VERR(SVAS_ShmError);
					return SVAS_FAILURE;
				}

				DBG(<<"Setting shared mem segment to uid " 
					<< this->uid()
					<<" gid " 
					<< this->gid()
					<< "mode"
					<< 0644
					);
				if(shm.set(this->uid(), this->gid(), 0644)!=RCOK) {
					VERR(SVAS_ShmError);
					return SVAS_FAILURE;
				}

				_page_buf = shm.base();
				_lg_buf = _page_buf + (num_page_bufs()*page_size());

				DBG(<<"shm id " 
					<< dec<<shm.id() 
					<< " at base " 
					<< ::hex((unsigned int)shm.base()) );
			} else {
				DBG(<<"converting to over-the-wire");
				clrflags(vf_shm);
				setflags(vf_wire);
				redo = true;
			}
		}
	}
	checkflags(false); // not yet xfer time
	return  SVAS_OK;
}

#include <sthread_stats.h>
#include <shmc_stats.h>

void 
svas_server::dump(bool verbose, ostream &out ) const
{
	char pathbuf[200];

	if(over_the_wire()) {
		out << " REMOTE-TCP";
	} else if(pseudo_client()){
		out << " PSEUDO    ";
	} else {
		out << " LOCAL-SHM  "
			<< " (smpg " << (num_page_bufs() * page_size()) << "B)"
			<< " (lgpg " << (num_lg_bufs() * page_size()) << "B)";
	}
	out << endl;

	// user,groupt info
	out << "\t" << username << "(";  out << uid();
	if(uid() != euid) {
		out << ",eff=" << euid ;
	}
	out << ") grp=" << gid();
	if(gid() != egid) {
		out << ",eff=" << egid ;
	}
	out << " groups: " ;
		for(int n=0; n<ngroups; n++) {
			out << ::form(" %d",groups[n]);
		}
	out << endl;

	out << "\tumask=" <<  _umask
		<< "/0x" << ::hex(((unsigned int)_umask)) ;

	out << " cwd=" << _cwd << endl;
	// can't gwd it because that requires a transaction

	if(_xact) {
		char *description;
		out << "\ttid= " << transid <<
			::form(" (0x%x)", ((unsigned int)_xact)) << " " ;
		description = 
			 (ShoreVasLayer.Sm->state_xct(_xact) == ss_m::xct_active)?"active":
			 (ShoreVasLayer.Sm->state_xct(_xact) == ss_m::xct_stale)?"stale":
			 (ShoreVasLayer.Sm->state_xct(_xact) == ss_m::xct_prepared)?"prepared":
			 (ShoreVasLayer.Sm->state_xct(_xact) == ss_m::xct_aborting)?"aborting":
			 (ShoreVasLayer.Sm->state_xct(_xact) == ss_m::xct_chaining)?"chaining":
			 (ShoreVasLayer.Sm->state_xct(_xact) == ss_m::xct_committing)?"committing":
			 (ShoreVasLayer.Sm->state_xct(_xact) == ss_m::xct_ended)?"ended":
			"BAD STATE";

		out << description; 
	} else {
		out << "\tNo transaction running.";
	}
	if(objects_destroyed) {
		out << "; " << objects_destroyed 
		<< " objects partly destroyed " ;
	}
	out << endl;

	if(iscan || fscan) {
		out << "\tScans in progress: ";
		if(iscan) {
			out << "index "; 
		}
		if(fscan) {
			out << "file/pool/module ";
		}
		out << endl;
	}
	
	out << "\tStatus :";
	perr(out,0);	 // this is dump - so print to out stream
}

VASResult 
svas_server::startSession(
	const char *const username, 
	const uid_t idu,
	const gid_t idg,
	int		remoteness
)
{
	VFPROLOGUE(startSession); 

	DBG(<<"user name= " << username
		<< " uid= " << idu
		<< " gid= " << idg
		<< " remoteness= " << remoteness);

	assert(username);
	assert(strlen(username)<sizeof(this->username));
	(void) strcpy(this->username, username);

	this->euid = this->_uid = idu;
	this->egid = this->_gid = idg;

	ngroups = get_client_groups(username, groups);

	switch(remoteness) {
		case ::same_process:  // pseudo-client
			DBG(<<"pseudo-client!");
			setflags(svas_server::vf_pseudo|vf_no_xfer);
			break;
		case ::same_machine:  // same machine, diff process
			DBG(<<"LOCALHOST-client!");
			setflags(svas_server::vf_shm|vf_no_xfer);
			break;
		case ::other_machine:  // diff machine
			DBG(<<"REMOTE-client!");
			setflags(svas_server::vf_wire|vf_no_xfer);
			break;
		case ::error_remoteness:  
			// possibly rpcinfo's ping service
			return SVAS_FAILURE;
			break;

		default:
			assert(0);
	}
	checkflags(false); // not yet xfer time
	return SVAS_OK;
}

// true if this vas's group includes the given group id (argument)
bool  
svas_server::isInGroup(gid_t g) const
{
	FPROLOGUE(isInGroup, this); 
	int n = ngroups;

	for(n=0; n<ngroups; n++) {
		if(groups[n] == g) return true;
	}
	return false;
}
VASResult 
svas_server::use_page(int i) 
{
	VFPROLOGUE(use_page); 
	if(i >= 0 &&  i < num_page_bufs() ) {
		if(over_the_wire()) {
			dassert(_page_buf != NULL);
			dassert(num_page_bufs() >= 1);
		} else if(pseudo_client()){
			assert(0); // should not get here
		} else {
			dassert(_flags & vf_shm);
			_page_buf = shm.base() + (i*page_size());

			dassert( _lg_buf==0 || _lg_buf == shm.base() + 
				(num_page_bufs() * page_size()));
		}
		RETURN SVAS_OK;
	} else if (num_page_bufs()==0) {
		dassert(i==0);
		RETURN SVAS_OK;
	} else {
		VERR(SVAS_ShmError);
		RETURN SVAS_FAILURE;
	}
}

struct timeval & 
svas_server::Now()
{
	static struct timeval 	_Now;
	struct timezone 		_Zone;
#ifdef SOLARIS2
	if(gettimeofday( &_Now, (struct timezone *)&_Zone)<0) {
#else
	if(gettimeofday( &_Now, &_Zone)<0) {
#endif
		catastrophic("gettimeofday");
	}
	return  _Now;
}


VASResult		
svas_server::setUmask(
	unsigned int	umask		// in
)
{
	VFPROLOGUE(svas_server::setUmask); 
	this->_umask = umask;
	RETURN SVAS_OK;
}
VASResult		
svas_server::getUmask(
	OUT(unsigned int) result
) 
{
	VFPROLOGUE(svas_server::getUmask); 
	*result = this->_umask;
	RETURN SVAS_OK;
}

ostream &
operator<<(ostream &o, const svas_server& v)
{
	v.dump(false, o);
	return o;
}

mode_t	
svas_server::creationMask(mode_t m, gid_t filegrp) const 
{
	DBG(<<"creationMask: " << m);

#ifdef COMMENTNOTDEF
for regular files:
(from man 2v open:)
o  All bits set in the file mode creation mask of the process 
   are cleared.  See umask(2V).
o  The "save text image after execution" bit of the mode is 
   cleared.  See chmod(2V).
o  The "set group ID on execution" bit of the mode is cleared if 
   the effective user ID of the process is not super-user and the
   process is not a member of the group of the created file.

for directories
(from man 2 mkdir)
     mkdir() creates a new directory file with name path.  The
     mode mask of the new directory is initialized from mode.

     The low-order 9 bits of mode (the file access permissions)
     are modified such that all bits set in the process file
     mode creation mask are cleared (see umask(2V)).

     The set-GID bit of mode is ignored.  The set-GID bit of the
     new file is inherited from that of the parent directory.
#endif
	
	if( ((m & S_IFMT)==S_IFDIR) || ((m & S_IFMT) ==S_IFPOOL)) {
		// SetGid bit is taken care of in directory.C, _mkDir() for S_IFDIR.
		// For pools, we ignore it, since *all* objects inherit
		// this mode.  Executable objects probably should't be
		// anonymous (not that we even support executable objects...)

		m &=  ~_umask; 
		DBG(<<"creationMask: " << m);
		return m;
	}
	if( ((m & S_IFMT)==S_IFLNK) || ((m & S_IFMT) ==S_IFXREF)) {
		m = m&S_IFMT | 0777; // oh well
		DBG(<<"creationMask: " << m);
		return m;
	}

	// regular files and the rest:
	if( ((m & S_IFMT)==S_IFREG) || ((m & S_IFMT) ==S_IFNTXT)) {
		// clear bits in umask and sticky bit
		m &=  ~(_umask|Permissions::Sticky); 

		// clear set gid on exec if apropos
		if((euid != ShoreVasLayer.RootUid) && (!isInGroup(filegrp) )) 
			m &= ~Permissions::SetGid;

		DBG(<<"creationMask: " << m);
		return m;
	}
	assert(0);
	// keep compiler quiet:
	return 0;
}

bool				svas_server::is_nfsd() const { return false; }

VASResult		
svas_server::start_batch(int qlen) 
{ 
	FUNC(svas_server::start_batch);
	status.vasresult = SVAS_FAILURE;
	status.vasreason = SVAS_NotImplemented; /* start-batch on server */
	RETURN SVAS_FAILURE;
}
VASResult       
svas_server::send_batch(batched_results_list &res) 
{
	FUNC(svas_server::send_batch);
	res.attempts = 0;
	res.results = 0;
	res.list	=0;
	status.vasresult = SVAS_FAILURE;
	status.vasreason = SVAS_NotImplemented; /* send-batch on server */
	RETURN SVAS_FAILURE;
}

int		
svas_server::sockbufsize()
{ 
	return get_sock_opt(cl->_fd,SO_RCVBUF); 
}

VASResult		
svas_server::nextPoolScan(
	INOUT(Cookie)		cookie,	
	OUT(bool)			eof,	// true if no result
						// if false, result is legit
	OUT(lrid_t)			result
)
{
	return _nextPoolScan(cookie,eof,result,false);
}

VASResult		
svas_server::nextPoolScan(
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
	return _nextPoolScan( cookie,
		eof, result, offset, requested,buf,used,more,lock,
			sysprops,sysp_size,false);
}

#include <msg_stats.h>
extern _msg_stats mmsg_stats;
extern _msg_stats nmsg_stats;
extern _msg_stats cmsg_stats;
extern _msg_stats vmsg_stats;

#include "rusage.h"


void
svas_server::compute() 
{
	// compute stats before << to w_statistics_t
	sysp_cache->compute();
}

void			
svas_server::cstats(
) 
{
	_cstats(true);
}

static sm_stats_info_t smstats;  // gak

void			
svas_server::_cstats(
	bool	 all
) 
{

	if(all) {
		SthreadStats.clear();
		ShmcStats.clear();

		// true here clears the stats

		if(ShoreVasLayer.Sm->gather_stats(smstats, true)) {
			DBG(<<"Failure in gather_stats.");
			return;
		}
		ccmsg_stats();
		cvmsg_stats();
		cmmsg_stats();
		cnmsg_stats();
#ifndef SOLARIS2
		svc_clearstats();
#endif
		efs_cstats();
		flat_rusage.clear();
	}
	sysp_cache->cstats();
}
// for use by 
// w_statistics_t &svas_base::operator<<(const w_statistics_t &)
void			
svas_server::pstats(
	w_statistics_t &s		// for output
)
{
	compute();

	s << SthreadStats;
	s << ShmcStats;

	// TODO clean up lower layers stats
	// don't have to do this function call
	//
	if(ShoreVasLayer.Sm->
		gather_stats(smstats, false) != RCOK) {
		cerr << "Could not gather SM stats." << endl; w_assert1(0);
	} else {
		s << smstats;
	}

	s << *sysp_cache;

	/* no computation needed for these: */
	s << cmsg_stats;
	s << vmsg_stats;
	s << nmsg_stats;
	s << mmsg_stats;

#ifndef SOLARIS2
	svc_pstats(s);
#endif
	efs_pstats(s);

	flat_rusage.compute();
	s << flat_rusage;

}

VASResult		
svas_server::devices(
	INOUT(char) buf,  // user-provided buffer
	int 	bufsize,	  // length of buffer in bytes
	INOUT(char *) list, // user-provided char *[]
	INOUT(devid_t) devs,  // user-provided devid_t[]
	INOUT(int) count,		// length of list on input & output
	OUT(bool) more
)
{
	FPROLOGUE(svas_server::devices, this);

	const char **buf_val=0; int buf_len=0;
	devid_t		*buf_devs=0;

FSTART
	if(buf==0) {
		VERR(SVAS_BadParam1);
		goto failure;
	}
	if(list==0) {
		VERR(SVAS_BadParam3);
		goto failure;
	}
	if(count==0) {
		VERR(SVAS_BadParam4);
		goto failure;
	}
	if(more==0) {
		VERR(SVAS_BadParam5);
		goto failure;
	}
	_DO_(_devices(&buf_val, &buf_devs, &buf_len));
	{
		int i,l;
		char *b;
		const char *c;
		b = buf;
		for(i=0; i<buf_len; i++) {
			c = buf_val[i];
			l = strlen(c);
			// if the string fits in the buffer...
			if((int)(b - buf) + l +1 < bufsize) {
				// go ahead and copy it to the buffer
				list[i] = b;
				strcpy(b, c);
				b+=l;
				*b = '\0'; // safety
				b++;

				// copy the device id also
				devs[i] = buf_devs[i];
			} else {
				// stop here
				break;
			}
		}
		*count = i;
		*more = (i<buf_len)?true:false;
	}
FOK:
	res =  SVAS_OK;
FFAILURE:
	if(buf_val) delete[] buf_val;
	if(buf_devs) delete[] buf_devs;
	RETURN res;
}

VASResult		
svas_server::_devices(
	OUT(Path *) list, // ss_m-provided char *[]
	OUT(devid_t *) devl, // ss_m-provided devid_t *[]
	OUT(int) count	// length of list 
)
{
	FPROLOGUE(svas_server::_devices, this); 

FSTART
	Path *x;
	devid_t *l;
	u_int cnt=0; // grot should be int

	if SMCALL(list_devices(x, l, cnt)) {
		VERR(SVAS_SmFailure);
		FAIL;
	}
	// *devl = l;
	*list = x;
	*devl = l;
	DBG(<<"returned " << cnt);
	*count = (int) cnt;
	// CALLER MUST delete[] list and devl
FOK:
	RETURN SVAS_OK;

FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::lock_timeout(
	IN(locktimeout_t)	newtimeout,	
	OUT(locktimeout_t)	oldtimeout// null ptr if you're not interested
								   // in getting the old value
)
{
	VFPROLOGUE(svas_server::lock_timeout);
	long	n;

	if(newtimeout < 0) {
		n = WAIT_FOREVER;
	} else if (newtimeout == 0) {
		n = WAIT_IMMEDIATE;
	} else n = (int) newtimeout;
	if(oldtimeout) {
		*oldtimeout = (int) me()->lock_timeout();
	}
	me()->lock_timeout((long) n);
	RETURN SVAS_OK;
}

#include "sdl_fct.h"

VASResult		
svas_server::sdl_test(
	IN(int)	ac,	
	const Path av[10],
	OUT(int)	rc // return code
)
{
	VFPROLOGUE(svas_server::sdl_test);
	long	n;
	int sdl_rc;
	cerr << "sdl app test." << endl;
	sdl_main_fctpt sdl_app= 0;
	if (ac <=0)
	{
		cerr << "no args: nothing to call" << endl;
	}
	else 
	{
		sdl_app = sdl_fct::lookup((char *)av[0]);
		if (sdl_app==0)
			cerr << " no sdl function " << av[0] << " found " <<endl;
		else
		{
			uint4 old_flags = _flags;
			setflags(vf_pseudo|vf_no_xfer);
			sdl_rc = (sdl_app)(ac,(char **)av );
			cerr << av[0] << " return value " << sdl_rc <<endl;
			initflags();
			setflags(old_flags);
		}
	}
	if (rc)
		*rc = sdl_rc;
	return SVAS_OK;
}

////////////////////////////////////////////////////////////
// static, so it's really a member of svas_base::
////////////////////////////////////////////////////////////
OC 		*
svas_base::get_oc() {
	svas_server *v = (svas_server *)me()->user_p(); 
	return v->_oc_ptr;
}
