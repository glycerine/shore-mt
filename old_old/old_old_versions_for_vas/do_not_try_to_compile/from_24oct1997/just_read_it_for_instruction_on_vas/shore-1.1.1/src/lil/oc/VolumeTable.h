/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// VolumeTable.h
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/VolumeTable.h,v 1.12 1997/01/24 20:14:11 solomon Exp $ */

#ifndef _VOLUMETABLE_H_
#define _VOLUMETABLE_H_

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma interface
#endif

#include "OCTypes.h"

#ifndef INIT_VT_SIZE
#define INIT_VT_SIZE	8
#endif

struct VTEntry
{
    VolId volid;
    int ncreates;
};

class VolumeTable
{
 public:

    void init()
    { table = 0; size = 0; next = 0; }

    void reset();

    int nvolumes(void)
    { return next; }

    int add(const VolId &volid)
    { return lookup(volid, true); }
 
    int lookup(const VolId &volid, bool create = false);

    const VolId &get(int i)
    { return table[i].volid; }

    const VolId &operator[](int i)
    { return table[i].volid; }

    int get_ncreates(int i)
    { return table[i].ncreates; }

    void inc_ncreates(int i, int count = 1)
    { table[i].ncreates += count; }

    void reset_ncreates(int i)
    { table[i].ncreates = 0; }

    static const int NoEntry;

 private:

    VTEntry *table;
    int size;
    int next;
};

#endif
