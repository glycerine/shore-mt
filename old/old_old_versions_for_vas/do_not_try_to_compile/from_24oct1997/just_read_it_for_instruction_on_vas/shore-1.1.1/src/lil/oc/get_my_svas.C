
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// get_my_svas.C
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/get_my_svas.C,v 1.6 1995/09/15 03:42:33 zwilling Exp $ */
#include "shore_vas.h"
#include "OCRef.h"
#include "SH_error.h"

w_rc_t get_my_svas(svas_base ** vpt, bool & in_server)
{
	in_server = false;
	return new_svas(vpt);
}

extern class ObjCache * cur_oc;
// return current oc ptr whether or not it is null.  This is used
// for initialization only.
class ObjCache *
check_my_oc() { return cur_oc; }
// in the client context, exactly one oc.

// this is the normal access method for the oc; if the pointer is not
// set, handle the error.
extern class ObjCache * get_my_oc()
{
	if (cur_oc==0) {
		shrc tmp_rc = RC(SH_NotInitialized);
		OCRef::call_error_handler(tmp_rc,__FILE__,__LINE__,true);
	}
	return cur_oc;
}


void set_my_oc(ObjCache * new_oc) {  cur_oc = new_oc; }
// in the client context, exactly one oc.
