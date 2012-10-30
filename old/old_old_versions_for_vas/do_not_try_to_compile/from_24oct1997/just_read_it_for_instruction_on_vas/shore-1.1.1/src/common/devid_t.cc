/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/devid_t.cc,v 1.5 1997/06/15 02:36:01 solomon Exp $
 */

#define DEVID_T_C

#ifdef __GNUC__
#pragma implementation
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <stream.h>
#include <sys/stat.h>
#include "w_base.h"
#include "basics.h"
#include "devid_t.h"
#ifdef PURIFY
#include <purify.h>
#include <memory.h>
#endif

devid_t::devid_t(const char* path)
{
    // generate a device id by stat'ing the path and using the
    // inode number

    struct stat buf;
#ifdef PURIFY
    if(purify_is_running()) {
	memset(&buf, '\0', sizeof(buf));
    }
#endif
    if(stat(path, &buf)) {
	// error
	id = 0;
	dev = 0;
    }
    id = buf.st_ino;
    dev = buf.st_dev;
}

ostream& operator<<(ostream& o, const devid_t& d)
{
    return o << d.dev << "." << d.id;
}

const devid_t devid_t::null;
