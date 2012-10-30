/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: xct_dependent.h,v 1.1 1997/05/19 20:21:45 nhall Exp $
 */
#ifndef XCT_DEPENDENT_H
#define XCT_DEPENDENT_H


class xct_dependent_t {
public:
    virtual NORET		~xct_dependent_t();

    virtual void		xct_state_changed(
	smlevel_1::xct_state_t	    old_state,
	smlevel_1::xct_state_t	    new_state) = 0;

protected:
    NORET			xct_dependent_t(xct_t* xd); 
private:
    friend class xct_impl;
    w_link_t			_link;
};

#endif /*XCT_DEPENDENT_H*/
