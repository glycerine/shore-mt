/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: chkpt.cc,v 1.59 1997/06/15 03:12:52 solomon Exp $
 */

#define SM_SOURCE
#define CHKPT_C

#ifdef __GNUG__
#   pragma implementation
#endif

#include "sm_int_1.h"
#include "chkpt_serial.h"
#include "chkpt.h"
#include "logdef.i"

#ifdef __GNUG__
template class w_auto_delete_t<logrec_t>;
template class w_auto_delete_array_t<lsn_t>;
template class w_auto_delete_array_t<tid_t>;
template class w_auto_delete_array_t<smlevel_1::xct_state_t>;
#endif

#if !defined(DBGTHRD)
#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)
#endif



/*********************************************************************
 *
 *  class chkpt_thread_t
 *
 *  Checkpoint thread. 
 *
 *********************************************************************/
class chkpt_thread_t : public smthread_t  {
public:
    NORET			chkpt_thread_t();
    NORET			~chkpt_thread_t();

    virtual void		run();
    void			retire();
    void			awaken();
private:
    bool			_retire;
    sevsem_t			_wakeup;

    // disabled
    NORET			chkpt_thread_t(const chkpt_thread_t&);
    chkpt_thread_t&		operator=(const chkpt_thread_t&);
};



/*********************************************************************
 *
 *  chkpt_m::chkpt_m()
 *
 *  Constructor for Checkpoint Manager. 
 *
 *********************************************************************/
NORET
chkpt_m::chkpt_m()
    : _chkpt_thread(0)
{
}


/*********************************************************************
 * 
 *  chkpt_m::~chkpt_m()
 *
 *  Destructor. If a thread is spawned, tell it to exit.
 *
 *********************************************************************/
NORET
chkpt_m::~chkpt_m()
{
    if (_chkpt_thread) {
	retire_chkpt_thread();
    }
}
    



/*********************************************************************
 *
 *  chkpt_m::spawn_chkpt_thread()
 *
 *  Fork the checkpoint thread.
 *
 *********************************************************************/
void
chkpt_m::spawn_chkpt_thread()
{
    w_assert1(_chkpt_thread == 0);
    if (log)  {
        _chkpt_thread = new chkpt_thread_t;
        if (! _chkpt_thread)  W_FATAL(eOUTOFMEMORY);
	W_COERCE(_chkpt_thread->fork());
    }
}
    


/*********************************************************************
 * 
 *  chkpt_m::retire_chkpt_thread()
 *
 *  Kill the checkpoint thread.
 *
 *********************************************************************/
void
chkpt_m::retire_chkpt_thread()
{
    if (log)  {
        w_assert1(_chkpt_thread);
        _chkpt_thread->retire();
        W_COERCE( _chkpt_thread->wait() ); // wait for it to end
        delete _chkpt_thread;
        _chkpt_thread = 0;
    }
}

void
chkpt_m::wakeup_and_take()
{
    if(log && _chkpt_thread) {
        _chkpt_thread->awaken();
    }
}

/*********************************************************************
 *
 *  chkpt_m::take()
 *
 *  Take a checkpoint. A Checkpoint consists of:
 *	1. Checkpoint Begin Log
 *	2. Checkpoint Device Table Log(s)
 *		-- all mounted devices
 *	3. Checkpoint Buffer Table Log(s)
 *		-- dirty page entries in bf and their recovery lsn
 *	4. Checkpoint Transaction Table Log(s)
 *		-- active transactions and their first lsn
 *	5. Checkpoint Prepared Transactions (optional)
 *		-- prepared transactions and their locks
 *	6. Checkpoint End Log
 *
 *********************************************************************/
void chkpt_m::take()
{
    FUNC(chkpt_m::take);
    if (! log)   {
	/*
	 *  recovery facilities disabled ... do nothing
	 */
	return;
    }
    smlevel_0::stats.log_chkpt_cnt++;
    
    /*
     *  Allocate a buffer for storing log records
     */
    logrec_t* logrec = new logrec_t;
    w_assert1(logrec);
    w_auto_delete_t<logrec_t> auto_del(logrec);


    /*
     * Acquire the mutex to serialize prepares and
     * checkpoints. 
     *
     * NB: EVERTHING BETWEEN HERE AND RELEASING THE MUTEX
     * MUST BE W_COERCE (not W_DO).
     */
    chkpt_serial_m::chkpt_mutex_acquire();

    /*
     *  Write a Checkpoint Begin Log and record its lsn in master
     */
    lsn_t master;
    new (logrec) chkpt_begin_log(io->GetLastMountLSN());
    W_COERCE( log->insert(*logrec, &master) );

    /*
     *  Checkpoint the dev mount table
     */
    {
	/*
	 *  Define chunk with max_vols rather than
	 *  chkpt_dev_tab_t::max since chkpt_dev_tab_t::max can
	 *  be too large for the devs[] array below
	 */
	const chunk = max_vols/2;
	w_assert3(chunk <= chkpt_dev_tab_t::max);
	int dev_cnt = io->num_vols();

	// make sure devs is not too big for stack
	typedef	char	devs_type[chunk][max_devname+1];
	w_assert1(sizeof(devs_type) < (size_t)smthread_t::me()->stack_size()/4);

	devs_type	devs;
	vid_t		vids[chunk];

	for (int i = 0; i < dev_cnt; i += chunk)  {
	    
	    int ret;
	    W_COERCE( io->get_vols(i, MIN(dev_cnt - i, chunk),
				      devs, vids, ret));
	    if (ret)  { 
		/*
		 *  Write a Checkpoint Device Table Log
		 */
		new (logrec) chkpt_dev_tab_log(ret, devs, vids);
		W_COERCE( log->insert(*logrec, 0) );
	    }
	}
    }

    /*
     *  Checkpoint the buffer pool dirty page table, and record
     *  minimum of the recovery lsn of all dirty pages.
     */
    lsn_t min_rec_lsn = lsn_t::max;
    {
	int 	bfsz = bf->npages();
	const 	chunk = chkpt_bf_tab_t::max;

	lpid_t* pid = new lpid_t[chunk];
	w_auto_delete_array_t<lpid_t> auto_del_pid(pid);
	lsn_t*	rec_lsn = new lsn_t[chunk];
	w_auto_delete_array_t<lsn_t> auto_del_rec(rec_lsn);
	w_assert1(pid && rec_lsn);

	for (int i = 0; i < bfsz; i += chunk)  {
	    /*
	     *  Loop over all buffer pages
	     */
	    int count;
	    W_COERCE( bf->get_rec_lsn(i, MIN(bfsz - i, chunk),
				      pid, rec_lsn, count));
	    if (count)  { 
		lpid_t oldest_page;
		for (int j = 0; j < count; j++)  {
		    if (min_rec_lsn > rec_lsn[j]) {
			min_rec_lsn = rec_lsn[j];
			oldest_page = pid[j];
		    }
		}
		DBG( << "Oldest page = " << oldest_page);

		/*
		 *  Write a Buffer Table Log
		 */
		new (logrec) chkpt_bf_tab_log(count, pid, rec_lsn);
		W_COERCE( log->insert(*logrec, 0) );
	    }
	}
    }

    /*
     *  Checkpoint the transaction table, and record
     *  minimum of first_lsn of all transactions.
     */
    lsn_t min_xct_lsn = lsn_t::max;
    {
	const 		chunk = chkpt_xct_tab_t::max;
	tid_t		youngest = xct_t::youngest_tid();
	tid_t* 		tid = new tid_t[chunk];
	w_auto_delete_array_t<tid_t> auto_del_tid(tid);
	xct_state_t*	state = new xct_state_t[chunk];
	w_auto_delete_array_t<xct_state_t> auto_del_state(state);
	lsn_t*		last_lsn = new lsn_t[chunk];
	w_auto_delete_array_t<lsn_t> auto_del_last(last_lsn);
	lsn_t*		undo_nxt = new lsn_t[chunk];
	w_auto_delete_array_t<lsn_t> auto_del_undo(undo_nxt);

	xct_i x;
	xct_t* xd = 0;
	do {
	    int i = 0;
	    while (i < chunk && (xd = x.next()))  {
		/*
		 *  Loop over all transactions and record only
		 *  xcts that dirtied something.
		 *  Skip those that have ended but not yet
		 *  been destroyed.
		 */
		if( xd->state() == xct_t::xct_ended) {
		   continue;
		}
		if (xd->first_lsn())  {
		    tid[i] = xd->tid();
		    state[i] = xd->state();
		    //
		    if (state[i] == xct_t::xct_aborting) 
			state[i] = xct_t::xct_active;
		    //

		    if (state[i] == xct_t::xct_prepared)  {
			DBG(<< tid[i] <<" is prepared -- logging as active");
			state[i] = xct_t::xct_active;
		    }
		    //  ^^^^^^^^^^^^^^^^^^^^^^^^^^^
		    // don't worry - it
		    // will be prepared in the next section.
		    // this just makes recovery debug-checking
		    // a little easier
		    /////////////////////////////////////////

		    last_lsn[i] = xd->last_lsn();
		    undo_nxt[i] = xd->undo_nxt();
		    
		    if (min_xct_lsn > xd->first_lsn())
			min_xct_lsn = xd->first_lsn();

		    i++;
		}
	    }

	    /*
	    // We *always* have to write this record, because we have
	    // to record the youngest xct!!!! NEH
	    // if (i)  
	    */
	    {
		/*
		 *  Write a Transaction Table Log
		 */
		new (logrec) chkpt_xct_tab_log(youngest, i, tid, state,
					       last_lsn, undo_nxt);
		W_COERCE( log->insert(*logrec, 0) );
	    }
	} while (xd);
    }

    /*
     *  Checkpoint the prepared transactions 
     */
    {
	DBG(<< "checkpoint prepared tx");
	xct_i x;
	xct_t* xd = 0;
	while( (xd = x.next()) )  {
	    DBG(<< xd->tid() << " has state " << xd->state());
	    if (xd->state() == xct_t::xct_prepared)  {
		DBG(<< xd->tid() << "LOG PREPARED ");
		//
		// To write tx-related log records for a tx,
		// logstub.i functions expect the tx to be attached.
		// NB: we might be playing with fire by attaching
		// two threads to the tx.  NOT PREEMPTIVE-SAFE!!!!
		//
		me()->attach_xct(xd);
		W_COERCE(xd->log_prepared(true));
		me()->detach_xct(xd);
	    }
	}
    }

    /*
     *  Make sure that min_rec_lsn and min_xct_lsn are valid
     */
    if (min_rec_lsn == lsn_t::max) min_rec_lsn = master;
    if (min_xct_lsn == lsn_t::max) min_xct_lsn = master;

    /*
     *  Write the Checkpoint End Log
     */
    new (logrec) chkpt_end_log (master, min_rec_lsn);
    W_COERCE( log->insert(*logrec, 0) );

    /*
     *  Sync the log
     */
    W_COERCE( log->flush_all() );


    /*
     *  Make the new master lsn durable
     */
    log->set_master(master, min_rec_lsn, min_xct_lsn);

    /*
     *  Scavenge some log
     */
    W_COERCE( log->scavenge(min_rec_lsn, min_xct_lsn) );

    chkpt_serial_m::chkpt_mutex_release();
}


/*********************************************************************
 *
 *  chkpt_thread_t::chkpt_thread_t()
 *
 *  Construct a Checkpoint Thread. Priority level is t_time_critical
 *  so that checkpoints are done as fast as it could be done. Most of
 *  the time, however, the checkpoint thread is blocked waiting for
 *  a go-ahead signal to take a checkpoint.
 *
 *********************************************************************/
chkpt_thread_t::chkpt_thread_t()
    : smthread_t(t_time_critical), _retire(false)
{
    rename("chkpt_thread");			// for debugging
    _wakeup.setname("chkptwk");	// for debugging
}


/*********************************************************************
 *
 *  chkpt_thread_t::~chkpt_thread_t()
 *
 *  Destroy a checkpoint thread.
 *
 *********************************************************************/
chkpt_thread_t::~chkpt_thread_t()
{
    /* empty */
}



/*********************************************************************
 *
 *  chkpt_thread_t::run()
 *
 *  Body of checkpoint thread. Repeatedly:
 *	1. wait for signal to activate
 *	2. if retire intention registered, then quit
 *	3. write all buffer pages dirtied before the n-1 checkpoint
 *	4. if toggle off then take a checkpoint
 *	5. flip the toggle
 *	6. goto 1
 *
 *  Essentially, the thread will take one checkpoint for every two 
 *  wakeups.
 *
 *********************************************************************/
void
chkpt_thread_t::run()
{
    bool toggle = true;
    while (! _retire)  {
	W_IGNORE( 
	    smlevel_0::log->wait(smlevel_0::chkpt_displacement/2, 
				 _wakeup) );
	W_IGNORE( _wakeup.reset() );
	if (! _retire)  {
	    if (smlevel_0::log->old_master_lsn())  {
		W_IGNORE(
		    smlevel_0::bf->force_until_lsn( 
			smlevel_0::log->old_master_lsn()) );
	    }
	    if (toggle) {
		smlevel_0::bf->activate_background_flushing();
	    } else  {
		smlevel_1::chkpt->take();
	    }
	}
	toggle = !toggle;
    }
}



/*********************************************************************
 *
 *  chkpt_thread_t::retire()
 *
 *  Register an intention to retire and activate the thread.
 *  The thread will exit when it wakes up and checks the retire
 *  flag.
 *
 *********************************************************************/
void
chkpt_thread_t::retire()
{
    _retire = true;
    W_IGNORE( _wakeup.post() );
}

void
chkpt_thread_t::awaken()
{
    W_IGNORE( _wakeup.post() );
}
