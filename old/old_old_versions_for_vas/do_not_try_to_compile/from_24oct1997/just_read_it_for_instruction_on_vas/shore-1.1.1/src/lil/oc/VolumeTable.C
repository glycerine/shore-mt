/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// VolumeTable.C
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/VolumeTable.C,v 1.11 1995/07/14 23:05:48 nhall Exp $ */

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma implementation "VolumeTable.h"
#endif

#include "VolumeTable.h"

const int VolumeTable::NoEntry = -1;

void
VolumeTable::reset()
{
    if(table)
	delete [] table;

    table = 0;
    size = 0;
    next = 0;
}

int
VolumeTable::lookup(const VolId &volid, bool create)
{
    int i;

    // see if we already have this volid in the table
    for(i = 0; i < next && volid != table[i].volid; ++i);

    // if so then we're done
    if(i < next)
	return i;

    // if we're not supposed to create a new entry, give up
    if(!create)
	return NoEntry;

    // if there's no space left in the table...
    if(next == size){

	VTEntry *tmp;
	int newsize;

	// get the new table size
	if(size == 0)
	    newsize = INIT_VT_SIZE;
	else
	    newsize = size * 2;

	// allocate the new table and copy the old to the new
	tmp = new VTEntry[newsize];
	for(i = 0; i < next; ++i)
	    tmp[i] = table[i];

	// delete the old table
	if(table != 0)
	    delete [] table;

	// install the new one
	table = tmp;
	size = newsize;
    }

    // put the volid in the next free slot
    i = next;
    ++next;
    table[i].volid = volid;
    table[i].ncreates = 0;

    return i;
}
