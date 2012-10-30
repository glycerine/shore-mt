/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef RDTREE_P_H
#define RDTREE_P_H

#ifdef __GNUG__
#pragma interface
#endif

class rdtree_p : public rtree_base_p {

    bool exact_match(const rangeset_t& key, u_char smap[], const cvec_t& el,
		       const shpid_t child = 0);
    bool set_srch(const rangeset_t& key, sob_cmp_t ctype, u_char smap[],
			int2& numSlot);

public:

    MAKEPAGE(rdtree_p, rtree_base_p, 1);

    NORET rdtree_p(const rtree_base_p &rbp) : rtree_base_p(rbp) {};
    rc_t calc_bound(rangeset_t& nbound);
    void pick_optimum(const rangeset_t& key, int2& ret_slot);

    rc_t insert(const rtrec_t& tuple);
    rc_t insert(const rangeset_t& key, const cvec_t& el, shpid_t child=0);

//    bool ov_remove(rdtree_p* tmp, const rangeset_t& key, const nbox_t& bound);
    bool search(const rangeset_t& key, sob_cmp_t ctype, u_char smap[],
			int2& num_slot, const cvec_t* el = 0,
		        const shpid_t child = 0);
    bool query(const rangeset_t& key, sob_cmp_t ctype, u_char smap[], 
		 int2& num_slot);
    shpid_t empties() const { w_assert3(pid().page == root()); return next(); }
    void add_empty(rdtree_p newpage) { 
	w_assert3(pid().page == root());
	newpage.link_up(root(), (shpid_t)NULL);
	link_up((shpid_t)NULL, newpage.pid().page);
    }
    void print();
//    void draw();

};

class rdtwork_p {
    page_s* _buf;
    rdtree_p _pg;
    rangeset_t *mbb;
    
public:
    NORET rdtwork_p()
    : _buf(new page_s), _pg(_buf, _pg.st_tmp)  {
        w_assert3(_pg);
	mbb = new rangeset_t;
    }

    NORET rdtwork_p(const lpid_t& pid, int2 l, int2 d = 2)
    : _buf(new page_s), _pg(_buf, _pg.st_tmp) {
            _pg.format(pid, rdtree_p::t_rdtree_p, _pg.t_virgin);
            _pg.set_level(l);
            _pg.set_dim(d);
	    mbb = new rangeset_t;
        }

    NORET ~rdtwork_p() { if (_buf)  delete _buf;  if (mbb) delete mbb; }

    void init(int2 l, int2 d = 2) {
        lpid_t pid;

	_pg.format(pid, rdtree_p::t_rdtree_p, _pg.t_virgin);
        _pg.set_level(l);
        _pg.set_dim(d);
	if (!mbb) mbb = new rangeset_t;
        }

    void swap(rdtwork_p& page) {
        rdtree_p tmp = _pg; _pg = page._pg; page._pg = tmp;
	page_s* tmp_buf = _buf; _buf = page._buf; page._buf = tmp_buf;
	rangeset_t* tmp_set; tmp_set = mbb; mbb = page.mbb; page.mbb = tmp_set;
        }

    const rangeset_t& bound() { return *mbb; }

    rc_t calc_set_bound() {
	W_DO( _pg.calc_bound(*mbb) );
	return RCOK;
        }

    void expn_bound(const rangeset_t& key) {
	(*mbb) += key;
        }

    rdtree_p* rp()		{ return &_pg; }  // convert to rtree page
};

struct rdt_wrk_branch_t {
    rangeset_t set;
    int2 idx;
    double size;
};

struct rdt_cursor_t {
    sob_cmp_t   cond;
    rangeset_t	qset;
    ftstk_t     fl;

    int2	num_slot;
    u_char	smap[rtree_p::smap_sz];
    int2	idx;
    rdtree_p 	page;

    NORET rdt_cursor_t() { num_slot = idx = 0; bm_zero(smap, sizeof(u_char)*8); }

};

#endif /*RDTREE_P_H*/
