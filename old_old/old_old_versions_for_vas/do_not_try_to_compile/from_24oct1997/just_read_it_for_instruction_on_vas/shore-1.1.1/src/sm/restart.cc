/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: restart.cc,v 1.108 1997/06/15 03:13:50 solomon Exp $
 */
#define SM_SOURCE
#define RESTART_C

#ifdef __GNUG__
#pragma implementation "restart.h"
#pragma implementation "restart_s.h"
#endif

#include <sm_int_1.h>
#include "restart.h"
#include "restart_s.h"


//#define LOGTRACE(x)  cout x << endl;
#define LOGTRACE(x)  DBG(x)

#ifdef __GNUG__
template class w_hash_t<dp_entry_t, bfpid_t>;
template class w_hash_i<dp_entry_t, bfpid_t>;
template class w_list_t<dp_entry_t>;
template class w_list_i<dp_entry_t>;
#endif

tid_t		restart_m::_redo_tid;

/*********************************************************************
 *
 *  class dirty_pages_tab_t
 *
 *  In memory dirty pages table -- a dictionary of of pid and 
 *  its recovery lsn.
 *
 *********************************************************************/
class dirty_pages_tab_t {
public:
    NORET			dirty_pages_tab_t(int sz);
    NORET			~dirty_pages_tab_t();
    
    dirty_pages_tab_t& 		insert(
	const lpid_t& 		    pid,
	const lsn_t& 		    lsn);

    dirty_pages_tab_t& 		remove(const lpid_t& pid);

    bool 			look_up(
	const lpid_t& 		    pid,
	lsn_t** 		    lsn = 0); 

    lsn_t 			min_rec_lsn();

    friend ostream& operator<<(ostream&, const dirty_pages_tab_t& s);
    
private:
    w_hash_t<dp_entry_t, bfpid_t> tab; // hash table for dictionary

    // disabled
    NORET			dirty_pages_tab_t(const dirty_pages_tab_t&);
    dirty_pages_tab_t&		operator=(const dirty_pages_tab_t&);

    lsn_t			cachedMinRecLSN;
    bool			validCachedMinRecLSN;
};


/*********************************************************************
 *  
 *  restart_m::recover(master)
 *
 *  Start the recovery process. Master is the master lsn (lsn of
 *  the last successful checkpoint record).
 *
 *********************************************************************/
void 
restart_m::recover(lsn_t master)
{
    dirty_pages_tab_t dptab(2 * bf->npages());
    lsn_t redo_lsn;
    bool found_xct_freeing_space = false;

    // set so mount and dismount redo can tell that they should log stuff.

    smlevel_0::errlog->clog << info_prio << "Restart recovery:" << flushl;
#ifdef DEBUG
    {
	DBG(<<"TX TABLE before analysis:");
	xct_i iter;
	xct_t* xd;
	while ((xd = iter.next()))  {
	w_assert1(  xd->state() == xct_t::xct_active ||
		    xd->state() == xct_t::xct_prepared ||
		    xd->state() == xct_t::xct_freeing_space );
	DBG(<< "transaction " << xd->tid() << " has state " << xd->state());
	}
	DBG(<<"END TX TABLE before analysis:");
    }
#endif /* DEBUG */

    /*
     *  Phase 1: ANALYSIS.
     *  Output : dirty page table and redo lsn
     */
    smlevel_0::errlog->clog << info_prio << " analysis ..." << flushl;

    DBG(<<"starting analysis at " << master << " redo_lsn = " << redo_lsn);
    analysis_pass(master, dptab, redo_lsn, found_xct_freeing_space);

    /*
     *  Phase 2: REDO -- use dirty page table and redo lsn of phase 1
     * 		 We save curr_lsn before redo_pass() and assert after
     *		 redo_pass that no log record has been generated.
     *  pass in highest_lsn for debugging
     */
    smlevel_0::errlog->clog << info_prio << " redo ..." << flushl;
    lsn_t curr_lsn = log->curr_lsn(); 

#ifdef DEBUG
    {
	DBG(<<"TX TABLE at end of analysis:");
	xct_i iter;
	xct_t* xd;
	while ((xd = iter.next()))  {
	    w_assert1(  xd->state() == xct_t::xct_active ||
			xd->state() == xct_t::xct_prepared);
	    DBG(<< "Transaction " << xd->tid() << " has state " << xd->state());
	}
	DBG(<<"END TX TABLE at end of analysis:");
    }
#endif

    DBG(<<"starting redo at " << redo_lsn << " highest_lsn " << curr_lsn);
    redo_pass(redo_lsn, curr_lsn, dptab);


    /* no logging during redo */
    w_assert1(curr_lsn == log->curr_lsn()); 

    /*
     * free the exts of files which were started to be freed.
     * this needs to be done before undo since nothing prevents (no locks) the
     * reuse of an extent which is marked free and therefore extents after this
     * one will become unreachable when the next field of the reused extent is
     * reallocated.
     */
    if (found_xct_freeing_space)  {
	xct_t*	xd = new xct_t();
	w_assert1(xd);
	W_COERCE( io_m::free_stores_during_recovery(t_store_freeing_exts) );
	W_COERCE( xd->commit(false) );
	delete xd;
    }


    /*
     *  Phase 3: UNDO -- abort all active transactions
     *  pass in highest_lsn for debugging
     */
    smlevel_0::errlog->clog  << info_prio<< " undo ..." 
	<< " curr_lsn = " << curr_lsn
	<< flushl;

    undo_pass();

    /*
     * if there are any files with there deleting bit still set then it was set by
     * a xct which completed (entered freeing_space mode), but not yet done freeing
     * space.  these files are destroyed here.
     */
    if (found_xct_freeing_space)  {
	xct_t*	xd = new xct_t();
	w_assert1(xd);
	W_COERCE( io_m::free_stores_during_recovery(t_deleting_store) );
	W_COERCE( io_m::free_exts_during_recovery() );
	W_COERCE( xd->commit(false) );
	delete xd;
    }

    smlevel_0::errlog->clog << info_prio << "Oldest active transaction is " 
	<< xct_t::oldest_tid() << flushl;
    smlevel_0::errlog->clog << info_prio 
	<< "First new transaction will be greater than "
	<< xct_t::youngest_tid() << flushl;
    smlevel_0::errlog->clog << info_prio << "Restart successful." << flushl;

#ifdef DEBUG
    {
	DBG(<<"TX TABLE at end of recovery:");
	xct_i iter;
	xct_t* xd;
	while ((xd = iter.next()))  {
	    w_assert1(xd->state()==xct_t::xct_prepared);
	    server_handle_t ch = xd->get_coordinator();

	    DBG(<< "transaction " << xd->tid() 
		<< " has state " << xd->state()
		<< " coordinator " << ch
		);
	}
	DBG(<<"END TX TABLE at end of recovery:");
    }
#endif /* DEBUG */
}

/*********************************************************************
 *
 *  restart_m::analysis_pass(master, dptab, redo_lsn)
 *
 *  Scan log forward from master_lsn. Insert and update dptab.
 *  Compute redo_lsn.
 *
 *********************************************************************/
void 
restart_m::analysis_pass(
    lsn_t 		master,
    dirty_pages_tab_t&	dptab,
    lsn_t& 		redo_lsn,
    bool&		found_xct_freeing_space
)
{
    FUNC(restart_m::analysis_pass);

    AutoTurnOffLogging turnedOnWhenDestroyed;

    redo_lsn = null_lsn;
    found_xct_freeing_space = false;
    if (master == null_lsn) return;

    smlevel_0::in_analysis = true;

    /*
     *  Open a forward scan
     */
    log_i 	scan(*log, master);
    logrec_t*	log_rec_buf;
    lsn_t	lsn;

    lsn_t	theLastMountLSNBeforeChkpt;

    /*
     *  Assert first record is Checkpoint Begin Log
     *  and get last mount/dismount lsn from it
     */
    {
	if (! scan.next(lsn, log_rec_buf)) {
	    W_FATAL(eINTERNAL);
	}
	logrec_t&	r = *log_rec_buf;
	w_assert1(r.type() == logrec_t::t_chkpt_begin);
	theLastMountLSNBeforeChkpt = *(lsn_t *)r.data();
	DBG( << "last mount LSN from chkpt_begin=" << theLastMountLSNBeforeChkpt);
    }
    
    /*
     *  Number of complete chkpts handled.  Only the first
     *  chkpt is actually handled.  There may be a second
     *  complete chkpt due to a race condition between writing
     *  a chkpt_end record, updating the master lsn and crashing.
     *  Used to avoid processing an incomplete checkpoint.
     */
    int num_chkpt_end_handled = 0;

    while (scan.next(lsn, log_rec_buf)) {
	logrec_t&	r = *log_rec_buf;

	/*
	 *  Scan next record
	 */
	LOGTRACE( << lsn << " A: " << r );
	xct_t* xd = 0;

	/*
	 *  If log is transaction related, insert the transaction
	 *  into transaction table if it is not already there.
	 */
	if ((r.tid() != null_tid) && ! (xd = xct_t::look_up(r.tid()))) {
	    DBG(<<"analysis: inserting tx " << r.tid() << " active ");
	    xd = new xct_t(r.tid(), xct_t::xct_active, lsn, r.prev());
	    w_assert1(xd);
	    xct_t::update_youngest_tid(r.tid());
	}

	/*
	 *  Update last lsn of transaction
	 */
	if (xd) {
	    xd->set_last_lsn(lsn);
	    w_assert1( xd->tid() == r.tid() );
	}

	switch (r.type()) {
	case logrec_t::t_xct_prepare_st:
	case logrec_t::t_xct_prepare_lk:
	case logrec_t::t_xct_prepare_alk:
	case logrec_t::t_xct_prepare_stores:
	case logrec_t::t_xct_prepare_fi:
	    if (num_chkpt_end_handled == 0)  {
		// - redo now, because our redo phase can start after
		//   the master checkpoint.
		// - records after chkpt will be handled in redo and only
		//   if the xct is not in the prepared state to prevent
		// - redoing these records.
		//   records before/during chkpt will be ignored in redo
		r.redo(0);
	    }
	    break;

	case logrec_t::t_chkpt_begin:
	    /*
	     *  Found an incomplete checkpoint --- ignore 
	     */
	    break;

	case logrec_t::t_chkpt_bf_tab:
	    if (num_chkpt_end_handled == 0)  {
		/*
		 *  Still processing the master checkpoint record.
		 *  For each entry in log,
		 *	if it is not in dptab, insert it.
		 *  	If it is already in the dptab, update the rec_lsn.
		 */
		const chkpt_bf_tab_t* dp = (chkpt_bf_tab_t*) r.data();
		for (uint i = 0; i < dp->count; i++)  {
		    lsn_t* rec_lsn;
		    if (! dptab.look_up(dp->brec[i].pid, &rec_lsn))  {
			DBG(<<"dptab.insert dirty pg " 
			<< dp->brec[i].pid << " " << dp->brec[i].rec_lsn);
			dptab.insert(dp->brec[i].pid, dp->brec[i].rec_lsn);
		    } else {
			DBG(<<"dptab.update dirty pg " 
			<< dp->brec[i].pid << " " << dp->brec[i].rec_lsn);
			*rec_lsn = dp->brec[i].rec_lsn;
		    }
		}
	    }
	    break;
		
	case logrec_t::t_chkpt_xct_tab:
	    if (num_chkpt_end_handled == 0)  {
		/*
		 *  Still processing the master checkpoint record.
		 *  For each entry in the log,
		 * 	If the xct is not in xct tab, insert it.
		 */
		const chkpt_xct_tab_t* dp = (chkpt_xct_tab_t*) r.data();
		xct_t::update_youngest_tid(dp->youngest);
		for (uint i = 0; i < dp->count; i++)  {
		    xct_t* xd = xct_t::look_up(dp->xrec[i].tid);
		    if (!xd) {
			if (dp->xrec[i].state != xct_t::xct_ended)  {
			    xd = new xct_t(dp->xrec[i].tid,
					   dp->xrec[i].state,
					   dp->xrec[i].last_lsn,
					   dp->xrec[i].undo_nxt);
			    DBG(<<"add xct " << dp->xrec[i].tid
				    << " state " << dp->xrec[i].state
				    << " last lsn " << dp->xrec[i].last_lsn
				    << " undo " << dp->xrec[i].undo_nxt
				);
			    w_assert1(xd);
			}
			// skip finished ones
			// (they can get in there!)
		    } else {
		       w_assert3(dp->xrec[i].state != xct_t::xct_ended);
		    }
		}
	    }
	    break;
	    
	case logrec_t::t_chkpt_dev_tab:
	    if (num_chkpt_end_handled == 0)  {
		/*
		 *  Still processing the master checkpoint record.
		 *  For each entry in the log, mount the device.
		 */
		const chkpt_dev_tab_t* dv = (chkpt_dev_tab_t*) r.data();
		for (uint i = 0; i < dv->count; i++)  {
		    smlevel_0::errlog->clog << info_prio << "  device " << dv->devrec[i].dev_name 
			 << " will be recovered as vid " << dv->devrec[i].vid
			 << flushl;
		    W_COERCE(io_m::mount(dv->devrec[i].dev_name, 
				       dv->devrec[i].vid));

		    w_assert3(io_m::is_mounted(dv->devrec[i].vid));
		}
	    }
	    break;
	
	case logrec_t::t_dismount_vol:
	case logrec_t::t_mount_vol:
	    /* JK: perform all mounts and dismounts up to the minimum redo lsn, so that the system
	     * has the right volumes mounted during the redo phase.  the only time the this should
	     * be redone is when no dirty pages were in the checkpoint and a mount/dismount occurs
	     * before the first page is dirtied after the checkpoint.  the case of the first dirty
	     * page occuring before the checkpoint is handled by undoing mounts/dismounts back to
	     * to the min dirty page lsn in the analysis_pass after the log has been scanned.
	     */
	    w_assert3(num_chkpt_end_handled > 0);  // mount & dismount shouldn't happen during a check point
	    if (lsn < dptab.min_rec_lsn())  {
		r.redo(0);
	    }
	    break;
		
	case logrec_t::t_chkpt_end:
	    /*
	     *  Done with the master checkpoint record. Flag true 
	     *  to avoid processing an incomplete checkpoint.
	     */
#ifdef DEBUG
	    {
		const lsn_t* l = (lsn_t*) r.data();
		const lsn_t* l2 = ++l;

		DBG(<<"checkpt end: master=" << *l 
		    << " min_rec_lsn= " << *l2);

		if(lsn == *l) {
		    w_assert3(*l2 == dptab.min_rec_lsn());
		}
	    }

	    if (num_chkpt_end_handled > 2) {
		/*
		 * We hope we do not encounter more than one complete chkpt.
		 * Unfortunately, we *can* crash between the flushing
		 * of a checkpoint-end record and the time we
		 * update the master record (move the pointer to the last
		 * checkpoint)
		 */
		smlevel_0::errlog->clog << "Error: more than 2 complete checkpoints found! " <<flushl;
		/* 
		 * comment out the following if you are testing
		 * a situation that involves a crash at the
		 * critical point
		 */
		w_assert1(0);
	    }

#endif /*DEBUG*/

	    num_chkpt_end_handled++;
	    break;


	case logrec_t::t_xct_freeing_space:
		xd->change_state(xct_t::xct_freeing_space);
		break;

	case logrec_t::t_xct_end:
	    /*
	     *  Remove xct from xct tab
	     */
		if (xd->state() == xct_t::xct_prepared || xd->state() == xct_t::xct_freeing_space) {
			/*
			 * was prepared in the master
			 * checkpoint, so the locks
			 * were acquired.  have to free them
			 */
		    me()->attach_xct(xd);	
		    // release all locks (1st true) and don't free extents which hold locks (2nd true)
		    W_COERCE( lm->unlock_duration(t_long, true, true) )
		    me()->detach_xct(xd);	
		}
	    xd->change_state(xct_t::xct_ended);
	    delete xd;
	    break;

	default:
	    if (r.is_page_update()) {
		if (r.is_undo()) {
		    /*
		     *  r is undoable. Update next undo lsn of xct
		     */
		    xd->set_undo_nxt(lsn);
		}
		if (r.is_redo() && !(dptab.look_up(r.pid()))) {
		    /*
		     *  r is redoable and not in dptab ...
		     *  Register a new dirty page.
		     */
		    DBG(<<"dptab.insert dirty pg " << r.pid() << " " << lsn);
		    dptab.insert( r.pid(), lsn );
		}

	    } else if (r.is_cpsn()) {
		/* 
		 *  Update undo_nxt lsn of xct
		 */
		if(r.is_undo()) {
		    /*
		     *  r is undoable. There is one possible case of
		     *  this (undoable compensation record)
		     */
		    xd->set_undo_nxt(lsn);
		} else {
		    xd->set_undo_nxt(r.undo_nxt());
		}
		if (r.is_redo() && !(dptab.look_up(r.pid()))) {
		    /*
		     *  r is redoable and not in dptab ...
		     *  Register a new dirty page.
		     */
		    DBG(<<"dptab.insert dirty pg " << r.pid() << " " << lsn);
		    dptab.insert( r.pid(), lsn );
		}
	    } else if (r.type()!=logrec_t::t_comment) {
		W_FATAL(eINTERNAL);
	    }
	}
    }

    /*
     *  Start of redo is the minimum of recovery lsn of all entries
     *  in the dirty page table.
     */
    redo_lsn = dptab.min_rec_lsn();

    /*
     * undo any mounts/dismounts that occured between chkpt and min_rec_lsn
     */
    DBG( << ((theLastMountLSNBeforeChkpt != lsn_t::null && theLastMountLSNBeforeChkpt > redo_lsn) \
		? "redoing mounts/dismounts before chkpt but after redo_lsn"  \
		: "no mounts/dismounts need to be redone"));

    while (theLastMountLSNBeforeChkpt != lsn_t::null 
	&& theLastMountLSNBeforeChkpt > redo_lsn)  {
	W_COERCE(log->fetch(theLastMountLSNBeforeChkpt, log_rec_buf));  
	// HAVE THE LOG_M MUTEX
	// We have to release it in order to do the mount/dismounts
	// so we make a copy of the log record

	logrec_t& r = *log_rec_buf;
	logrec_t 	copy = r;
	log->release();

	DBG( << theLastMountLSNBeforeChkpt << ": " << copy );

	w_assert3(copy.type() == logrec_t::t_dismount_vol || 
	    copy.type() == logrec_t::t_mount_vol);

	chkpt_dev_tab_t *dp = (chkpt_dev_tab_t*)copy.data();
	w_assert3(dp->count == 1);

	// it is ok if the mount/dismount fails, since this may be caused by the destruction
	// of the volume.  if that was the case then there won't be updates that need to be
	// done/undone to this volume so it doesn't matter.
	if (copy.type() == logrec_t::t_dismount_vol)  {
	    W_IGNORE(io_m::mount(dp->devrec[0].dev_name, dp->devrec[0].vid));
	}  else  {
	    W_IGNORE(io_m::dismount(dp->devrec[0].vid));
	}

	theLastMountLSNBeforeChkpt = copy.prev();
    }
    io_m::SetLastMountLSN(theLastMountLSNBeforeChkpt);

    /*
     * delete xcts which are freeing space
     */
    bool done = false;
    while (!done)  {
	{  // start scope so iter gets reinitialized
	    xct_i	iter;
	    xct_t*	xd;

	    done = true;
	    while ((xd = iter.next()))  {
		if (xd->state() == xct_freeing_space)  {
		    DBG( << xd->tid() << " was found freeing space after analysis, deleting" );
		    done = false;
		    found_xct_freeing_space = true;
		    me()->attach_xct(xd);
		    W_COERCE( xd->dispose() );
		    delete xd;
		    break;
		}  else  {
		    DBG( << xd->tid() << " was not freeing space after analysis" );
		}
	    }
	}
    }
    smlevel_0::in_analysis = false;
}



/*********************************************************************
 * 
 *  restart_m::redo_pass(redo_lsn, highest_lsn, dptab)
 *
 *  Scan log forward from redo_lsn. Base on entries in dptab, 
 *  apply redo if durable page is old.
 *
 *********************************************************************/
#ifndef DEBUG
#define highest_lsn /* highes_lsn not used */
#endif
void 
restart_m::redo_pass(
    lsn_t redo_lsn, 
    const lsn_t &highest_lsn, 
    dirty_pages_tab_t& dptab
)
#undef highest_lsn
{
    FUNC(restart_m::redo_pass);

    AutoTurnOffLogging turnedOnWhenDestroyed;

    /*
     *  Open a scan
     */
    log_i scan(*log, redo_lsn);

    /*
     *  Allocate a (temporary) log record buffer for reading 
     */
    logrec_t* log_rec_buf=0;

    lsn_t lsn;
    while (scan.next(lsn, log_rec_buf))  {
	logrec_t& r = *log_rec_buf;
	/*
	 *  For each log record ...
	 */
	lsn_t* rec_lsn = 0;		// points to rec_lsn in dptab entry

	if (!r.valid_header(lsn)) {
	    smlevel_0::errlog->clog << error_prio 
	    << "Internal error during redo recovery." << flushl;
	    smlevel_0::errlog->clog << error_prio 
	    << "    log record at position: " << lsn 
	    << " appears invalid." << endl << flushl;
	    abort();
	}

	bool redone = false;
	LOGTRACE( << lsn << " R: " << r );
	if ( r.is_redo() ) {
	    if (r.pid() == lpid_t::null) {
		/*
		 * Handle prepare stuff
		 * if the transaction is still
		 * in the table after analysis, it
		 * didn't get committed or aborted yet,
		 * so go ahead and process it.  If
		 * it isn't in the table, it was
		 * already committed or aborted.
		 */
		if (r.tid() != null_tid)  {
		    xct_t *xd = xct_t::look_up(r.tid());
		    if (xd) {
			if (xd->state() == xct_t::xct_active)  {
			    DBG(<<"redo - no page, xct is " << r.tid());
			    r.redo(0);
			    redone = true;
			}  else  {
			    w_assert1(xd->state() == xct_t::xct_prepared);
			    w_assert3(r.type() == logrec_t::t_xct_prepare_st
				||    r.type() == logrec_t::t_xct_prepare_lk
				||    r.type() == logrec_t::t_xct_prepare_alk
				||    r.type() == logrec_t::t_xct_prepare_stores
				||    r.type() == logrec_t::t_xct_prepare_fi);
			}
		    }
		}  else  {
		    // JK: redo mounts and dismounts, at the start of redo, all the volumes which
		    // were mounted at the redo lsn should be mounted.  need to do this to take
		    // care of the case of creating a volume which mounts the volume under a temporary
		    // volume id inorder to create stores and initialize the volume.  this temporary
		    // volume id can be reused, which is why this must be done.

		    w_assert3(r.type() == logrec_t::t_dismount_vol || r.type() == logrec_t::t_mount_vol);
		    DBG(<<"redo - no page, no xct ");
		    r.redo(0);
			io_m::SetLastMountLSN(lsn);
		    redone = true;
		}

	    } else {
		if(dptab.look_up(r.pid(), &rec_lsn) && lsn >= *rec_lsn)  {
		/*
		 *  We are only concerned about log records that involve
		 *  page updates.
		 */
		DBG(<<"redo page update, pid " 
			<< r.pid() 
			<< " dirty page lsn: "  << *rec_lsn
			);
		w_assert1(r.pid().page); 

		/*
		 *  Fix the page.
		 */ 
		page_p page;

		/* 
		 * The following code determines whether to perform
		 * redo on the page.  If the log record is for a page
		 * format (page_init) then there are two possible
		 * implementations.
		 * 
		 * 1) Trusted LSN on New Pages
		 *   If we assume that the LSNs on new pages can always be
		 *   trusted then the code reads in the page and 
		 *   checks the page lsn to see if the log record
		 *   needs to be redone.  Note that this requires that
		 *   pages on volumes stored on a raw device must be
		 *   zero'd when the volume is created.
		 * 
		 * 2) No Trusted LSN on New Pages
		 *   If new pages are not in a known (ie. lsn of 0) state
		 *   then when a page_init record is encountered, it
		 *   must always be redone and therefore all records after
		 *   it must be redone.
		 *   
		 * These are selected by the preprocessor symbol
		 * DONT_TRUST_PAGE_LSN (defined in shore.def).
		 */
#ifndef DONT_TRUST_PAGE_LSN
		store_flag_t store_flags = st_bad;
		DBG(<< "TRUST_PAGE_LSN");
		W_COERCE( page.fix(r.pid(), 
				page_p::t_any_p, 
				LATCH_EX, 
				0,  // page_flags
				store_flags,
				true // ignore store_id
				) );

#ifdef DEBUG
		if(r.pid() != page.pid()) {
                    DBG(<<"Pids don't match: expected " << r.pid()
                        << " got " << page.pid());
                }
#endif /* DEBUG */

		lsn_t page_lsn = page.lsn();
		DBG(<<" page lsn " << page_lsn);
		if (page_lsn < lsn) 
#else
		DBG(<< "DON'T TRUST_PAGE_LSN");
		uint4_t page_flags = 0;
		if (r.type() == logrec_t::t_page_init
		    || r.type() == logrec_t::t_page_format) {
		    page_flags = page_p::t_virgin;
		}
		W_COERCE( page.fix(r.pid(), 
			page_p::t_any_p, 
			LATCH_EX, 
			page_flags, 
			page_p::st_bad,  // store flags
			true // ignore store id
			) );

		lsn_t page_lsn = page.lsn();
		DBG(<<" page lsn " << page_lsn);
		if (page_lsn < lsn ||  (page_flags & page_p::t_virgin)) 
#endif /* ndef DONT_TRUST_PAGE_LSN */
	
		{
		    /*
		     *  Redo must be performed if page has lower lsn 
		     *  than record
		     */
		    xct_t* xd = 0; 
		    if (r.tid() != null_tid)  { 
			if ((xd = xct_t::look_up(r.tid())))  {
			    /*
			     * xd will be aborted following redo
			     * thread attached to xd to make sure that
			     * redo is correct for the transaction
			     */
			    me()->attach_xct(xd);
			}
		    }

		    /*
		     *  Perform the redo. Do not generate log.
		     */
		    {
			redone = true;
			// remember the tid for space resv hack.
			_redo_tid = r.tid();
			r.redo(page ? &page : 0);
			_redo_tid = tid_t::null;
			page.set_lsn(lsn);	/* page is updated */
		    }
			
		    if (xd) me()->detach_xct(xd);
			
		} else 
#ifdef DEBUG
		if(page_lsn >= highest_lsn) {
		    cerr << "WAL violation! page " 
		    << page.pid()
		    << " has lsn " << page_lsn
		    << " end of log is record prior to " << highest_lsn
		    << endl;

		    W_FATAL(eINTERNAL);
		} else
#endif
		{
		    /*
		     *  Increment recovery lsn of page to indicate that 
		     *  the page is younger than this record
		     */
		    *rec_lsn = page_lsn.advance(1);
		}

		// page.destructor is supposed to do this:
		// page.unfix();
	    }
	    }
	}
	LOGTRACE( << (redone ? " redo" : " skip") );
    }
}



/*********************************************************************
 *
 *  class sm_undo_thread_t
 *
 *  Thread that aborts the transaction supplied during construction.
 *
 *********************************************************************/
class sm_undo_thread_t : public smthread_t {
public:
    NORET			sm_undo_thread_t(
	xct_t*			    xd);
    NORET			~sm_undo_thread_t()   {};

    virtual void		run();
private:
    xct_t*			_xd;
};


/*********************************************************************
 *
 *  restart_m::undo_pass()
 *
 *  For each active transaction, fork an sm_undo_thread_t (worker)
 *  to abort the transaction.
 *
 *********************************************************************/
void 
restart_m::undo_pass()
{
    FUNC(restart_m::undo_pass);

    int count = 0;
    sm_undo_thread_t* worker[max_xcts];

    /*
     *  Fork workers
     */
    xct_t* xd;

#define CAREFUL_RESTART
#ifdef CAREFUL_RESTART

    /*
     * NB: this is NOT SUFFICIENT to deal with the
     * restart-undo processing of btrees, but it
     * reduces the window of opportunity for multi-user
     * crash problems.  The ONLY real fix is for restart-undo 
     * to guarantee processing in reverse chronological order.
     */
	{
	xct_i iter;
	while ((xd = iter.next()))  {
	    w_assert1(  xd->state() == xct_t::xct_active ||
			xd->state() == xct_t::xct_prepared);
	    DBG(<< "Transaction " << xd->tid() 
		<< " has state " << xd->state()
		<< " has latches " << xd->recovery_latches()
		);
	    if(xd->state() == xct_t::xct_active && xd->recovery_latches()>0) {
		w_assert1(count <  max_xcts);
		smlevel_0::errlog->clog << info_prio 
		    << "Undo SMO xct: " << xd->tid() << flushl;
		worker[count] = new sm_undo_thread_t(xd);
		if (! worker[count])  {
		    W_FATAL(eOUTOFMEMORY);
		}
		W_COERCE(worker[count]->fork());
		++count;
	    }
	}
	/*
	 *  Wait for first set of workers to finish
	 */
	for (int i = 0; i < count; i++) {
	    W_IGNORE( worker[i]->wait() );
	    delete worker[i];
	}
    }
#endif
    xct_i iter;
    count = 0;
    while ((xd = iter.next()))  {
	w_assert1(  xd->state() == xct_t::xct_active ||
		    xd->state() == xct_t::xct_prepared);
	DBG(<< "Transaction " << xd->tid() << " has state " << xd->state());
	if(xd->state() == xct_t::xct_active) {
	    w_assert1(count <  max_xcts);
	    smlevel_0::errlog->clog << info_prio 
		<< "Parallel undo xct: " << xd->tid() << flushl;
	    worker[count] = new sm_undo_thread_t(xd);
	    if (! worker[count])  {
		W_FATAL(eOUTOFMEMORY);
	    }
	    W_COERCE(worker[count]->fork());
	    ++count;
	}
    }

    /*
     *  Wait for workers to finish
     */
    for (int i = 0; i < count; i++) {
	W_IGNORE( worker[i]->wait() );
	delete worker[i];
    }
}



/*********************************************************************
 *
 *  dirty_pages_tab_t::dirty_pages_tab_t(sz)
 *
 *  Construct a dirty page table with hash table of sz slots.
 *
 *********************************************************************/
NORET 
dirty_pages_tab_t::dirty_pages_tab_t(int sz) 
    : tab(sz, offsetof(dp_entry_t, pid), offsetof(dp_entry_t, link)),
    cachedMinRecLSN(lsn_t::null),
    validCachedMinRecLSN(false)
{ 
}

/*********************************************************************
 *
 *  dirty_pages_tab_t::~dirty_pages_tab_t(sz)
 *
 *  Destroy the dirty page table.
 *
 *********************************************************************/
NORET
dirty_pages_tab_t::~dirty_pages_tab_t()
{
    w_hash_i<dp_entry_t, bfpid_t> iter(tab);
    /*
     *  Pop all remaining entries and delete them.
     */
    dp_entry_t* p;
    while ((p = iter.next()))  {
	tab.remove(p);
	delete p;
    }
}

/*********************************************************************
 *
 *  friend operator<< for dirty page table
 *
 *********************************************************************/
NORET
ostream& operator<<(ostream& o, const dirty_pages_tab_t& s)
{
    o << " Dirty page table: " <<endl;

    w_hash_i<dp_entry_t, bfpid_t> iter(s.tab);
    const dp_entry_t* p;
    while ((p = iter.next()))  {
	o << " Page " << p->pid
	<< " lsn " << p->rec_lsn
	<< endl;
    }
    return o;
}

/*********************************************************************
 *
 *  dirty_pages_tab_t::min_rec_lsn()
 *
 *  Compute and return the minimum of the recovery lsn of all
 *  entries in the table.
 *
 *********************************************************************/
lsn_t
dirty_pages_tab_t::min_rec_lsn()
{
    lsn_t l = max_lsn;
    if (validCachedMinRecLSN)  {
	l = cachedMinRecLSN;
    }  else  {
        w_hash_i<dp_entry_t, bfpid_t> iter(tab);
        dp_entry_t* p;
        while ((p = iter.next())) {
	    if (l > p->rec_lsn && p->rec_lsn != null_lsn) {
	        l = p->rec_lsn;
	    }
        }
	cachedMinRecLSN = l;
	validCachedMinRecLSN = true;
    }

    return l;
}


/*********************************************************************
 *
 *  dirty_pages_tab_t::insert(pid, lsn)
 *
 *  Insert an association (pid, lsn) into the table.
 *
 *********************************************************************/
dirty_pages_tab_t&
dirty_pages_tab_t::insert( 
    const lpid_t& 	pid,
    const lsn_t&	lsn)
{
    if (validCachedMinRecLSN && lsn < cachedMinRecLSN)  {
	cachedMinRecLSN = lsn;
    }
    w_assert1(! tab.lookup(pid) );
    dp_entry_t* p = new dp_entry_t(pid, lsn);
    w_assert1(p);
    tab.push(p);
    return *this;
}



/*********************************************************************
 *
 *  dirty_pages_tab_t::look_up(pid, lsn)
 *
 *  Look up pid in the table and return a pointer to its lsn
 *  (so caller may update it in place) in "lsn".
 *
 *********************************************************************/

bool
dirty_pages_tab_t::look_up(const lpid_t& pid, lsn_t** lsn)
{
    if (lsn)
	*lsn = 0;

    dp_entry_t* p = tab.lookup(pid);
    if (p && lsn) *lsn = &p->rec_lsn;
    return (p != 0);
}


/*********************************************************************
 *
 *  dirty_pages_tab_t::remove(pid)
 *
 *  Remove entry of pid from the table.
 *
 *********************************************************************/
dirty_pages_tab_t& 
dirty_pages_tab_t::remove(const lpid_t& pid)
{
    validCachedMinRecLSN = false;
    dp_entry_t* p = tab.remove(pid);
    w_assert1(p);
    delete p;
    return *this;
}



/*********************************************************************
 *
 *  sm_undo_thread_t::sm_undo_thread_t(xd)
 *
 *  Create an sm_undo_thread_t to abort the transaction xd.
 *
 *********************************************************************/
NORET
sm_undo_thread_t::sm_undo_thread_t(
    xct_t*		xd)
    : smthread_t(t_regular), _xd(xd)
{
    rename("undo thread");
}


/*********************************************************************
 *
 *  sm_undo_thread_t::run()
 *
 *  Body of sm_undo_thread_t. Attach to xd, abort and remove xd
 *  from the system.
 *
 *********************************************************************/
void 
sm_undo_thread_t::run()
{
    me()->attach_xct(_xd);
    W_COERCE( _xd->abort() );

    delete _xd;
}
