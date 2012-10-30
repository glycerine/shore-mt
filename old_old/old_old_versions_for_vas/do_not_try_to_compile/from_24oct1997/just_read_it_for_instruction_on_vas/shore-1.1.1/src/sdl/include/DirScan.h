/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// DirScan.h
//

/* $Header: /p/shore/shore_cvs/src/sdl/include/DirScan.h,v 1.11 1996/07/01 18:23:48 schuh Exp $ */

#ifndef __DIRSCAN_H__
#define __DIRSCAN_H__

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma interface
#endif

#include "Shore.h"

#ifndef DEF_DIRSCAN_BUFSIZE
#define DEF_DIRSCAN_BUFSIZE	(sizeof(_entry) + MAXNAMLEN - 1)
#endif

class DirScan
{
 public:

    // Constructors
    DirScan();
    DirScan(const char *path, size_t bufsize = DEF_DIRSCAN_BUFSIZE);

    // Methods for opening and closing scans
    shrc open(const char *path, size_t bufsize = DEF_DIRSCAN_BUFSIZE);
    shrc close();

    // Destructor; closes the scan if it is open.
    ~DirScan();

    // Methods for dealing with the state of the scan.
    bool is_open();
    bool operator!()			{ return (void *)_rc != (void *)RCOK; }
    inline int operator==(shrc &r)	{ return (void *)_rc == (void *)r; }
    inline int operator!=(shrc &r)	{ return (void *)_rc != (void *)r; }
    inline shrc rc()			{ return _rc; }

    // Advances the scan.
    shrc next(DirEntry *entry);

 protected:

    bool _open;
    shrc _rc;
    int xact_num;
    LOID loid;
    Cookie cookie;
    size_t _bufsize;
    int _nentries;
    char *_buf;
    char *_ptr;
};

#endif
