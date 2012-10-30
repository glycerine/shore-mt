/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: chkpt.h,v 1.16 1997/05/19 19:46:57 nhall Exp $
 */
#ifndef CHKPT_H
#define CHKPT_H

#ifdef __GNUG__
#pragma interface
#endif

class chkpt_thread_t;

/*********************************************************************
 *
 *  class chkpt_m
 *
 *  Checkpoint Manager. User calls spawn_chkpt_thread() to fork
 *  a background thread to take checkpoint every now and then. 
 *  User calls take() to take a checkpoint immediately.
 *
 *  User calls wakeup_and_take() to wake up the checkpoint thread to checkpoint soon.
 *  (this is only for testing concurrent checkpoint/other-action.)
 *
 *********************************************************************/
class chkpt_m : public smlevel_1 {
public:
    NORET			chkpt_m();
    NORET			~chkpt_m();

    void 			take();
    void 			wakeup_and_take();
    void			spawn_chkpt_thread();
    void			retire_chkpt_thread();

private:
    chkpt_thread_t*		_chkpt_thread;


public:
    // These functions are for the use of chkpt -- to serialize
    // logging of chkpt and prepares 
};


#endif /*CHKPT_H*/
