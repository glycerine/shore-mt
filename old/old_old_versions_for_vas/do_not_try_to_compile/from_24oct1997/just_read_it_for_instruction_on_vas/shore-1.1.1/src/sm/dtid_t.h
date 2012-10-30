/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: dtid_t.h,v 1.6 1997/05/19 19:47:09 nhall Exp $
 */

#ifndef DTID_T_H
#define DTID_T_H

#ifndef W_H
#include <w.h>
#endif
#ifndef BASICS_H
#include <basics.h>
#endif
#include <memory.h>
#ifndef TID_T_H
#include <tid_t.h>
#endif

// distributed transaction type
// fixed size for us

class DTID_T {

public:
    NORET 	DTID_T();
    void 	update();
    void 	convert_to_gtid(gtid_t &g) const;
    void 	convert_from_gtid(const gtid_t &g);
	
private:
    static uint4 unique();

    uint4	location;
    uint4	relative;
    char	date[26]; 
    char	nulls[2]; // pad to mpl of 4, 
	// and terminate the string with nulls.
};

inline void
DTID_T::convert_to_gtid(gtid_t &g) const
{
    w_assert3(sizeof(DTID_T) < max_gtid_len);
    g.zero();
    g.append(&this->location, sizeof(this->location));
    g.append(&this->relative, sizeof(this->relative));
    g.append(&this->date, sizeof(this->date));
}

inline void        
DTID_T::convert_from_gtid(const gtid_t &g)
{
    w_assert3(sizeof(DTID_T) < max_gtid_len);

    w_assert3(g.length() == sizeof(DTID_T));
    int i=0;

#define GETX(field) \
    memcpy(&this->field, g.data_at_offset(i), sizeof(this->field)); \
    i += sizeof(this->field);

    GETX(location);
    GETX(relative);
    GETX(date);
#undef GETX

}
#endif /*DTID_T_H*/
