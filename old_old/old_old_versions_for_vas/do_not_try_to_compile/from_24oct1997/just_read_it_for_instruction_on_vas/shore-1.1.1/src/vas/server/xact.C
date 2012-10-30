/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/xact.C,v 1.4 1997/06/13 21:43:35 solomon Exp $
 */
#include <copyright.h>

// fake an extension of the sm interface:
#define SM_SOURCE
#define TEST_C
#include <sys/stat.h>
#include "w.h"
#include "option.h"
#include "opt_error_def.h"
#include "sm_int_4.h"

#include "sysp.h"
#include "vas_internal.h"
#include "vaserr.h"
#include "smcalls.h"

scan_index_i *
svas_server::check_index_cookie(
	const Cookie &cookie
)
{
	scan_index_i	*i = (scan_index_i *)cookie;
	xct_t			*x = this->_xact;

	//if(x->find_dependent((xct_dependent_t *)i)) {
		dassert(i->xid() == x->tid());
		return i;
	//}
	//return (scan_index_i *)0;
}

scan_file_i *
svas_server::check_file_cookie(
	const Cookie &cookie
)
{
	scan_file_i	*i = (scan_file_i *)cookie;
	xct_t			*x = this->_xact;

	//if(x->find_dependent((xct_dependent_t *)i)) {
		dassert(i->xid() == x->tid());
		return i;
	//}
	//return (scan_file_i *)0;
}

long
svas_server::timeout()const
{
	xct_t			*x = this->_xact;
	//return x->timeout_c();
	return 0; // FIXME
}
void
svas_server::set_timeout(long t)const
{
	xct_t			*x = this->_xact;
	//x->set_timeout(t); // FIXME
}
