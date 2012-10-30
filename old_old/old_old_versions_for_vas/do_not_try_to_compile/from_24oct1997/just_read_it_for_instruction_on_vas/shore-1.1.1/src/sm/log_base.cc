/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: log_base.cc,v 1.14 1997/06/15 03:13:22 solomon Exp $
 */
#define SM_SOURCE
#define LOG_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include <sm_int_0.h>
#include "log_buf.h"

/* Until/unless we put this into shared memory: */
static struct log_base::_shared_log_info  __shared;

/*********************************************************************
 *
 *  Constructor 
 *  log_base::log_base 
 *
 *********************************************************************/

NORET
log_base::log_base(
	char */*shmbase*/
	)
    : 
    _shared (&__shared)
{
    /*
     * writebuf and readbuf are "allocated"
     * in srv_log constructor
     */

#ifdef notdef
    /* 
     * create shm seg for the shared data members
     */
    w_assert3(_shmem_seg.base()==0);
    if(segid) {
	// server size -- attach to it
	int i;
	istrstream(segid) >> i;
	w_rc_t rc = _shmem_seg.attach(i);
	if(rc) {
	    cerr << "log daemon:-  cannot attach to shared memroy " << segid << endl;
	    cerr << rc;
	    W_COERCE(rc);
	}
    } else {
	// client side -- create it

	// TODO add diskport-style queue
	w_rc_t rc = _shmem_seg.create(sizeof(struct _shared_log_info));

	if(rc) {
	    cerr << "fatal error: cannot create shared memory for log" << endl;
	    W_COERCE(rc);
	}
    }
    _shared = new(_shmem_seg.base()) _shared_log_info;
    w_assert3(_shared != 0);
#endif

    // re-initialize
    _shared->_log_corruption_on = false;
    _shared->_min_chkpt_rec_lsn = lsn_t(uint4(1),uint4(0));

    DBG(<< "_shared->_min_chkpt_rec_lsn = " 
	<<  _shared->_min_chkpt_rec_lsn);

}

NORET
log_base::~log_base()
{
/*
    if(_shmem_seg.base()) {
	W_COERCE( _shmem_seg.destroy() );
    }
*/
}

/*********************************************************************
 *
 *  log_base::check_raw_device(devname, raw)
 *
 *  Check if "devname" is a raw device. Return result in "raw".
 *
 *********************************************************************/
rc_t
log_base::check_raw_device(const char* devname, bool& raw)
{
	w_rc_t	e;
	int	fd;

	raw = false;

	W_DO(me()->open(devname, smthread_t::OPEN_LOCAL|smthread_t::OPEN_RDONLY, 0, fd));
	e = me()->fisraw(fd, raw);
	W_IGNORE(me()->close(fd));

	return e;
}

/*********************************************************************
 *
 *  log_i::next(lsn, r)
 *
 *  Read the next record into r and return its lsn in lsn.
 *  Return false if EOF reached. true otherwise.
 *
 *********************************************************************/
bool log_i::next(lsn_t& lsn, logrec_t*& r)  
{
    bool eof = (cursor == null_lsn);
    if (! eof) {
	lsn = cursor;
	rc_t rc = log.fetch(lsn, r, &cursor);
	// release right away, since this is only
	// used in recovery.
	log.release();
	if (rc)  {
	    if (rc.err_num() == smlevel_0::eEOF)  
		eof = true;
	    else  {
		smlevel_0::errlog->clog << error_prio 
		<< "Fatal error : " << RC_PUSH(rc, smlevel_0::eINTERNAL) << flushl;
	    }
	}
    }
    return ! eof;
}
