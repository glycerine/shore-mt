/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/kvl_t.cc,v 1.2 1997/06/15 02:36:13 solomon Exp $
 */

#ifdef __GNUC__
#pragma implementation "kvl_t.h"
#endif

#define VEC_T_C
#include <stdlib.h>
#include <memory.h>
#include <iostream.h>
#include "w_base.h"
#include "basics.h"
#include "kvl_t.h"

const cvec_t 	kvl_t::eof("\0255EOF", 4); // the lexical order doesn't really matter;
					// it's the likelihood of a user coming up with
					// this as a legit key that matters
const cvec_t 	kvl_t::bof("\0BOF", 4); // not used


/*********************************************************************
 *
 *  operator<<(ostream, kvl)
 *
 *  Pretty print "kvl" to "ostream".
 *
 *********************************************************************/
ostream& 
operator<<(ostream& o, const kvl_t& kvl)
{
    return o << "k(" << kvl.stid << '.' << kvl.h << '.' << kvl.g << ')';
}

/*********************************************************************
 *
 *  operator>>(istream, kvl)
 *
 *  Read a kvl from istream into "kvl". Format of kvl is a string
 *  of format "k(stid.h.g)".
 *
 *********************************************************************/
istream& 
operator>>(istream& i, kvl_t& kvl)
{
    char c[6];
    i >> c[0] >> c[1] >> kvl.stid >> c[2]
      >> kvl.h >> c[3]
      >> kvl.g >> c[4];
    c[5] = '\0';
    if (i) {
	if (strcmp(c, "k(..)"))  {
	    i.clear(ios::badbit|i.rdstate());  // error
	}
    }
    return i;
}

