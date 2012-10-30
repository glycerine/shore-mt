/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// OCTypes.h
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/OCTypes.h,v 1.24 1995/09/04 12:44:34 solomon Exp $ */

#ifndef _OCTYPES_H_
#define _OCTYPES_H_

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma interface
#endif

// for caddr_t
#include <sys/types.h>

// for MAXNAMLEN
#include <dirent.h>

#ifdef Linux
/* MAXNAMLEN is BSD, NAME_MAX is Posix */
#define MAXNAMLEN NAME_MAX
#endif

// for w_rc_t, w_error_t
#include "w_base.h"
#include "w_rc.h"

// For now, the client side will use w_rc_t as its return code type.
// This may change in the future.
typedef w_rc_t shrc;

// for uint1, uint2, etc.
#include "basics.h"

// for serial_t and lid_t
#include "lid_t.h"

// for TxStatus and LockMode
// note: there is a bad naming conflict in vas_types.h, 
// with typedef serial_t_data Ref;
// vs. the sdl Ref template; we do a hack define to get around this.
#define Ref vasRef
#include "vas_types.h"
#undef Ref
// this sort of sucks.

// error handler function type
typedef void (*sh_error_handler)(shrc rc);

typedef lvid_t VolId;
typedef serial_t VolRef;

// Statistics
#include "w_statistics.h"
class OCstats {
public:
	void compute(class ObjCache *);
#   include "OCstats_struct.i"
};
extern OCstats stats;

// A logical OID.
struct LOID
{
    friend class ObjCache;
    friend class PoolScan;
    friend ostream &operator<<(ostream &os, const LOID &loid)
    { return os << loid.id; }
    friend istream &operator>>(istream &is, LOID &loid)
    { return is >> loid.id; }

 public:

    LOID() : id(lid_t::null) {}

    bool is_local()		  const { return id.serial.is_local(); }
    bool is_remote()		  const { return id.serial.is_remote(); }

    operator==(const LOID &loid)  const { return id == loid.id; }
    operator!=(const LOID &loid)  const { return id != loid.id; }

    operator==(const lid_t &lid)  const { return id == lid; }
    operator!=(const lid_t &lid)  const { return id != lid; }

    const VolId &volid()	  const	{ return id.lvid; }
    const VolRef &volref()	  const	{ return id.serial; }

    inline void set(const VolId &vi, const VolRef &vr)
    {
	id.lvid = vi;
	id.serial = vr;
    }

    inline void set(const VolId &vi, const serial_t_data &d)
    {
	id.lvid = vi;
	id.serial.set(d);
    }

    inline void set(const LOID &loid)
    {
	id = loid.id;
    }

    // This silliness is only to be used to construct the null LOID,
    // which, for arcane bizarre reasons, has a serial number of 1.
#ifdef BITS64
    inline LOID(int v_high, int v_low, int s_high, int s_low)
    {
	id.lvid.high         = v_high;
	id.lvid.low          = v_low;
	id.serial.data._high = s_high;
	id.serial.data._low  = s_low;
    }
#else
    inline LOID(int v_high, int v_low, int s_low)
    {
	id.lvid.high         = v_high;
	id.lvid.low          = v_low;
	id.serial.data._low  = s_low;
    }
#endif

    // defined in ObjCache.C, for want of a better place to put it
    static const LOID null;

    lid_t id;
};

//
// These types are used by OCRef::stat()
//

class rType;

struct AnonStat
{
    LOID pool;		// primary loid of pool in which obj resides
};

struct RegStat
{
    short nlink;	// num. dir. entries pointing to object
    mode_t mode;	// permissions flags
    uid_t uid;		// owner
    gid_t gid;		// group
    time_t atime;	// access time
    time_t mtime;	// modify time
    time_t ctime;	// props change time
};

struct OStat
{
    LOID loid;		// object's primary logical oid
    LOID type_loid;	// loid of object's type object
    ObjectSize csize;	// core size
    ObjectSize hsize;	// heap size
    int nindices;	// number of indices in object
    ObjectKind kind;	// KindRegistered or KindAnonymous
    AnonStat astat;	// if kind == KindAnonymous
    RegStat  rstat;	// if kind == KindRegistered
};

//
// This type is used by DirScan
//

struct DirEntry
{
    LOID loid;
    int namelen;
    char name[MAXNAMLEN + 1];
};


// Default lock modes.  When an object is cached for reading, the
// object cache will request a READ_LOCK_MODE lock.  If an object is
// to be written, the object cache requests a WRITE_LOCK_MODE lock.
// NEW_OBJ_LOCK_MODE is the lock mode that is recorded for new
// objects.  New objects are not actually locked, but they are
// effectively locked by virtue of being new.  SYSPROPS_LOCK_MODE is
// the lock mode that is used when making a sysprops vas call.  If the
// application creates an object in a pool, the object cache requests
// a POOL_LOCK_MODE lock on the pool to make sure it isn't deleted
// before the buffered object creation request makes it to the server.
//
// MIN_LOCK_MODE and MAX_LOCK_MODE are the lowest and highest lock
// modes; they are used for checking bounds.  MIN_WRITE_LOCK_MODE is
// the lowest lock mode required to modify an object.

#define NO_LOCK_MODE		NL
#define MIN_LOCK_MODE		SH
#define MAX_LOCK_MODE		EX

#define READ_LOCK_MODE		SH
#define WRITE_LOCK_MODE		UD
#define NEW_OBJ_LOCK_MODE	EX
#define DESTROY_LOCK_MODE	EX
#define SYSPROPS_LOCK_MODE	SH
#define POOL_LOCK_MODE		SH
#define MIN_WRITE_LOCK_MODE	UD

// for rType (which needs LOID)
#include "Type.h"

// Used by the language binding.
struct StringRec
{
    char *string;
    int length;
};

#endif
