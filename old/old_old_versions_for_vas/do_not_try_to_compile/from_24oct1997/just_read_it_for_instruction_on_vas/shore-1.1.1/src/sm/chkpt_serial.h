/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: chkpt_serial.h,v 1.1 1997/05/19 20:27:52 nhall Exp $
 */

#ifndef CHKPT_SERIAL_H
#define CHKPT_SERIAL_H

#ifdef __GNUG__
#pragma interface
#endif

class chkpt_serial_m : public smlevel_0 {
    // mutex for serializing prepares and checkpoints
    static smutex_t		_chkpt_mutex;
public:
    static void			chkpt_mutex_acquire();
    static void			chkpt_mutex_release();
};

#endif /*CHKPT_SERIAL_H*/
