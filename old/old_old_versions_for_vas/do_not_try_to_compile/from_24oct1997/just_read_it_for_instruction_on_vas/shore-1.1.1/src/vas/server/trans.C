/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/trans.C,v 1.56 1997/06/13 21:43:34 solomon Exp $
 */
#include <copyright.h>

#include "sysp.h"
#include "vas_internal.h"
#include "vaserr.h"
#include "smcalls.h"



VASResult 
svas_server::enter(bool tx_active) 
{
	FUNC(enter);

	// don't let any Tcl shells starve
	// other clients
	if(pseudo_client()) me()->yield(); 

	assert_context(client_op);

#	ifdef DEBUG
	failure_line = 0;
#	endif /* DEBUG */

#ifdef DEBUG
	if(++enter_count>1) {
		dassert(enter_count == 1); // fail here
	}
	tx_rq_count = 0;
	tx_na_count = 0;
#endif

	if(tx_active) {
		return tx_required();
	} else {
		// TODO: handle prepared state
		return tx_not_allowed();
	}
}

VASResult 
svas_server::tx_required(bool	clearstatus) 
{
	VFPROLOGUE(svas_server::tx_required);

#ifdef DEBUG
	dassert(tx_rq_count<=0);
	tx_rq_count++;
#endif
	if(status.txstate == Interrupted) {
		// Some other thread interrupted us: abort the transaction
		// Change state back to active so this abort works
		// (rather than going into infinite loop)
		status.txstate = Active;
		clr_error_info();
		_DO_(_abortTrans(this->transid, SVAS_UserAbort));
		assert(me()->xct() == 0);
		assert(this->_xact == 0);

		_ERR(SVAS_TxInterrupted,this,SVAS_ABORTED,svas_server::ET_USER);
		RETURN SVAS_ABORTED;
	}

	// update the status
	status.txstate = txstate(_xact);

	if(status.txstate == Aborting) {
		_ERR(SVAS_ABORTED,this,SVAS_ABORTED,svas_server::ET_USER);
		RETURN SVAS_ABORTED;
	}
	if(xct() == (xct_t *)0) {
		_ERR(SVAS_TxRequired,this,SVAS_FAILURE,svas_server::ET_USER);
		RETURN SVAS_FAILURE;
	} 

	// clear the status
	// in case a previous error left something
	// lying around, but it wasn't enough to
	// abort the tx -- eg. object not found
	// and user is trying something else.

	audit_context(true); // any directory txs must have been committed

	if(clearstatus) {
		clr_error_info();
	}

	RETURN SVAS_OK;

FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult 
svas_server::tx_not_allowed() {
	VFPROLOGUE(svas_server::tx_not_allowed);
#ifdef DEBUG
	dassert(tx_na_count<=0);
	tx_na_count++;
#endif
	DBG(<<"in thread " << me()->id
		<<" user=" << ::hex((unsigned int)(me()->user_p()))
	);
	if(xct() != (xct_t *)0) {
		VERR(SVAS_TxNotAllowed);
		RETURN SVAS_FAILURE;
	} else {
		// in case previous request had an error
		clr_error_info();
		RETURN SVAS_OK;
	}
	audit_no_tx_context(); // any directory txs must have been committed
}

VASResult
svas_server::commitTrans(bool chain) // = false
{
	LOGVFPROLOGUE(svas_server::commitTrans);

	errlog->clog << info_prio << "COMMIT " << transid  << flushl;
	TX_REQUIRED;
FSTART
	_DO_(_commitTrans(this->transid, chain));
FOK:
	res = SVAS_OK;
FFAILURE:
	LEAVE;
	DBG(<<"returning " << res);
	RETURN res;
}

TxStatus	
svas_server::txstate(xct_t *x) 
{
	VFPROLOGUE(svas_server::txstate); // don't wipe out status
	
	TxStatus txstat = NoTx;

	if(x) switch(ss_m::state_xct(x)) {
		case ss_m::xct_stale:
			txstat = Stale;
			break;
		case ss_m::xct_active:
			txstat = Active;
			break;
		case ss_m::xct_prepared:
			txstat = Prepared;
			break;
		case ss_m::xct_aborting:
			txstat = Aborting;
			break;
		case ss_m::xct_chaining:
			txstat =  Chaining;
			break;
		case ss_m::xct_committing:
			txstat = Committing;
			break;
		case ss_m::xct_ended:
			txstat = Ended;
			break;
		default:
			assert(0);
			break;
	} else {
		txstat = NoTx;
	}
	DBG(<< "txstate =" << txstat);
	RETURN txstat;
}

VASResult
svas_server::beginTrans(
	int degree,       
	OUT(tid_t)    tid         // OUT default=NULL
)
{
	VFPROLOGUE(svas_server::beginTrans);
	TX_NOT_ALLOWED; // beginTrans
FSTART
	clr_error_info();

	_DO_(_beginTrans(degree, tid));

	errlog->clog << info_prio << "BEGUN " << transid << flushl;

FOK:
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::_beginTrans(
	int 		  degree,     
	OUT(tid_t)    tid         // OUT default=NULL
)
{
	VFPROLOGUE(svas_server::_beginTrans);

	dassert(ShoreVasLayer.Sm != NULL);

FSTART
	switch(degree) {
	case 0:
	case 1:
		VERR(SVAS_NotImplemented); 
		FAIL;
		break;
	case 2:
		(void)set_service(ds_degree2);
		break;
	case 3:
		(void)set_service(ds_degree3);
		break;
	default:
		VERR(SVAS_BadParam1); 
		FAIL;
		break;
	}

	dassert(_xact == 0);
	dassert(me()->xct() == 0);

	// begin_xct() attaches it to this thread
	DBG(<<"degree=" << degree);
	if SMCALL(begin_xct()) {
		VERR(SVAS_SmFailure);
		FAIL;
	}
	_xact = xct(); // xct() returns that attached to this thread

	assert(_xact!=0);
	transid = ShoreVasLayer.Sm->xct_to_tid(_xact);
	if(tid) *tid = transid;
	status.txstate = txstate(_xact);
FOK:
	DBG(<<"transid is " << transid
		<< "_xact structure at 0x" << ::hex((unsigned int)_xact)
		<<" in thread " << me()->id
		<<" user=" << ::hex((unsigned int)(me()->user_p()))
	);

	RETURN SVAS_OK;

FFAILURE:
	status.txstate = NoTx;
	RETURN SVAS_FAILURE;
} 



VASResult
svas_server::_trans(
	transGoal		goal,
	IN(tid_t)     tid,
	int				reason
)
{
	VFPROLOGUE(svas_server::_trans); 

	FSTART
	tid_t	t = tid;
	xct_t 	*x;
	bool	lazy = false;

	if((t != transid) && (goal != g_resume))   {
		VERR(SVAS_BadParam1); // until we have multiple txs
		FAIL;
	}

	/* SMCALL */
	if ((x=ShoreVasLayer.Sm->tid_to_xct(t)) == NULL ) {
		DBG( << "tid=" << tid << ", t=" << t << ", transid=" << transid);
		VERR(SVAS_NotFound); 
		FAIL;
	}
	if( (xct() != x) && (goal != g_resume)) {
		// not attached
		VERR(SVAS_TxRequired);
		FAIL;
	}
	if((objects_destroyed != 0) && 
		(goal == g_prepare || goal == g_commit || 
			goal == g_commitchain || goal == g_lazycommit)) {
		VERR(SVAS_IntegrityBreach);
		FAIL;
		// can only abort
	}

	switch(goal) {

		case g_suspend:
			sysp_cache->invalidate();
			DBG(<<"attach ? tx" << x);
			me()->detach_xct(x);
			this->_xact = 0;
			this->transid = tid_t::null;
			break;

		case g_resume:
			DBG(<<"attach ? tx" << x);
			me()->attach_xct(x);
			this->_xact = x;
			break;

		case g_prepare:
			{ /*prepare*/ VERR(SVAS_NotImplemented); RETURN SVAS_FAILURE; }
			sysp_cache->invalidate();
#ifdef notdef
			if SMCALL( prepare_xct() ) {
				VERR(SVAS_SmFailure);
				FAIL;
			}
#endif 
			break;

		case g_lazycommit:
			lazy = true; // fall through
		case g_commit:
			// flush the cache in case
			// another transaction updates
			// before we use it again.
			sysp_cache->invalidate();
			if SMCALL( commit_xct(lazy) ) {
				VERR(SVAS_SmFailure);
				FAIL;
			}
			// no more transaction for this vas or thread
			this->_xact = 0;
			transid = (tid_t)tid_t::null;
			assert(xct() == 0);
			break;

		case g_commitchain:
			if SMCALL( chain_xct(lazy) ) {
				VERR(SVAS_SmFailure);
				FAIL;
			}
			assert(_xact!=0);
			// change transaction ids
			transid = ShoreVasLayer.Sm->xct_to_tid(_xact);
			break;

		case g_abort:
			sysp_cache->invalidate();
			if SMCALL( abort_xct() ) {
				VERR(SVAS_SmFailure);
				FAIL;
			}
			// abort_xct detaches the tx
			// so we can't do this assert:
			// assert(ss_m::state_xct(_xact)== ss_m::ended);

			// no more transaction for this vas or thread
			this->_xact = 0;
			this->objects_destroyed = 0;
			assert(me()->xct() == 0);
			assert(this->_xact == 0);

			transid = (tid_t)tid_t::null;
			status.vasreason = reason;
			status.vasresult = SVAS_ABORTED;

		// report it
		// 	perr(_fname_debug_, __LINE__, __FILE__, 
		//		reason == SVAS_UserAbort ? ET_USER : ET_VAS
		//		);
			break;
		default:
			assert(0);
	}
	status.txstate = txstate(_xact);

FOK:
	RETURN SVAS_OK;
FFAILURE:
	status.txstate = txstate(_xact);
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::abortTrans(
	IN(tid_t)     	tid2abort, 	// TODO: use
	int				reason 
)
// called by user 
{
	LOGVFPROLOGUE(svas_server::abortTrans); 

	errlog->clog << info_prio 
		<< "ABORT(auto or user) " << transid << flushl;

	RETURN _abortTrans(tid2abort, reason);
}

VASResult
svas_server::_abortTrans(
	IN(tid_t)     	tid2abort, 	// TODO: use
	int				reason 
)
{
	LOGVFPROLOGUE(svas_server::_abortTrans); 

	errlog->clog << info_prio 
		<< "ABORT(auto or user) " << transid << flushl;

	if(status.smresult != 0) {
		smerrorrc = RC(status.smreason);
	}

#ifdef DEBUG
	dassert(tx_rq_count<=1);
	tx_rq_count--;
#endif
	// tx_required() updates status.txstate
	switch(tx_required(false)) {
		case SVAS_ABORTED:
			// just clear the status
			dassert(status.txstate==Aborting);
			clr_error_info();
			status.txstate = NoTx;
			RETURN SVAS_OK;

		case SVAS_FAILURE:
			RETURN SVAS_FAILURE;

		case SVAS_OK:
			// continue with the abort
			break;
	}

	// skip it if we've already done this...
	if(status.txstate == Aborting) {
		dassert (xct() == (xct_t *)0);
		RETURN SVAS_OK;
	}
	dassert (xct() != (xct_t *)0);
	dassert (_xact != (xct_t *)0);

	errlog->clog << info_prio << "ABORT(tx) " << transid << flushl;

	ShoreStatus 		saved = this->status;

	// _trans does the perr()
	res =  _trans(g_abort, this->transid, reason);
	if(res != SVAS_OK) {
		res = SVAS_FAILURE;
	}
	assert(me()->xct() == 0);
	assert(this->_xact == 0);

	if(res == SVAS_OK) {
		dassert (xct() == (xct_t *)0);
	} 
#ifdef DEBUG
	else { dassert(status.txstate==Aborting); }
#endif

	set_error_info(reason,SVAS_ABORTED,svas_base::ET_USER, smerrorrc,
					_fname_debug_, __LINE__, __FILE__);
	status.txstate = NoTx;

	// return OK even though the status is ABORTED
	RETURN SVAS_OK;
}

VASResult
svas_server::abortTrans(
	int				reason //  = SVAS_UserAbort
)
// called by user 
{
	VFPROLOGUE(svas_server::abortTrans);
	res =  abortTrans(this->transid, reason);
	dassert(enter_count == 0);
	DBG(<<"returning " << res);
	RETURN res;
}

/*
 * Testing aborts and savepoints:
 *
 * rollback to a savepoint:
 * interrupt a client's transaction:
 * use the "interrupt" command from the server shell.
 *
 */

VASResult		
svas_server::abort2savepoint(
	IN(sm_save_point_t) 	sp
) 
{
	VFPROLOGUE(svas_server::abort2savepoint);

	dassert(status.txstate != Aborting);
	dassert(xct() != (xct_t *)0);
	dassert(_xact  != (xct_t *)0);
	DBG(<<"rolling back to savept at " << (void *)&sp << " "  << sp);

	errlog->clog << info_prio << "ABORT(savept) " << transid << flushl;

FSTART
	dassert( me()->xct()==_xact );
	dassert( sp.tid() == transid);
	dassert( ShoreVasLayer.Sm->xct_to_tid(_xact) == transid);

	if( _dirservice == ds_degree2 && in_quark() &&
		_context == directory_op
		) {
		DBG(<<" in dir tx");
		dassert( ShoreVasLayer.Sm->xct_to_tid(_xact) == _dirxct->tid());
		dassert( transid == _dirxct->tid());
		dassert( _context == directory_op);
		// abort dir tx and resume client tx
		_DO_(abort_parallel());
	} else {
		// we could be here because _dirservice is same, 
		// in which case we can be in directory_op context
		// but have no quark

		DBG(<<" in client tx");
		dassert( !in_quark() );
	}

	// TODO: warning -- scan state might be a problem
	// after a rollback to a savepoint, if updates were
	// done during the scan.

	if SMCALL(rollback_work(sp)) {
		VERR(SVAS_InternalError);
		FAIL;
	}

FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult
svas_server::_commitTrans(
	IN(tid_t)     tid,
	bool			chain // = false
)
{
	LOGVFPROLOGUE(svas_server::_commitTrans);

	RETURN   _trans(chain? g_commitchain:g_commit, tid);
}

VASResult
svas_server::commitTrans(
	IN(tid_t)     tid 
)
{
	LOGVFPROLOGUE(svas_server::commitTrans);

	errlog->clog << info_prio << "COMMIT " << transid  << flushl;
	TX_REQUIRED;
FSTART
	_DO_(_commitTrans(tid));
FOK:
	res = SVAS_OK;
FFAILURE:
	LEAVE;
	DBG(<<"returning " << res);
	RETURN res;
}

VASResult
svas_server::resumeTrans(
	IN(tid_t)     tid 
)
{
	LOGVFPROLOGUE(svas_server::resumeTrans);

	errlog->clog << info_prio << "RESUME " << tid  << flushl;
	TX_REQUIRED;

	res =   _trans(g_resume, tid);
	LEAVE;
	DBG(<<"returning " << res);
	RETURN res;
}

VASResult
svas_server::suspendTrans(
	IN(tid_t)     tid 
)
{
	LOGVFPROLOGUE(svas_server::suspendTrans);

	errlog->clog << info_prio << "SUSPEND " << tid  << flushl;
	TX_REQUIRED;

	res =   _trans(g_suspend, tid);
	LEAVE;
	DBG(<<"returning " << res);
	RETURN res;
}

//
//
// FOR MANAGING PARALLEL transactions for directory operations:
//

VASResult
svas_server::commit_parallel(bool /* release */)
{
	VFPROLOGUE(svas_server::commit_parallel);
	// commit it, wipe out xct ptr
	// don't bother with lazy commit
	// for now
	//
	res = SVAS_OK;

	DBG(<<"closing");
	if( in_quark() )  {
		if CALL( _dirxct->close(true) ) {
			VERR(SVAS_SmFailure);
			res = SVAS_FAILURE;
			// this is really a catastrophic error
			assert(0);
		}
	} else {
		// already released because of an
		// abort or rollback to a savepoint
		dassert(status.vasresult != SVAS_OK);
	}
	dassert(me()->xct()==_xact);
	dassert(!in_quark());
	DBG(<<"returning " << res);
	RETURN res;
} 

//
VASResult
svas_server::abort_parallel(bool release)
{
	VFPROLOGUE(svas_server::abort_parallel);

	// they are the same with quarks
	RETURN commit_parallel(release);
} 

VASResult
svas_server::suspend_parallel(bool release) // and resume_client
{
	VFPROLOGUE(svas_server::suspend_parallel);

	// they are all the same with quarks
	RETURN commit_parallel(release);
} 

VASResult
svas_server::begin_parallel()
{
	VFPROLOGUE(svas_server::begin_parallel);
	dassert(txstate(_xact)==Active);

	if(!_dirxct) {
		_dirxct = new sm_quark_t;
	}
	dassert(!*_dirxct); // had better not be
						// opened already

	//
	// start a new quark for directory operations
	//
	audit_context(true);

	if CALL(_dirxct->open()) {
		VERR(SVAS_InternalError);
		// really a catastrophic error
		assert(0);
		RETURN SVAS_FAILURE;
	}
	dassert(in_quark());
	RETURN SVAS_OK;
} 

VASResult
svas_server::resume_parallel() // no such thing for now
{
	VFPROLOGUE(svas_server::resume_parallel);
	dassert(txstate(_xact)==Active);

	// for now, it's just a begin,
	// since suspend is close
	RETURN begin_parallel();
}

//
// change_context:
// it's an error if the context is already the one
// we're asking for.
// 
VASResult	
svas_server::change_context(
#ifdef DEBUG
	const char *file, int line,
#endif
	operation newcontext, 
	bool release,
	bool docommit, bool doabort
) 
{
	VFPROLOGUE(svas_server::change_context);
	DBG(<<"change context to " << newcontext
		<<" docommit==" << docommit
		<<" doabort==" << doabort
		<< " from " << line << " " << file
		);
	res = SVAS_OK;

	dassert(!(doabort&&docommit));

	dassert( _context != newcontext); 
	// we already know that we're not already
	// in the newcontext, so :
	// if we're in client, going to directory
	// and there's not already a tx for directory
	// operations, we have to create one
	//
	// if we're in directory context, there
	// had better be a directory tx already
	// so if that's the case, we'll let the
	// audit catch it.
	//
	sysp_cache->invalidate();

	if(_dirservice == ds_degree2 ) {

		DBG(<<" parallel service ");
		if(newcontext == client_op) {
			//
			// we're switching to client
			//
			//
			// Until such time as we want (and want?)
			// nested transactions for directory operations,
			// leaving a directory-operation context
			// means closing the quark, and there
			// is no difference between committing and
			// aborting; suspending is also treated
			// like a commit/abort.  (Re-entering means
			// opening another quark....) so... just
			// override the arguments docommit and doabort:
			//
			doabort=false; docommit=true; // for now

			// suspend or commit directory tx
			if(docommit) {
				res = commit_parallel(release);
			} else if(doabort) {
				res = abort_parallel(release);
			} else {
				res = suspend_parallel(release);
			}

		} else {
			//
			// we're switching to directory
			//
			dassert(newcontext == directory_op);
			res = resume_parallel();
		}
	}
	_context = newcontext;
	DBG(<<" context is now " << _context);
	audit_context(docommit);
	DBG(<<"returning " << res << " from " << file << " line# " << line);
	RETURN res;
}

void
svas_server::audit_no_tx_context()
{
	// this is what must be true
	// when there is no client tx
	dassert(_xact == 0);
	dassert(!in_quark());
	dassert(_context == client_op);
}
void
svas_server::audit_context(bool dircommitted) 
{
	VFPROLOGUE(svas_server::audit_context);
	// _dirxct     -- directory tx quark
	// _xact	   -- client tx
	// me()->xct() -- what's attached

	if(_dirservice == ds_degree2 ) {
		// there must be a directory tx that's
		// not the same as the client tx
		DBG(<<" parallel service ");

		if(status.txstate != NoTx) {
			dassert(_xact != 0);
			dassert(txstate(_xact) == Active);
		} else {
			dassert(txstate(_xact) != Active);
		}
		if(dircommitted) {
			dassert(!in_quark());
		} else {
			dassert(in_quark());
		}

		if(_context == client_op) {
			if(status.txstate != NoTx) {
				// assert client tx is attached
				dassert(_xact == me()->xct());
			}
		} else {
			dassert(_context== directory_op);
			dassert( !dircommitted );
		}
	} else {
		DBG(<<" no parallel service  ");
		dassert(_dirservice == ds_degree3);

		// there's no directory tx quark
		dassert(!in_quark());

		// there's a client tx, active, attached
		if(status.txstate != NoTx) {
			dassert(_xact != 0);
			dassert(txstate(_xact) == Active);
			dassert(_xact == me()->xct());
		}
	}
	RETURN;
}

enum svas_server::operation 
svas_server::assure_context(
#ifdef DEBUG
	const char *file, int line,
#endif
operation c, bool release)
{
	VFPROLOGUE(svas_server::assure_context);

	DBG(<<"assure_context("<<c<<") from " << line << " " << file);
	operation save = _context;
	if(_context != c) {
		DBG(<<"assure_context -- need to switch");
		if( change_context(
#ifdef DEBUG
		__FILE__,__LINE__,
#endif
		c, release)!= SVAS_OK) {
			assert(0);
		}
		dassert(_context ==  c);
	} else {
		dassert(_context ==  c);
		DBG(<<"assure_context-- didn't need to switch");
		audit_context();
	}
	dassert(_context ==  c);
	DBG(<<"end of assure_context("<<c<<") from " << line << " " << file
		<<"returning " << save);
	RETURN save;
}

void 
svas_server::leave(const char *fn)
{
	VFPROLOGUE(svas_server::leave);

	DBG(<<"Leave from " << fn);
#ifdef DEBUG
	if(--enter_count < 0) {
		cerr << "more leaves than enters!" << enter_count << endl; dassert(0);
	}
#endif
	dassert( _context== client_op);
	//
	// we should always in client_op mode when we leave
	//

	if(_context == client_op && 
		_dirservice == ds_degree2  && in_quark()
		) {
		//
		// end the directory tx
		//
		dassert(txstate(_xact)==Active);

		// if there were updates by the directory tx (we're 
		// about to abort it), we must be leaving in error,
		// and returning an error status
		if(status.vasresult == SVAS_OK) {
			dassert(status.vasreason == 0);
			if(commit_parallel() != SVAS_OK) {
				// really a catastrophic error
				assert(0);
			}
		} else {
			if(abort_parallel() != SVAS_OK) {
				// really a catastrophic error
				assert(0);
			}
		}
	}
	audit_end_directory_op();

	// translate some SM errors
	if(status.vasresult == SVAS_FAILURE &&
		status.vasreason == SVAS_SmFailure) {
		VASResult replacement = SVAS_OK;

		switch(status.smreason) {
			case ss_m::eBADLOGICALID:
				replacement = SVAS_NotFound;
				break;
			case ss_m::eBADSTART:
			case ss_m::eBADAPPEND:
			case ss_m::eBADLENGTH:
				replacement = SVAS_BadRange;
				break;
			case ss_m::eBADLOGICALIDTYPE:
				replacement = SVAS_WrongObjectType;
				break;
			case ss_m::eDUPLICATE:
				replacement = SVAS_Already;
				break;
			default:
				break;
		}
		if(replacement != SVAS_OK) {
			VERR(replacement);
		}
	}
	DBG(<<"END Leave from " << fn);
}

VASResult	
svas_server::set_service(dirservice d)
{ 
	FUNC(svas_server::set_service);
	// can only happen when in client context, and
	// only if no dir tx is running:
	VASResult r;
	dassert(!in_quark() && _context==client_op);
	_dirservice=d; 
	RETURN SVAS_OK;
}

