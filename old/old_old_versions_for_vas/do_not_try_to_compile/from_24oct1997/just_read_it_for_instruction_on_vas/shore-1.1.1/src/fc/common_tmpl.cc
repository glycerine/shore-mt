/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: common_tmpl.cc,v 1.3 1997/06/15 02:03:08 solomon Exp $
 */

/*
 * Instantiations of commonly used fc templates
 */
#ifdef __GNUG__
#include "w.h"
#include "w_minmax.h"

template class w_auto_delete_array_t<char>;
#ifndef GNUG_BUG_14
template int max<int>(int, int);
template  u_long max<u_long>(u_long,  u_long);
template  int    max<int>( int,  int);
template  u_int  max<u_int>( u_int,  u_int);
template  u_short max<u_short>( u_short,  u_short);
#endif /* ! GNUG_BUG_14 */
#endif
