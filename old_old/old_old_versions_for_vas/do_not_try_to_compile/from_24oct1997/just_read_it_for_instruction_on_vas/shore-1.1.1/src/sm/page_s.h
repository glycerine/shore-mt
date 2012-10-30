/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: page_s.h,v 1.14 1997/05/19 19:47:39 nhall Exp $
 */
#ifndef PAGE_S_H
#define PAGE_S_H

#ifdef __GNUG__
#pragma interface
#endif

/* BEGIN VISIBLE TO APP */

/*
 * Basic page structure for all pages.
 */
class xct_t;

struct page_s {
    struct slot_t {
	int2 offset;		// -1 if vacant
	uint2 length;
    };
    
    class space_t {
    public:
	space_t()	{};
	~space_t()	{};
	
	void init(int nfree, int rflag)  { 
	    _tid = tid_t(0, 0);
	    _nfree = nfree;
	    _nrsvd = _xct_rsvd = 0;
	    _rflag = rflag;
	}
	
	int nfree() const	{ return _nfree; }
	bool rflag() const	{ return _rflag; }
	
	int			usable(xct_t* xd);
				// slot_bytes means bytes for new slots
	rc_t			acquire(int amt, int slot_bytes, xct_t* xd,
					bool do_it=true);
	void 			release(int amt, xct_t* xd);
	void 			undo_acquire(int amt, xct_t* xd);
	void 			undo_release(int amt, xct_t* xd);


    private:
   
	void _check_reserve();
	
	tid_t	_tid;		// youngest xct contributing to _nrsvd
	int2	_nfree;		// free space counter
	int2	_nrsvd;		// reserved space counter
	int2	_xct_rsvd;	// amt of space contributed by _tid to _nrsvd
	int2	_rflag;
    };
    enum {
	data_sz = (smlevel_0::page_sz - 
		   2 * sizeof(lsn_t) 
		   - sizeof(lpid_t) -
		   2 * sizeof(shpid_t) 
		   - sizeof(space_t) -
		   4 * sizeof(int2) - 
		   2 * sizeof(int4) -
		   2 * sizeof(slot_t)),
	max_slot = data_sz / sizeof(slot_t) + 2
    };

 
    lsn_t	lsn1;
    lpid_t	pid;			// id of the page
    shpid_t	next;			// next page
    shpid_t	prev;			// previous page
    space_t 	space;			// space management
    uint2	end;			// offset to end of data area
    int2	nslots;			// number of slots
    int2	nvacant;		// number of vacant slots
    uint2	tag;			// page_p::tag_t
    uint4	store_flags;		// page_p::store_flag_t
    uint4	page_flags;		// page_p::page_flag_t
    char 	data[data_sz];		// must be aligned
    slot_t	reserved_slot[1];	// 2nd slot (declared to align
					// end of _data)
    slot_t	slot[1];		// 1st slot
    lsn_t	lsn2;
    void ntoh(vid_t vid);
};

/* END VISIBLE TO APP */
#endif /*PAGE_S_H*/
