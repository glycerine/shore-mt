/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: pmap.cc,v 1.2 1997/06/15 03:14:29 solomon Exp $
 */
#define SM_SOURCE
#define PMAP_C
#ifdef __GNUG__
#   pragma implementation
#endif
#include "sm_int_0.h"

ostream	&Pmap::print(ostream &s) const
{
	for (unsigned i = 0; i < sizeof(bits); i++)
		s.form("%02x", bits[i]);
	return s;
}

ostream	&operator<<(ostream &s, const Pmap &pmap)
{
	return pmap.print(s);
}
