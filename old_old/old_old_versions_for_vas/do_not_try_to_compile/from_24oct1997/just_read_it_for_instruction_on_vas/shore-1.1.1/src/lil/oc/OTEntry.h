/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// OTEntry.h
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/OTEntry.h,v 1.12 1995/10/27 15:30:39 schuh Exp $ */

#ifndef _OTENTRY_H_
#define _OTENTRY_H_

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma interface
#endif

#include "OCTypes.h"

// Flags:
//
// OF_New: The object is not yet known to the vas.  If the object has
// no serial, then OF_New should be set.  The reverse is not
// necessarily true: a new object can acquire a serial number without
// being known to the vas.
//
// OF_Dirty: The object has been modified, and the new contents of the
// object have not been written back to the vas.  Set in
// make_writable().  Cleared in writeback() or remove_destroyed().
//
// OF_OnPoolList: This flag is set only for pool objects.  It
// indicates that the pool is on a list of pools in which objects have
// been created during the current transaction.
// OF_Unswizzled: This flag is set if an object has been unswizzled
// for a disk writeback but is still cached in memory.  reclaim_object
// needs to be called to put it back into memory form

typedef uint1 ObjFlags_t;
enum ObjFlags
{
    OF_New	  = 0x1,
    OF_Dirty	  = 0x2,
    OF_Secondary  = 0x4,
    OF_OnPoolList = 0x8,
	OF_UnSwizzled = 0x10, // new flag: we've done writeback so obj. is 
	OF_Transient  = 0x20, // transient object: don't touch server.
};

////////////////////////////////////////////////////////////////////////
//
//    The structure of an object table entry.
//
//    Do not rearrange the order of these fields: volume, lockmode,
//    and flags should be packed together without any padding.
//
////////////////////////////////////////////////////////////////////////

struct OTEntry
{
    inline bool is_new()       const { return flags & OF_New;        }
    inline bool is_dirty()     const { return flags & OF_Dirty;      }
    inline bool has_serial()   const { return !volref.is_null();     }
    inline bool is_secondary() const { return flags & OF_Secondary;  }
    inline bool is_primary()   const { return !is_secondary();       }
    inline bool on_pool_list() const { return flags & OF_OnPoolList; }
    inline bool is_unswizzled() const { return flags & OF_UnSwizzled; }

    // The object's serial number.  The volid is given by vindex.
    VolRef volref;

    // This field is used for different purposes depending on the
    // otentry.  For anonymous objects created within the current
    // transaction, it points to the pool in which the object is to be
    // created.  For other anonymous objects, it may point to the pool
    // in which the object lives or it may be 0.  For pools, it is
    // used as the `next' field of a linked list of pools in which
    // objects have been created during the current transaction.
    union{
	OTEntry *pool;
	OTEntry *next;
    };

    // The index into the volume table for this object's volume.
    uint2 volume;

    // Lock state of the object relative to this transaction.
    uint1 lockmode;

    // Status flags
    uint1 flags;

    // The object's location in memory, if it is cached.  For secondary
    // OTEs, this field is *always* 0.  To find the object's location
    // given a secondary OTE, use `snap' to find the primary OTE.  If
    // the primary OTE's `obj' field is 0, then the object is not cached.
    caddr_t obj;

    // Points to a circular linked list of aliases (secondary OT entries).
    OTEntry *alias;

    // Points to the object's C++ type object.
    rType *type;

    //
    // This info is used by LOIDTable methods ONLY (i.e., don't touch).
    //

    // Points to the next entry in the LOID hash table bucket.
    OTEntry *lt_next;
};

#ifdef oldcode
// One of these is placed behind every cached object.
struct BehindRec
{
    // Back pointer to the object's primary OT entry
    OTEntry *otentry;
    // to save a few bytes, put the length here.
    ObjectSize osize;
    // pinning & cache management flags
    short pin_count;
    short ref_bits;
    BehindRec * page_link;
};
#endif
#include "CacheBHdr.h"

typedef CacheBHdr BehindRec;
// Given a pointer to the beginning of an object, return its behind
// rec.
inline BehindRec *get_brec(const void *p)
{
    return ((BehindRec *)p) - 1;
}

#endif
