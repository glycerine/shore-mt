/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// LOIDTable.h
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/LOIDTable.h,v 1.16 1995/09/22 22:26:13 schuh Exp $ */

#ifndef _LOIDTABLE_H_
#define _LOIDTABLE_H_

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma interface
#endif

#include "OCTypes.h"
#include "OTEntry.h"

//
// The LOIDTable class.
//
class LOIDTable
{
 public:

    // Initialize the table
	LOIDTable() { table=0; }
    void init(ObjCache *);

    // Empty out the table
    void reset(void);

    // Adds a new entry to the table.
    w_rc_t add(OTEntry *ote);

    // These functions return the ote corresponding to the given loid
    // if there is one.  The first form returns 0 if there is no
    // appropriate entry in the table.  The second form creates an
    // entry and returns it.
    OTEntry *lookup(int vindex, const VolRef &volref);
    w_rc_t lookup_add(int vindex, const VolRef &volref, OTEntry *&ote);

    // Removes the entry corresponding to `volref' from the table.  If
    // there is no corresponding entry in the table, no action is taken.
    void remove(int vindex, const VolRef &volref);
	~LOIDTable();

 private:

    // Hash function.  NB: Size must be a power of 2 for this to work
    // correctly.
    static inline int hash(const VolRef &volref, int size)
    {
	// return ((volref.data._low >> 2) & (size - 1));	// FIX
	// shift by 2 guarantees that adjacent serials collide.
	return ((volref.data._low >> 1) & (size - 1));	// FIX
    }

    // The table: an array of pointers to buckets, which are linked
    // lists of OTEntries.
    OTEntry **table;

    // The size of the table (i.e., the length of the array).
    int size;

    // The number of entries currently in the table.
    int nentries;

	// pointer to ObjCache class
	ObjCache *myoc;

};

#endif /* _LOIDTABLE_H_ */
