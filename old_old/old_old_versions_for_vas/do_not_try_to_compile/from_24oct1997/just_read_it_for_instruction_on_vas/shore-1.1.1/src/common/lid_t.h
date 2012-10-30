/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/lid_t.h,v 1.23 1997/05/19 19:41:04 nhall Exp $
 */
#ifndef LID_T_H
#define LID_T_H

/*
 * NB -- THIS FILE MUST BE LEGITIMATE INPUT TO cc and RPCGEN !!!!
 * Please do as follows:
 * a) keep all comments in traditional C style.
 * b) If you put something c++-specific make sure it's 
 * 	  got ifdefs around it
 */
#ifndef BASICS_H
#include "basics.h"
#endif
#ifndef SERIAL_T_H
#include "serial_t.h"
#endif

#ifdef __GNUG__
#pragma interface
#endif

/*********************************************************************
 * Logical IDs
 *
 ********************************************************************/

/*
    Physical volume IDs (vid_t) are currently used to make unique
    logical volume IDs.  This is a temporary hack which we support with
    this typedef:
*/
typedef uint2 VID_T;

	/* 
	// logical volume ID 
	*/

#define LVID_T
struct lvid_t {
    /* usually generated from net addr of creating server */
    uint4 high;
    /* usually generated from timeofday when created */
    uint4 low;

#ifdef __cplusplus
    /* do not want constructors for things embeded in objects. */
    lvid_t() : high(0), low(0) {}
    lvid_t(uint4 hi, uint4 lo) : high(hi), low(lo) {}
	
    operator==(const lvid_t& s) const
			{return (low == s.low) && (high == s.high);}
    operator!=(const lvid_t& s) const
			{return (low != s.low) || (high != s.high);}

    // in lid_t.c:
    friend ostream& operator<<(ostream&, const lvid_t&);
    friend istream& operator>>(istream&, lvid_t&);

    // defined in lid_t.c
    static const lvid_t null;
#endif 
};

/*
// short logical record ID (serial number)
// defined in serial_t.h
*/

struct lid_t {
    lvid_t	lvid;
    serial_t    serial;

#ifdef __cplusplus

    lid_t() {};
    lid_t(const lvid_t& lvid_, const serial_t& serial_) :
		lvid(lvid_), serial(serial_)
    {};

    lid_t(uint4 hi, uint4 lo, uint4 ser, bool remote) :
		lvid(hi, lo), serial(ser, remote)
    {};

    inline operator==(const lid_t& s) const { 
		return ( (lvid == s.lvid) && (serial == s.serial)) ;
	};
    inline operator!=(const lid_t& s) const {
		return (serial != s.serial) || (lvid != s.lvid);
	};

	// in lid_t.c:
    friend ostream& operator<<(ostream&, const lid_t& s);
    friend istream& operator>>(istream&, lid_t& s);

    static const lid_t null;

    /* this is the key descriptor for using serial_t's as btree keys */
    static const char* key_descr; 
#endif

};

typedef	lid_t lrid_t;

#ifdef __cplusplus

inline u_long hash(const lvid_t& lv)
{
    return lv.high + lv.low;
}

inline u_long hash(const lid_t& l)
{
    return hash(l.serial) * hash(l.lvid);
}
#endif /*__cplusplus*/

#endif /*LID_T_H*/
