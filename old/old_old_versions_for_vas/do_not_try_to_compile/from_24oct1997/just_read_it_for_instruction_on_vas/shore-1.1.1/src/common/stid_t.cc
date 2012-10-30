/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/stid_t.cc,v 1.3 1997/06/15 02:36:14 solomon Exp $
 */

#ifdef __GNUC__
#pragma implementation "stid_t.h"
#endif

#define VEC_T_C
#include <stdlib.h>
#include <memory.h>
#include <iostream.h>
#include "w_base.h"
#include "w_minmax.h"
#include "basics.h"
#include "dual_assert.h"
#include "stid_t.h"

const stid_t stid_t::null;

ostream& operator<<(ostream& o, const stid_t& stid)
{
    return o << "s(" << stid.vol << '.' << stid.store << ')';
}

istream& operator>>(istream& i, stid_t& stid)
{
    char c[5];
    memset(c, '\0', sizeof(c));
    i >> c[0] >> c[1] >> stid.vol >> c[2] >> stid.store >> c[3];
    c[4] = '\0';
    if (i) {
        if (strcmp(c, "s(.)")) {
	    i.clear(ios::badbit|i.rdstate());  // error
	}
    }
    return i;
}
