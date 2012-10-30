/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: restart.h,v 1.17 1997/05/19 19:47:49 nhall Exp $
 */
#ifndef RESTART_H
#define RESTART_H

class dirty_pages_tab_t;

#ifdef __GNUG__
#pragma interface
#endif

#ifndef BF_S_H
#include <bf_s.h>
#endif

#ifndef RESTART_S_H
#include <restart_s.h>
#endif

class restart_m : public smlevel_1 {
public:
    NORET			restart_m()	{};
    NORET			~restart_m()	{};

    static void 		recover(lsn_t master);

private:
    static void 		analysis_pass(
	lsn_t 			    master,
	dirty_pages_tab_t& 	    ptab, 
	lsn_t& 			    redo_lsn,
	bool&			    found_xct_freeing_space
	);

    static void 		redo_pass(
	lsn_t 			    redo_lsn, 
	const lsn_t 	            &highest,  /* for debugging */
	dirty_pages_tab_t& 	    ptab);

    static void 		undo_pass();

private:
    // keep track of tid from log record that we're redoing
    // for a horrid space-recovery handling hack
    static tid_t		_redo_tid;
public:
    tid_t			*redo_tid() { return &_redo_tid; }

};


class AutoTurnOffLogging {
    public:
	AutoTurnOffLogging()
	{
	    w_assert1(smlevel_0::logging_enabled == true);
	    smlevel_0::logging_enabled = false;
	};

	~AutoTurnOffLogging()
	{
	    w_assert1(smlevel_0::logging_enabled == false);
	    smlevel_0::logging_enabled = true;
	};
    private:
	AutoTurnOffLogging& operator=(const AutoTurnOffLogging&);
	AutoTurnOffLogging(const AutoTurnOffLogging&);
};

#endif /*RESTART_H*/
