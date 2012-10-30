/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: dtid_t.cc,v 1.5 1997/06/15 03:13:03 solomon Exp $
 */

#define SM_SOURCE
#define DTID_T_C

#include "dtid_t.h"

uint4 DTID_T::unique() {
    // TODO: make this unique
    return 0x42424242; // "BBBB"
}
void
DTID_T::update() 
{
    relative ++;
    const long int l = time(0);
    const char *d = ctime(&l);
    w_assert3(strlen(d) == sizeof(date)-1);
    memcpy(date, d,  sizeof(date));
}

NORET 	
DTID_T::DTID_T() 
{
    memset(nulls, '\0', sizeof(nulls));
    location =  unique();
    relative =  0x41414140; // "AAAA"
    update();
}
