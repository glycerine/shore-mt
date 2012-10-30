/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: latch.h,v 1.22 1997/05/19 19:41:03 nhall Exp $
 */
#ifndef LATCH_H
#define LATCH_H

#ifndef STHREAD_H
#include <sthread.h>
#endif

#ifdef __GNUG__
#pragma interface
#endif

enum latch_mode_t { LATCH_NL = 0, LATCH_SH = 1, LATCH_EX = 2 };

/*

   Class latch_t provides shared and exclusive latches.  The code is
   optimized for the case of a single thread acquiring an exclusive
   latch (especially one that is not held).  Up to 4 (max_sh) threads
   can hold a shared latch at one time.  If more try, they are made to
   wait.
*/

/*
   A latch may be acquire()d multiple times by a single thread.  The
   mode of subsequent acquire()s must be at or above the level of the
   currently held latch.  Each of these individual locks must be
   released.

   The upgrade_if_not_block() method allows a thread to convert a
   shared latch into an exlusive latch.  The number of times the
   thread holds the latch is NOT changed.

   The following two sequences illustrate the difference.
   acquire(share)		acquire(share)
     acquire(exclusive)		  upgrade_if_not_block()
     // two latches		  // one latch
    release(exclusive)		release()
   release(share)
*/   

class latch_t : public sthread_named_base_t {

public:
    NORET			latch_t(const char* const desc = 0);
    NORET			~latch_t()	{};
#ifdef DEBUG
    ostream			&print(ostream &) const;
    friend ostream& operator<<(ostream&, const latch_t& l);
#endif

    inline const void *		id() const { return &_waiters; } // id used for resource tracing
    inline void 		setname(const char *const desc);
    w_rc_t			acquire(
	latch_mode_t 		    m, 
	int 			    timeout = sthread_base_t::WAIT_FOREVER);
    w_rc_t			upgrade_if_not_block(
	bool& 			    would_block);
    void   			release();
    bool 			is_locked() const;
    bool 			is_hot() const;
    int    			lock_cnt() const;
    int				num_holders() const;
    int				held_by(const sthread_t* t) const;
    bool 			is_mine() const;
    latch_mode_t		mode() const;

    const sthread_t * 		holder() const;

    enum { max_sh = 4 }; // max threads that can hold a share latch

    static const char* const    latch_mode_str[3];

private:
    smutex_t			_mutex;
    latch_mode_t		_mode;		    // highest mode held

    // slots for each holding thread
    uint2_t			_cnt[max_sh];	    // number of times held
    sthread_t*			_holder[max_sh];    // thread holding the latch

    scond_t			_waiters;
    
    // CONSTRAINTS:
    // Slots must be filled in order.  If slot i is empty then
    // the remaining slots must be empty.
    // This makes it's easy to see if the latch is held and
    // speeds counting the number of holders.

    uint2_t		 	_held_by(const sthread_t*, int& num_holders);
    uint2_t 			_first_free_slot();
    void			_fill_slot(uint2_t slot); // fill hole

    // disabled
    NORET			latch_t(const latch_t&);
    latch_t&   			operator=(const latch_t&);
};

inline void
latch_t::setname(const char* const desc)
{
    rename(desc?"l:":0, desc);
    _mutex.rename(desc?"l:m:":0, desc);
    _waiters.rename(desc?"l:c:":0, desc);
}

inline bool
latch_t::is_locked() const
{
    return _cnt[0] != 0; 
}

inline bool
latch_t::is_hot() const
{
    return _waiters.is_hot(); 
}

/* XXX this should be protected with a mutex.  It will
   work with non-preepmtive threads,  but ...  FIX IT. */

inline bool 
latch_t::is_mine() const
{
    return _mode == LATCH_EX && _holder[0] == sthread_t::me();
}

inline const sthread_t * 
latch_t::holder() const
{
    return _holder[0];
}

inline latch_mode_t
latch_t::mode() const
{
    return _mode;
}
#endif /*LATCH_H*/
