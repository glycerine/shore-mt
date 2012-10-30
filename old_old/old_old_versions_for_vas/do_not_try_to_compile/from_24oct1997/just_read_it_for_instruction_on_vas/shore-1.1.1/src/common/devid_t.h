/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/devid_t.h,v 1.8 1997/05/19 19:40:59 nhall Exp $
 */
#ifndef DEVID_T_H
#define DEVID_T_H

#ifdef __GNUG__
#pragma interface
#endif

struct devid_t {
    ino_t id;  /* device IDs are inodes + device */
    dev_t dev; 

#ifdef __cplusplus
    devid_t() : id(0), dev(0) {};
    devid_t(const char* pathname);

    bool operator==(const devid_t& d) const {
		return id == d.id && dev == d.dev;
	}
    bool operator!=(const devid_t& d) const {return !(*this==d);}
    friend ostream& operator<<(ostream&, const devid_t& d);

    static const devid_t null;
#endif
};
#endif /*DEVID_T_H*/
