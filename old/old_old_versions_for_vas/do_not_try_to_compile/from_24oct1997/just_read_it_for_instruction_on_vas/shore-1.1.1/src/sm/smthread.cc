/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: smthread.cc,v 1.47 1997/06/15 03:14:07 solomon Exp $
 */
#define SM_SOURCE
#define SMTHREAD_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include <sm_int_1.h>
//#include <e_error.i>

const eERRMIN = smlevel_0::eERRMIN;
const eERRMAX = smlevel_0::eERRMAX;

static smthread_init_t smthread_init;

int smthread_init_t::count = 0;
/*
 *  smthread_init_t::smthread_init_t()
 */
smthread_init_t::smthread_init_t()
{
}



/*
 *	smthread_init_t::~smthread_init_t()
 */
smthread_init_t::~smthread_init_t()
{
}

/*********************************************************************
 *
 *  Constructor and destructor for smthread_t::tcb_t
 *
 *********************************************************************/

void
smthread_t::no_xct(xct_t *xd)
{
    w_assert3(xd);
    xd->stash(
	    tcb()._lock_hierarchy,
	    tcb()._sdesc_cache,
	    tcb()._xct_log);
}

void
smthread_t::new_xct(xct_t *x)
{
    w_assert1(x);
    x->steal(
	tcb()._lock_hierarchy,
	tcb()._sdesc_cache,
	tcb()._xct_log);
}



/*********************************************************************
 *
 *  smthread_t::smthread_t
 *
 *  Create an smthread_t.
 *
 *********************************************************************/
smthread_t::smthread_t(
    st_proc_t* f,
    void* arg,
    priority_t priority,
    bool block_immediate,
    bool auto_delete,
    const char* name,
    long lockto)
: sthread_t(priority, block_immediate, auto_delete, name),
  _proc(f),
  _arg(arg)
#ifndef OLD_SM_BLOCK
  ,_block("smblock"),
  _awaken("smawaken")
#endif
{
	lock_timeout(lockto);
}


smthread_t::smthread_t(
    priority_t priority,
    bool block_immediate,
    bool auto_delete,
    const char* name,
    long lockto)
: sthread_t(priority, block_immediate, auto_delete, name),
  _proc(0),
  _arg(0)
#ifndef OLD_SM_BLOCK
  ,_block("smblock"),
  _awaken("smawaken")
#endif
{
	lock_timeout(lockto);
}




/*********************************************************************
 *
 *  smthread_t::~smthread_t()
 *
 *  Destroy smthread. Thread is already defunct the object is
 *  destroyed.
 *
 *********************************************************************/
smthread_t::~smthread_t()
{
    // w_assert3(tcb().pin_count == 0);
    w_assert3( tcb()._lock_hierarchy == 0 );
    w_assert3( tcb()._sdesc_cache == 0 );
    w_assert3( tcb()._xct_log == 0 );
}

#ifndef OLD_SM_BLOCK

void smthread_t::prepare_to_block()
{
    _unblocked = false;
}
#ifndef DEBUG
#define blockname /*not used*/
#endif
w_rc_t	smthread_t::block(smutex_t &lock,
			  int4_t timeout,
			  const char * const blockname)
#undef blockname
{
	bool	timed_out = false;

	W_COERCE(lock.acquire());
	_waiting = true;

	/* XXX adjust timeout for "false" signals */
	while (!_unblocked && !timed_out) {
#ifdef DEBUG
		_awaken.rename("c:", blockname);
#endif
		w_rc_t	e = _awaken.wait(lock, timeout);
#ifdef DEBUG
		_awaken.rename("c:", "smawaken");
#endif
		if (e && e.err_num() == stTIMEOUT)
			timed_out = true;
		else if (e)
			W_COERCE(e);
	}

	_waiting = false;
	lock.release();

	/* XXX possible race condition on sm_rc, except that
	   it DOES belong to me, this thread?? */
	/* XXX if so, the thread package _rc code has the same problem */
	return timed_out ? RC(stTIMEOUT) : _sm_rc;
}

w_rc_t	smthread_t::unblock(smutex_t &lock, const w_rc_t &rc)
{
	W_COERCE(lock.acquire());
	if (!_waiting) {
		cerr << "Warning: smthread_t::unblock():"
			<< " async thread unblock!" << endl;
	}

	_unblocked = true;

	/* Save rc (will be returned by block());
	   this code is copied from that in the
	   sthread block/unblock sequence. */
	if (_sm_rc)  {;}
	if (&rc) 
		_sm_rc = rc;
	else
		_sm_rc = RCOK;

	_awaken.signal();
	lock.release();
	return RCOK;
}

/* thread-compatability block() and unblock.  Use the per-smthread _block
   as the synchronization primitive. */
w_rc_t	smthread_t::block(int4_t timeout,
			  sthread_list_t *list,
			  const char * const caller,
			  const void *)
{
	w_assert1(list == 0);
	return block(_block, timeout, caller);
}

w_rc_t	smthread_t::unblock(const w_rc_t &rc)
{
	return unblock(_block, rc);
}
#endif


/*********************************************************************
 *
 *  smthread_t::run()
 *
 *  Befault body of smthread. Could be overriden by subclass.
 *
 *********************************************************************/
#ifdef NOTDEF
// Now this function is pure virtual in an attempt to avoid
// a possible gcc bug

void smthread_t::run()
{
    w_assert1(_proc);
    _proc(_arg);
}
#endif /* NOTDEF */

smthread_t*
smthread_t::dynamic_cast_to_smthread()
{
    return this;
}


const smthread_t*
smthread_t::dynamic_cast_to_const_smthread() const
{
    return this;
}


void
smthread_t::for_each_smthread(SmthreadFunc& f)
{
    SelectSmthreadsFunc g(f);
    for_each_thread(g);
}


void 
smthread_t::attach_xct(xct_t* x)
{
    w_assert3(tcb().xct == 0);  // eTWOTRANS
    tcb().xct = x;
    int n = x->attach_thread();
    w_assert1(n >= 1);
}


void 
smthread_t::detach_xct(xct_t* x)
{
    w_assert3(tcb().xct == x); 
    int n=x->detach_thread();
    w_assert1(n >= 0);
    tcb().xct = 0;
}

void		
smthread_t::_dump(ostream &o)
{
	sthread_t *t = (sthread_t *)this;
	t->sthread_t::_dump(o);

	o << "smthread_t: " << (char *)(is_in_sm()?"in sm ":"");
	if(tcb().xct) {
	  o << "xct " << tcb().xct->tid() << endl;
	}
// no output operator yet
//	if(sdesc_cache()) {
//	  o << *sdesc_cache() ;
//	}
	o << endl;
}


void SelectSmthreadsFunc::operator()(const sthread_t& thread)
{
    if (const smthread_t* smthread = thread.dynamic_cast_to_const_smthread())  {
	f(*smthread);
    }
}


void PrintSmthreadsOfXct::operator()(const smthread_t& smthread)
{
    if (smthread.const_xct() == xct)  {
	o << "--------------------" << endl << smthread;
    }
}
