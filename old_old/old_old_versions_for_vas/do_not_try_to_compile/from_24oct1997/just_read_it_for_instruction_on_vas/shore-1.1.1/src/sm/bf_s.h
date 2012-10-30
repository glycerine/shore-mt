/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: bf_s.h,v 1.29 1997/05/27 13:09:19 kupsch Exp $
 */
#ifndef BF_S_H
#define BF_S_H

struct page_s;

class bfpid_t : public lpid_t {
public:
    NORET			bfpid_t();
    NORET			bfpid_t(const lpid_t& p);
    bfpid_t& 			operator=(const lpid_t& p);
    bool 		        operator==(const bfpid_t& p) const;
    static const bfpid_t	null;
};

inline NORET
bfpid_t::bfpid_t()
{
}

inline NORET
bfpid_t::bfpid_t(const lpid_t& p) : lpid_t(p)
{
}

inline bfpid_t&
bfpid_t::operator=(const lpid_t& p)  
{
    *(lpid_t*)this = p;
    return *this;
}

inline bool 
bfpid_t::operator==(const bfpid_t& p) const
{
    return vol() == p.vol() && page == p.page;
}


/*
 *  bfcb_t: buffer frame control block.
 */
struct bfcb_t {
public:
    NORET       bfcb_t()    {};
    NORET       ~bfcb_t()   {};

    inline void	clear();

    w_link_t    link;           // used in hash table

    bfpid_t	pid;            // page currently stored in the frame
    bfpid_t     old_pid;        // previous page in the frame
    bool        old_pid_valid;  // is the previous page in-transit-out?
    page_s*     frame;          // pointer to the frame

    bool	dirty;          // true if page is dirty
    lsn_t       rec_lsn;        // recovery lsn

    int4        pin_cnt;        // count of pins on the page
    latch_t     latch;          // latch on the frame
    scond_t     exit_transit;   //signaled when frame exits the in-transit state

    int4        refbit;         // ref count (for strict clock algorithm)
				// for replacement policy only

    int4        hot;         	// copy of refbit for use by the cleaner algorithm
				// without interfering with clock (replacement)
				// algorithm.

    inline ostream&    print_frame(ostream& o, bool in_htab);
    void 	       update_rec_lsn(latch_mode_t);

private:
    // disabled
    NORET       bfcb_t(const bfcb_t&);
    bfcb_t&     operator=(const bfcb_t&);
};


inline ostream&
bfcb_t::print_frame(ostream& o, bool in_htab)
{
    if (in_htab) {
        o << pid << '\t'
	  << (dirty ? "X" : " ") << '\t'
	  << rec_lsn << '\t'
	  << pin_cnt << '\t'
	  << latch_t::latch_mode_str[latch.mode()] << '\t'
	  << latch.lock_cnt() << '\t'
	  << latch.is_hot() << '\t'
	  << refbit << '\t'
#ifdef DEBUG
	  << latch.holder() 
#else
	  << latch.id() 
#endif
	  << endl;
    } else {
	o << pid << '\t' 
	  << " InTransit " << (old_pid_valid ? (lpid_t)old_pid : lpid_t::null)
	  << endl << flush;
    }
    return o;
}


inline void
bfcb_t::clear() 
{
    pid = lpid_t::null;
    old_pid = lpid_t::null;
    old_pid_valid = false;
    dirty = false;
    rec_lsn = lsn_t::null;
    hot = 0;
    refbit = 0;
    w_assert3(pin_cnt == 0);
    w_assert3(latch.num_holders() <= 1);
}


#endif /*BF_S_H*/
