/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm_escalation.h,v 1.2 1997/05/19 19:48:06 nhall Exp $
 */
#ifndef SM_ESCALATION_H
#define SM_ESCALATION_H

#ifdef __GNUG__
#pragma interface
#endif

class sm_escalation_t {
public:
    NORET		sm_escalation_t(
			    int4 p = dontEscalate,
			    int4 s = dontEscalate, 
			    int4 v = dontEscalate);
    NORET		~sm_escalation_t(); 
private:
    int4 _p;
    int4 _s;
    int4 _v; // save old values
    // disable
    sm_escalation_t(const sm_escalation_t&);
    operator=(const sm_escalation_t&);
};

#endif SM_ESCALATION_H
