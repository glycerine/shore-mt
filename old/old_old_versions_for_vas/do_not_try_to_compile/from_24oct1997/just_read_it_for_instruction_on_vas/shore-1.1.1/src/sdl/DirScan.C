/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// DirScan.C
//

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma implementation "DirScan.h"
#endif

#include <string.h>
#include "DirScan.h"
#include "OCRef.h"
#include "ObjCache.h"
#include "SH_error.h"


bool
DirScan::is_open()
{
    if(_open && xact_num == OC_ACC(curr_xact_num))
	return true;
    return false;
}

DirScan::DirScan()
{
    _open = false;
    _rc = RCOK;
    _bufsize = 0;
    _buf = 0;
}

DirScan::DirScan(const char *path, size_t bufsize)
{
    _open = false;
    W_IGNORE(open(path, bufsize));
}

shrc
DirScan::open(const char *path, size_t bufsize)
{
    shrc rc;

    // if the scan object is already open then error
    if(is_open())
	_rc = RC(SH_ScanAlreadyOpen);

    else{

	// try to open a scan over the directory
	rc = OC_ACC(dirscan_open)(path, loid);
	if(rc)
	    _rc = RC_AUGMENT(rc);

	else{

	    cookie = NoSuchCookie;
	    if(bufsize < DEF_DIRSCAN_BUFSIZE)
		_bufsize = DEF_DIRSCAN_BUFSIZE;
	    else
		_bufsize = bufsize;

	    // use the "transactional allocator" so the memory will
	    // be freed at EOT if we don't get to it ourselves
	    _buf = OC_ACC(xact_alloc)(_bufsize);
	    _nentries = 0;
	    _rc = RCOK;
	}
    }

    // if we got here with no errors, then the scan must be ok
    if(!_rc /* _rc == RCOK */){
	_open = true;
	xact_num = OC_ACC(curr_xact_num);
    }

    // otherwise, it's still closed
    else
	_open = false;

    return _rc;
}

shrc
DirScan::next(DirEntry *entry)
{
    OTEntry *ote;
    shrc rc;

    // if the scan object isn't open then error
    if(!is_open())
	_rc = RC(SH_ScanNotOpen);

    // if the scan is OK...
    if(!_rc /* _rc == RCOK */){

	// if we don't have any cached entries, get some
	if(_nentries == 0){

	    rc = OC_ACC(dirscan_next)(loid, &cookie,
				    _buf, _bufsize, &_nentries);
	    if(rc)
		_rc = RC_AUGMENT(rc);
	    else{
		_rc = RCOK;
		_ptr = _buf;
	    }
	}

	// if the scan is still OK, return the next entry
	if(!_rc /* _rc == RCOK */){

	    // make sure there is one
	    if(_nentries == 0)
		// how odd.  was
		// _rc == RC(SH_Internal);
		// a stunning inversion  of the normal if (a=b) error.
		_rc = RC(SH_Internal);

	    else{

		_entry *e = (_entry *)_ptr;
		entry->loid.set(loid.volid(), e->serial);
		entry->namelen = e->string_len;
		strncpy(entry->name, &(e->name), e->string_len);
		if(e->string_len < MAXNAMLEN)
		    entry->name[e->string_len] = '\0';
		_ptr += e->entry_len;
		--_nentries;
	    }
	}
    }

    return _rc;
}

shrc
DirScan::close()
{
    // if the scan isn't open then error
    if(!is_open())
	_rc = RC(SH_ScanNotOpen);

    else{
	_rc = RCOK;
	_open = false;

	// free the memory via the transactional allocator
	if(_buf){
	    W_DO(OC_ACC(xact_free)(_buf));
	    _buf = 0;
	}
    }

    return _rc;
}

DirScan::~DirScan()
{
    if(is_open())
	W_IGNORE(close());
}
