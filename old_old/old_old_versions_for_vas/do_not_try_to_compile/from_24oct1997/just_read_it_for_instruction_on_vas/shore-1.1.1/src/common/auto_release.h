/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: auto_release.h,v 1.7 1995/04/24 19:27:26 zwilling Exp $
 */
#ifndef AUTO_RELEASE_H
#define AUTO_RELEASE_H

template <class T>
class auto_release_t {
public:
    NORET                       auto_release_t(T& t)  
        : obj(t)  {};
    NORET                       ~auto_release_t()  {
        obj.release();
    }
private:
    T&                          obj;
};

#endif /*AUTO_RELEASE_H*/
