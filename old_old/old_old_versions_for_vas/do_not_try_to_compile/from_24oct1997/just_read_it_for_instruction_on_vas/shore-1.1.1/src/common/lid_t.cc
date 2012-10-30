/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/lid_t.cc,v 1.24 1997/06/15 02:36:04 solomon Exp $
 */

#define LID_T_C

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <stream.h>
#include <memory.h>
#include "basics.h"
#include "serial_t.h"
#include "lid_t.h"
#include "tid_t.h"

const lvid_t    lvid_t::null;            
const lid_t     lid_t::null; 

const char* lid_t::key_descr =
#   ifdef BITS64
					"u4u4u4u4";
#   else
					"u4u4u4";
#   endif /*BITS64*/

ostream& operator<<(ostream& o, const lvid_t& lvid)
{
    const u_char* p = (const u_char*) &lvid.high;
    //(char*)inet_ntoa(lvid.net_addr)

	// WARNING: byte-order-dependent
    return o << u_int(p[0]) << '.' << u_int(p[1]) << '.'
	     << u_int(p[2]) << '.' << u_int(p[3]) << ':'
	     <<	lvid.low;
}

istream& operator>>(istream& is, lvid_t& lvid)
{
    is.clear();
    int i;
    char c;
    const parts = sizeof(lvid.high); // should be 4
    int  temp[parts];

    // in case not all fields are represented
    // in the input string
    for (i=0; i<parts; i++) { temp[i] = 0; }

    // read each part of the lvid "address" and stop if
    // it ends early
    for (i=0, c='.'; i<parts && c!='\0'; i++) {
	is >> temp[i];	
	// peek to see the delimiters of lvid pieces.
	if (is.peek() == '.' || is.peek()== ':') {
	    is >> c;
	} else {
	    c = '\0';
	    break;
	}
    }
    if (i==1) {
	// we had a simple integer: put it
	// int the low part
	lvid.low = temp[0];
	temp[0]=0;
    } else if (c == ':') {
	// we had a.b.c.d:l
	// we had a.b.c:l
	// we had a.b:l
	// we had a:l
	if(i!=parts) {
	    is.clear(ios::badbit);
	} else {
	    is >> lvid.low;
	}
    } else  {
	// we had
	// a.b
	// a.b.c
	// a.b.c.d
	is.clear(ios::badbit);
    }

    ((char*)&lvid.high)[0] = temp[0];
    ((char*)&lvid.high)[1] = temp[1];
    ((char*)&lvid.high)[2] = temp[2];
    ((char*)&lvid.high)[3] = temp[3];

    return is;
}

ostream& operator<<(ostream& o, const lid_t& lid)
{
    return o << lid.lvid << '.' << lid.serial;
}

istream& operator>>(istream& i, lid_t& lid)
{
    char c = 0;
    i >> lid.lvid >> c;
    if (c == '.') {
	i >> lid.serial;
    } else {
	// bad lid
	i.clear(ios::badbit|i.rdstate());  // error
	lid.serial = serial_t::null;
    }
    return i;
}

/* 
 * for the benefit of the code that includes
 * the structure-only, non-c++ definitions (rpcgen output)
 * of  lvid_t 
 */
extern "C" bool lvid_t_is_null(const lvid_t &x); 

bool
lvid_t_is_null(const lvid_t &x)
{
	return x == lvid_t::null?true:false;
}
extern "C" bool lrid_t_is_null(const lrid_t &x); 

bool
lrid_t_is_null(const lrid_t &x)
{
	return x == lid_t::null?true:false;
}
