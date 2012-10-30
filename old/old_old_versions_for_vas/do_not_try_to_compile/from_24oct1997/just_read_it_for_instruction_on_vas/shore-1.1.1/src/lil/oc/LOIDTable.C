/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// LOIDTable.C
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/LOIDTable.C,v 1.18 1995/09/22 22:26:11 schuh Exp $ */

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma implementation "LOIDTable.h"
#endif

#include "LOIDTable.h"
#include "ObjCache.h"
#include "SH_error.h"
#include <debug.h>

//
// Default initial size of table.  NB: This must be a power of 2, or else
// the hash function won't work right!
//
#define DEF_INITIAL_LT_SIZE		1024

//
// The table will double its size every time the number of entries
// exceeds the size of the table * MAX_AVG_BUCKET_LEN
//
#define MAX_AVG_BUCKET_LEN			1

void
LOIDTable::init(ObjCache *me)
{
    int i;

	myoc = me;
    size = DEF_INITIAL_LT_SIZE;
    nentries = 0;

    table = new OTEntry*[size];
    for(i = 0; i < size; ++i)
	table[i] = 0;
}

w_rc_t
LOIDTable::add(OTEntry *otentry)
{
    OTEntry **new_table, *ote, *next;
    int new_size, hashval, i;

    // if there are too many entries for the table size, double the table size
    if(nentries == size * MAX_AVG_BUCKET_LEN){

	// the new table size is 2x the old table size
	new_size = size * 2;

	// allocate a new table
	new_table = new OTEntry*[new_size];
	if(new_table == 0)
	    return RC(SH_OutOfMemory);

	// initialize all the buckets to 0
	for(i = 0; i < new_size; ++i)
	    new_table[i] = 0;

	// insert everything from the old table into the new table
	for(i = 0; i < size; ++i){
	    for(ote = table[i]; ote != 0; ote = next){
		next = ote->lt_next;
		hashval = hash(ote->volref, new_size);
		// maintain order; this fixes a problem
		// with duplicates because of old type oid's
		// the alternative would be to check add to
		// look for duplicates... which has other
		// problems in this context.
		ote->lt_next= 0;
		if ( new_table[hashval] == 0)
		{
		    new_table[hashval] = ote;
		}
		else // insert at end of chain.
		{
		    OTEntry * end = new_table[hashval];
		    while (end->lt_next != 0) {
			dassert(end != end->lt_next);
			end = end->lt_next;
		    }
		    end->lt_next = ote;
		}
		

		//ote->lt_next = new_table[hashval];
		//new_table[hashval] = ote;
	    }
	}

	// trash the old table
	delete [] table;

	// update bookkeeping
	table = new_table;
	size = new_size;
    }

    // compute the hash value
    hashval = hash(otentry->volref, size);

    // install this entry as the first one in the bucket
    otentry->lt_next = table[hashval];
    table[hashval] = otentry;
    ++nentries;

    return RCOK;
}

OTEntry *
LOIDTable::lookup(int vindex, const VolRef &volref)
{
    int hashval;
    OTEntry *ote;

    // compute the hash value
    hashval = hash(volref, size);

    // look through the bucket
    for(ote = table[hashval];
        ote != 0 && (vindex != ote->volume || volref != ote->volref);
        ote = ote->lt_next);

    return ote;
}

w_rc_t
LOIDTable::lookup_add(int vindex, const VolRef &volref, OTEntry *&otentry)
{
    int hashval;
    OTEntry *ote;

    // compute the hash value
    hashval = hash(volref, size);

    // look through the bucket
    for(ote = table[hashval];
        ote != 0 && (vindex != ote->volume || volref != ote->volref);
        ote = ote->lt_next);

    // if we didn't find it, create it
    if(ote == 0){
	W_DO(myoc->new_ote(0,
			       vindex,
			       volref,
			       (ObjFlags)0,
			       0,
			       NO_LOCK_MODE,
			       ote));
        W_DO(add(ote));
    }

    otentry = ote;
    return RCOK;
}

void
LOIDTable::remove(int vindex, const VolRef &volref)
{
    int hashval;
    OTEntry **ote;

    // compute the hash value
    hashval = hash(volref, size);

    // look through the bucket
    for(ote = table + hashval;
        *ote != 0 && (vindex != (*ote)->volume || volref != (*ote)->volref);
        ote = &((*ote)->lt_next));

    // if we found it, remove it from the table
    if(*ote != 0){
        *ote = (*ote)->lt_next;
        --nentries;
    }
}
        
        
void
LOIDTable::reset(void)
{
    if(table)
	delete [] table;

    table = 0;
    size = 0;
    nentries = 0;
}

LOIDTable::~LOIDTable()
{
	if (table!=0) 
		delete [] table;
}
