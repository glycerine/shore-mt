/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: prologue.h,v 1.32 1997/05/19 19:47:46 nhall Exp $
 */
#ifndef PROLOGUE_H
#define PROLOGUE_H

#ifdef __GNUG__
#pragma interface
#endif

#define SM_PROLOGUE_RC(func_name, is_in_xct, pin_cnt_change)	\
	FUNC(func_name); 					\
	prologue_rc_t prologue(prologue_rc_t::is_in_xct, (pin_cnt_change)); \
	if (prologue.error_occurred()) return prologue.rc();

#define INIT_SCAN_PROLOGUE_RC(func_name, pin_cnt_change)		\
	FUNC(func_name);					\
	prologue_rc_t prologue(prologue_rc_t::in_xct, (pin_cnt_change)); \
	if (prologue.error_occurred()) { 			\
	    _error_occurred = prologue.rc();			\
	    return; 						\
	}

 

class prologue_rc_t {
public:
    enum xct_state_t {
	in_xct,  	// must be active and not prepared
	commitable_xct, // must be prepared if external, else must be active or prepared
	not_in_xct,     // may not have tx, regardless of state
	can_be_in_xct,  // in or not -- no test for active or prepared
	abortable_xct   // active or prepared 
    };
 
    prologue_rc_t(xct_state_t is_in_xct, int pin_cnt_change);
    ~prologue_rc_t();
    void no_longer_in_xct();
    bool error_occurred() const {return _rc.is_error();}
    rc_t   rc() const {return _rc;}

private:
    xct_state_t  _xct_state;
    int     _pin_cnt_change;
    rc_t    _rc;
    xct_log_switch_t*    _toggle;
};

/*
 * Install the code in sm.c
 */
#ifdef SM_C

prologue_rc_t::prologue_rc_t(xct_state_t is_in_xct, int pin_cnt_change) :
		_xct_state(is_in_xct), _pin_cnt_change(pin_cnt_change),
		_toggle(0)
{
    w_assert3(!me()->is_in_sm());
    xct_t *x = xct();

    switch (_xct_state) {
    case in_xct:
	if ( (!x) || (x->state() != smlevel_1::xct_active)) {
	    _rc = rc_t(__FILE__, __LINE__, 
		    (x && x->state() == smlevel_1::xct_prepared)?
		    smlevel_0::eISPREPARED :
		    smlevel_0::eNOTRANS
		);
	    _xct_state = not_in_xct; // otherwise destructor will fail
	} 
	break;

    case commitable_xct: {
	// called from commit and chain
	// If this tx is participating in an external 2pc,
	// it MUST be prepared before commit.  If it's
	// an internally  "distributed"  transaction, it
	// must be prepared or active. (since this
	// prologue is called only on the "client" side.)
	int	error = 0;
	if ( ! x  ) {
	    error = smlevel_0::eNOTRANS;
	} else if (x->is_extern2pc() && (x->state() != smlevel_1::xct_prepared) ) {
	    error = smlevel_0::eNOTPREPARED;
	} else if( (x->state() != smlevel_1::xct_active) &&
		(x->state() != smlevel_1::xct_prepared) ) {
	    error = smlevel_0::eNOTRANS;
	}
	if(error) {
	    _rc = rc_t(__FILE__, __LINE__, error);
	    _xct_state = not_in_xct; // otherwise destructor will fail
	}

	break;
    }
    case abortable_xct:
	// do not special-case external2pc transactions -- they
	// can abort any time, since this is presumed-abort protocol
	if (! x || (x->state() != smlevel_1::xct_active && 
		x->state() != smlevel_1::xct_prepared)) {
	    _rc = rc_t(__FILE__, __LINE__, smlevel_0::eNOTRANS);
	    _xct_state = not_in_xct; // otherwise destructor will fail
	}
	break;

    case not_in_xct:
	if (x) _rc = rc_t(__FILE__, __LINE__, smlevel_0::eINTRANS);
	break;

    case can_be_in_xct:
	// do nothing
	break;

    default:
	W_FATAL(smlevel_0::eINTERNAL);
	break;
    }
#ifdef DEBUG
    me()->mark_pin_count();
    me()->in_sm(true);
#endif /* DEBUG */

    if(_xct_state != not_in_xct) {
	_toggle = new xct_log_switch_t(smlevel_0::ON);
    }
}


inline
prologue_rc_t::~prologue_rc_t()
{
    if (_xct_state == in_xct || _xct_state == abortable_xct) {
	xct_t& x = *xct();
	x.flush_logbuf(); // NEEDED?
    }
#ifdef DEBUG
    me()->check_pin_count(_pin_cnt_change);
    me()->in_sm(false);
#endif /* DEBUG */
    if(_toggle) { delete _toggle; }
}

inline void
prologue_rc_t::no_longer_in_xct()
{
    _xct_state = not_in_xct;
}

#endif /* SM_C */

#endif /* PROLOGUE_H */

