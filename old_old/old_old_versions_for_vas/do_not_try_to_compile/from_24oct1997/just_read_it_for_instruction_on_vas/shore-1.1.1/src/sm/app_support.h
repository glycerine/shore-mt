/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: app_support.h,v 1.7 1995/04/24 19:34:59 zwilling Exp $
 */
#ifndef APP_SUPPORT_H
#define APP_SUPPORT_H

/* BEGIN VISIBLE TO APP */

/*
 * This file contains support for application programs to access
 * file page structures.
 *
 * It is used to generate common/sm_app.h.
 * 
 * Users should see sm/file_s.h for definitions of record_t.
 */

class shore_file_page_t : public page_s {

public:
    int slot_count() const { return nslots; }

    record_t* rec_addr(int idx) const {
	return ((idx > 0 && idx < nslots && slot[-idx].offset >=0) ? 
	        (record_t*) (data + slot[-idx].offset) : 
		0);
    }
};

/*
 * compile time constants also available from ss_m::config_info()
 *
 * The correctness of these constants is checked in ss_m::ss_m().
 */
class ssm_constants {
public: 
    enum {
	max_small_rec = page_s::data_sz - sizeof(file_p_hdr_t) -
			sizeof(page_s::slot_t) - sizeof(rectag_t),
	lg_rec_page_space = page_s::data_sz
    };
};

/* END VISIBLE TO APP */

#endif /*APP_SUPPORT_H*/
