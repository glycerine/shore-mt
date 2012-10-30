/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef SORT_S_H
#define SORT_S_H

#ifdef __GNUG__
#pragma interface
#endif

typedef int  (*PFC) (uint4 kLen1, const void* kval1, uint4 kLen2, const void*
kval22, const void* cdata);

//
// info on keys
//
struct key_info_t {
    typedef sortorder::keytype key_type_t;

    // for backward compatibility only: should use keytype henceforth
    enum dummy_t { t_char=sortorder::kt_u1,
	t_int=sortorder::kt_i4,
	t_float=sortorder::kt_f4,
	t_string=sortorder::kt_b,
	t_spatial=sortorder::kt_spatial,
	t_custom=sortorder::kt_custom };

    enum where_t 	{ t_hdr=0, t_body };

    key_type_t  type;	    // key type
    nbox_t 	universe;   // for spatial object only
    bool	derived;    // if true, the key must be the only item in rec
			    // header, and the header will not be copied to
			    // the result record (allow user to store derived
			    // key temporarily for sorting purpose).

    // following applies to file sort only
    where_t 	where;      // where the key resides
    uint4	offset;	    // offset fron the begin
    uint4	len;	    // key length
    uint4	est_reclen; // estimated record length
    /**
     * A pointer to any additional data needed by the comparison function.
     * this pointer is passed directly into the function and is not altered
     * by surrounding code.
     */
    void *      comp_data;
    /**
     * A function which accepts two arguments, item_1 and item_2, and any
     * additional data necessary for comparison (see comp_data above) and
     * returns the following:
     * Returns     When
     *    0        item_1 = item_2
     *   < 0       item_1 < item_2
     *   > 0       item_1 > item_2
     */
    PFC         cmp;

    key_info_t() {
      type = sortorder::kt_i4;
      where = t_body;
      offset = 0;
      len = sizeof(int);
      derived = false;
      est_reclen = 0;
    }
};

//
// sort parameter
//
struct sort_parm_t {
    uint2    run_size;		// size for each run (# of pages)
    vid_t    vol;		// volume for files
    serial_t logical_id;	// logical oid
    bool   unique;		// result unique ?
    bool   ascending;		// ascending order ?
    bool   destructive;	// destroy the input file at the end ?
    bool   keep_lid;		// preserve logical oid for recs in sorted
				// file -- only for destructive sort
    lvid_t   lvid;		// logical volume id
    smlevel_3::sm_store_property_t property; // temporary file ?

    sort_parm_t() : run_size(10), unique(false), ascending(true),
		    destructive(false), keep_lid(false),
		    lvid(lvid_t::null), property(smlevel_3::t_regular) {}
};

#endif // _SORT_S_H_
