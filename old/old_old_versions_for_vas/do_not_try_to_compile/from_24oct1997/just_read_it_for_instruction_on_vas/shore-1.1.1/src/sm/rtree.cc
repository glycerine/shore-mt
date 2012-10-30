/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: rtree.cc,v 1.111 1997/06/15 03:13:53 solomon Exp $
 */
#define SM_SOURCE
#define RTREE_C

#ifdef __GNUG__
#    pragma implementation "rtree.h"
#    pragma implementation "rtree_p.h"
#endif

#include <values.h>
#include <math.h>
#include <sm_int_2.h>
#include <rtree_p.h>
#include "sm_du_stats.h"

#include <crash.h>

#ifdef __GNUG__
template class w_auto_delete_array_t<wrk_branch_t>;
#endif


nbox_t CoverAll;
FILE *DrawFile;

INLINE void 
SWITCH(int2& x, int2& y)
{
    { int2 _temp_ = x; x = y; y = _temp_; }
}

const double MaxDouble = 4.0*max_int4*max_int4;

void 
rtstk_t::drop_all_but_last()
{
    w_assert1(! is_empty());
    while (_top>1) {
	_stk[--_top].page.unfix();
    }
}

ftstk_t::ftstk_t() {
    _top = 0;
    for (int i=0; i<ftstk_chunk; i++) {
	_indirect[i] = NULL;
    }
}

ftstk_t::~ftstk_t() {
    shpid_t tmp;
    while (_top>0) {
	tmp = pop();
	// xd->lock()
    }
}

void
ftstk_t::empty_all() {
    while (_top > 0) {
	_top--;
	if (_top>=ftstk_chunk && _top%ftstk_chunk==0) {
	    delete _indirect[_top/ftstk_chunk - 1];
	}
    }
}

//
// class rtwork_p
//
// In-memory version of rtree_p. Useful for temporary storage of a page
// before converting to persistent version. Heavy use in rtree bulk load.
//
class rtwork_p : public smlevel_0 {
    page_s* _buf;
    rtree_p _pg;
    nbox_t* mbb;
    
public:
    rtwork_p()
    : _buf(new page_s), _pg(_buf, st_tmp)  {
        w_assert3(_pg);
	mbb = new nbox_t(2);
    }

    rtwork_p(const lpid_t& pid, int2 l, int2 d = 2)
    : _buf(new page_s), _pg(_buf, st_tmp) {
	    /*
	     * Turning off logging makes this a critical section:
	     */
	    xct_log_switch_t log_off(smlevel_0::OFF);
            W_COERCE( _pg.format(pid, rtree_p::t_rtree_p, _pg.t_virgin) );
            W_COERCE( _pg.set_level(l) );
            W_COERCE( _pg.set_dim(d) );
	    mbb = new nbox_t(d);
        }

    ~rtwork_p() { if (_buf)  delete _buf;  if (mbb) delete mbb; }

    void init(int2 l, int2 d = 2) {
        lpid_t pid;
	/*
	 * Turning off logging makes this a critical section:
	 */
	xct_log_switch_t log_off(smlevel_0::OFF);

	W_COERCE( _pg.format(pid, rtree_p::t_rtree_p, _pg.t_virgin) ) ;
        W_COERCE( _pg.set_level(l)) ;
        W_COERCE( _pg.set_dim(d)) ;
	if (!mbb) mbb = new nbox_t(d);
        }

    void swap(rtwork_p& page) {
        rtree_p tmp = _pg; _pg = page._pg; page._pg = tmp;
	page_s* tmp_buf = _buf; _buf = page._buf; page._buf = tmp_buf;
	nbox_t* tmp_box; tmp_box = mbb; mbb = page.mbb; page.mbb = tmp_box;
        }

    const nbox_t& bound() { return *mbb; }

    rc_t			calc_set_bound() {
	return _pg.calc_bound(*mbb);
    }

    void expn_bound(const nbox_t& key) {
	(*mbb) += key;
        }

    rtree_p* rp()		{ return &_pg; }  // convert to rtree page
};

//
// class rtld_cache_t
//
// Rtree bulk load cache entry: allow a buffer of 3 pages
// before repacking and flushing out the next page at one
// level.
//
class rtld_cache_t {
    rtwork_p buf[3];
    int2 _idx;
    nbox_t* last_box;

public:
    rtld_cache_t() { _idx = -1; last_box=NULL; }
    ~rtld_cache_t() { if (last_box) delete last_box; }

    int2 count() { return _idx+1; }
    void incr_cnt() { _idx++; w_assert1(_idx<3); }

    void init_buf(int2 lvl);
	
    rtwork_p* top() {
	w_assert1(_idx >= 0);
	return &buf[_idx];
	}

    rtwork_p* bottom() {
	w_assert1(_idx >= 0);
	return &buf[0];
	}

    rc_t	force(
	rtwork_p& 	    work,
	bool& 	    out,
	nbox_t* 	    universe);
};

//
// class rtld_stk_t
//
// Rtree bulk load stack: each entry is the cache entry defined
// above, which corresponds to a level in the Rtree. Top entry
// is for the leaf level.
//
class rtld_stk_t {
    enum { max_ldstk_sz = 10 };
    rtree_p rp;
    rtld_cache_t* layers;
    int _top;
    rtld_desc_t dc;

    void init_next_layer() {
	_top++;
	layers[_top].init_buf(_top+1);
	layers[_top].incr_cnt();
	}

    rc_t			tmp2real(rtwork_p* tmp, rtree_p* real);

public:
    uint2 height;
    uint4 leaf_pages;
    uint4 num_pages;
    uint4 fill_sum;

    rtld_stk_t(const rtree_p& rtp, const rtld_desc_t& desc){ 
	rp = rtp;
	_top = -1; 
	dc = desc;
	layers = new rtld_cache_t[max_ldstk_sz]; 
	leaf_pages = num_pages = fill_sum = 0;
	height = 0;
	}

    ~rtld_stk_t() { if (layers) delete[] layers; }

    rc_t			add_to_stk(
	const nbox_t&		    key,
	const cvec_t& 		    el,
	shpid_t 		    child,
	int2 			    level);
    rc_t			wrap_up();
};

//
// sort function: sort rectangles on specified axis. (-1 for area)
//

static int 
_sort_cmp_area_(const void* k1, const void* k2)
{
    wrk_branch_t &b1 = *(wrk_branch_t *) k1;
    wrk_branch_t &b2 = *(wrk_branch_t *) k2;

    double diff = b1.area - b2.area;

    if (diff > 0.0) return 1;
    else if (diff < 0.0) return -1;
    else return 0;
}


/* Until a method of supporting per-sort (per thread) sort context 
   is developed, only allow one per-axis sort at a time. */

struct sort_context {
	int		axis;
	smutex_t	lock;
};

static struct sort_context sort_context;


static int 
_sort_cmp_axis_(const void* k1, const void* k2)
{
    wrk_branch_t &b1 = *(wrk_branch_t *) k1;
    wrk_branch_t &b2 = *(wrk_branch_t *) k2;
    double	diff;
    int		axis = sort_context.axis;

    diff = (double)b1.rect.bound(axis) - (double)b2.rect.bound(axis);
    if (diff==0.0) {
	    diff = (double)(b1.rect.bound(axis + b1.rect.dimension()))
		    - (double)(b2.rect.bound(axis + b2.rect.dimension()));
    }

    if (diff > 0.0) return 1;
    else if (diff < 0.0) return -1;
    else return 0;
}


static void 
quick_sort(wrk_branch_t key[], int2 num, int axis)
{
    if (axis == -1)
	    qsort(key, num, sizeof(wrk_branch_t), _sort_cmp_area_);
    else {
	    /* only allow one sort at a time, due to the _axis dependency */
	    W_COERCE(sort_context.lock.acquire());
	    sort_context.axis = axis;
	    qsort(key, num, sizeof(wrk_branch_t), _sort_cmp_axis_);
	    sort_context.lock.release();
    }
}


//
// overlap: sum of overlap between target and each box in list[]
//
static double 
overlap(const nbox_t& target, int2 index, 
	wrk_branch_t list[], int2 num)
{
    register int i;
    double sum = 0.0, area;

    for (i=0; i<num; i++) {
	if (i==index) continue;
	if ((area = (target^list[i].rect).area()) < 0.0 ) continue;
	sum += area;
    }
    return sum;
}

//
// set up header info on rtree page
//
rc_t
rtree_base_p::set_hdr(const rtctrl_t& new_hdr)
{
    vec_t v;
    v.put(&new_hdr, sizeof(new_hdr));
    W_DO( overwrite(0, 0, vec_t(&new_hdr, sizeof(new_hdr))) );
    return RCOK;
}

void rtree_base_p::ntoh()
{
    /*
     *  BUGBUG -- fill in when appropriate 
     */
    W_FATAL(eINTERNAL);
}

rc_t
rtree_p::format(const lpid_t& pid, tag_t tag, 
	uint4_t flags)
{
    W_DO( rtree_base_p::format(pid, tag, flags) );
    return RCOK;
}

void rtree_p::ntoh()
{
    /*
     *  BUGBUG -- fill in when appropriate 
     */
    W_FATAL(eINTERNAL);
}

MAKEPAGECODE(rtree_p, rtree_base_p);
MAKEPAGECODE(rtree_base_p, keyed_p);

//
// set up level in header
//
rc_t
rtree_base_p::set_level(int2 l)
{
    rtctrl_t tmp;
    
    tmp = _hdr();
    if (tmp.level == l) return RCOK;
    tmp.level = l;
    W_DO( set_hdr(tmp) );
    return RCOK;
}

//
// set up dimension in header
//
rc_t
rtree_base_p::set_dim(int2 d)
{
    rtctrl_t tmp;
    
    tmp = _hdr();
    if (tmp.dim == d) return RCOK;
    tmp.dim = d;
    W_DO( set_hdr(tmp) );
    return RCOK;
}

//
// recalc the bounding box of the whole page
//
rc_t
rtree_p::calc_bound(nbox_t& nb)
{
    register int i;
    nbox_t cur;
    nb.nullify();

    for (i=nrecs()-1; i>=0; i--)  {
	cur.bytes2box(rec(i).key(), rec(i).klen());
	nb += cur;
    }

    return RCOK;
}

//
// print out all entries in the current rtree page
//
void 
rtree_p::print()
{
    register int i, j;
    nbox_t key;

    for (i=0; i<nrecs(); i++)  {
    	key.bytes2box(rec(i).key(), rec(i).klen());
	key.print( level() );

	for (j=0; j<5-level(); j++) cout << "\t";
	if ( is_node() )  
	    cout << ":- pid = " << rec(i).child() << endl;
	else 
	    cout << "(" << rec(i).elem() << ")" << endl;
    }
    cout << endl;
}

//
// draw all the entries in the current rtree page
//
void 
rtree_p::draw()
{
    register int i;
    nbox_t key;

    for (i=0; i<nrecs(); i++)  {
    	key.bytes2box(rec(i).key(), rec(i).klen());
	key.draw(level(), DrawFile, CoverAll);
    }
}

//
// calculate overlap percentage (expensive -- n**2)
// 	total overlap area / total area
// 
//
uint2
rtree_p::ovp()
{
    register int i, j;
    nbox_t key1, key2;
    double all_sum = 0.0, ovp_sum = 0.0, area;

    for (i=0; i<nrecs(); i++)  {
    	key1.bytes2box(rec(i).key(), rec(i).klen());
	if ((area = key1.area()) < 0.0) continue;
	all_sum += area;
	for (j=0; j<nrecs(); j++) {
	    if (i!=j) {
		key2.bytes2box(rec(j).key(), rec(j).klen());
		if ((area = (key1^key2).area()) < 0.0) continue;
		ovp_sum += area;
	    }
	}
    }

    if (all_sum == 0.0) return 0;
    else return (uint2) (ovp_sum*50.0 / all_sum);
}

//
// format the rtree page
//
rc_t
rtree_base_p::format(const lpid_t& pid, tag_t tag, 
	uint4_t flags)
{
    rtctrl_t rtc;
    rtc.level = 1;
    rtc.dim = 2;
    vec_t vec;
    vec.put(&rtc, sizeof(rtctrl_t));

    W_DO( keyed_p::format(pid, tag, flags, vec) );

    return RCOK;
}

//
// pick the optimum branch in non-leaf page
//
void 
rtree_p::pick_optimum(const nbox_t& key, slotid_t& ret_slot)
{
    w_assert3( is_node() );

    ret_slot = -1;
    register int i;
    double min_area = MaxDouble;
    int2 min_idx = -1, count = nrecs();
    wrk_branch_t* work = new wrk_branch_t[count];
    w_assert1(work);
    w_auto_delete_array_t<wrk_branch_t> auto_del_work(work);

    // load the working structure
    for (i=0; i<count; i++) {
	work[i].rect.bytes2box(rec(i).key(), rec(i).klen());
	work[i].area = work[i].rect.area();
	work[i].idx = i;
	if (work[i].rect / key && work[i].area < min_area) {
	    min_area = work[i].area; ret_slot = i;
	}
	work[i].area = (work[i].rect+key).area() - work[i].area;
    }

    if (this->level()==2) {
	// for one level above leaf
    	double min_diff = MaxDouble;
	if (ret_slot!=-1) { // find one containment with least area
	    return;
	}
	// sort according to area difference of expansion
	quick_sort(work, count, -1);

	// find the pick with least overlapping resulting from the insertion
	// a hack to reduce computation
	for (i=MIN(32, count)-1; i>=0; i--) {
	    double diff = overlap((key + work[i].rect), i, work, count) -
				overlap(work[i].rect, i, work, count);
	    if (diff < min_diff) { min_diff = diff; min_idx = i; }
	}
    } else {
	// for other levels: find the pick with least expansion and area
	min_idx = 0;
	for (i=1; i<count; i++) {
	    double diff = work[min_idx].area - work[i].area;
	    if (diff > 0.0) {
		min_idx = i; continue;
	    }
	    if (diff==0.0 && work[i].rect.area()<work[min_idx].rect.area())
		min_idx = i;
	}
    }
    
    ret_slot = work[min_idx].idx;
}
	    
//
// remove REMOVE_RATIO*100% of furthest entries (in terms of center distance)
//
rc_t
rtree_p::ov_remove(rtree_p* dst, const nbox_t& key, const nbox_t& bound)
{
    register int i, j;
    int2 count = this->nrecs();
    int2 num_rem = (int2) ((count+1) * REMOVE_RATIO + 0.5);
    wrk_branch_t* work = new wrk_branch_t[count+1];
    if (!work) return RC(smlevel_0::eOUTOFMEMORY);
    w_auto_delete_array_t<wrk_branch_t> auto_del_work(work);

    // load work structure
    for (i=0; i<count; i++) {
	 work[i].rect.bytes2box(rec(i).key(), rec(i).klen());
	 work[i].area = -1.0 * (work[i].rect*bound); // center distance
	 work[i].idx = i;
    }
    work[count].area = -1.0 * (key * bound); // negative of center distance
    work[count].idx = count;
    work[count].rect = key;

    // sort by "area" (center distance in descending order)
    quick_sort(work, count+1, -1);

    // remove the last num_rem entries
    for (i=0; i<num_rem; i++) {
        int2 index = work[i].idx;
	// find the largest index of tuple to be removed
	for (j=i+1; j<num_rem; j++)
	    if (index < work[j].idx) { SWITCH(index, work[j].idx); }
	if (index != count) {
	    // move the current tuple to destination page
	    // skip the work[count] which is for the extra entry
	    // to be inserted (not in the page yet).
    	    const rtrec_t& tuple = rec(index);
	    W_DO( dst->insert(tuple) );
	    W_DO( this->remove(index) );
	}
    }

    return RCOK;
}
	    
//
// insert one tuple
//
rc_t
rtree_p::insert(const rtrec_t& tuple)
{
    nbox_t key(tuple.key(), tuple.klen());
    vec_t el((const void*) tuple.elem(), tuple.elen());
    shpid_t child = tuple.child();

    return ( insert(key, el, child) );
}

//
// insert one entry
//
rc_t
rtree_p::insert(const nbox_t& key, const cvec_t& el, shpid_t child)
{
    if (child==0) { w_assert3(is_leaf()); }
    else { w_assert3(!is_leaf()); }

    slotid_t num_slot = 0;
    u_char smap[smap_sz];

    // do a earch to find the correct position
    if ( search(key, nbox_t::t_exact, smap, num_slot, &el, child) ) {
	return RC(eDUPLICATE);
    } else {
	keyrec_t::hdr_s hdr;
	hdr.klen = key.klen();
	hdr.elen = el.size();
	hdr.child = child;

	vec_t vec;
	vec.put(&hdr, sizeof(hdr)).put(key.kval(), key.klen()).put(el);
	W_DO( keyed_p::insert_expand( bm_first_set(smap, nrecs()+1, 0) + 1,
				      1, &vec) );
	return RCOK;
    }
}

//
// remove one tuple
//
rc_t
rtree_base_p::remove(slotid_t slot)
{
    W_DO( keyed_p::remove_compress(slot + 1, 1) );
    return RCOK;
}

//
// exact match
//
bool 
rtree_p::exact_match(const nbox_t& key, u_char smap[], const cvec_t& el, 
		     const shpid_t child)
{
    register int low, high, mid;
    int diff;
    int2 cnt = nrecs();
    nbox_t box;

    bm_zero(smap, cnt+1);
    for (low=mid=0, high=cnt-1; low<=high; ) {
	mid = (low + high) >> 1;
	const rtrec_t& tuple = rec(mid);
    	box.bytes2box(tuple.key(), tuple.klen());
	if (box==key) {
	    if (is_leaf()) { // compare elements
		if (el.size()==0) { bm_set(smap, mid); return true; }
		if ((diff = el.cmp(tuple.elem(), tuple.elen())) > 0)
		  low = mid + 1;
		else if (diff < 0) high = mid - 1;
		else { bm_set(smap, mid); return true; }
	    }
	    else { // compare children
		if (tuple.child() == child) {
		    bm_set(smap, mid); 
		    return true;
		}
		if (child > tuple.child())
		  low = mid + 1;
		else // child < tuple.child()
		  high = mid -1;
	    }
	} else if (box < key)  low = mid + 1; 
	else  high = mid - 1;
    }

    bm_set(smap, ((low > mid) ? low : mid));

    return false;
}

//
// basic comparison functions for spatial objects (other than exact match)
//
static int
sob_cmp(const nbox_t& key, const nbox_t& box, nbox_t::sob_cmp_t type)
{
    switch(type) {
	case nbox_t::t_overlap: 	// overlap match
	    return (box || key);
	case nbox_t::t_cover: 		// coverage: key covers box
	    return (key / box);
	case nbox_t::t_inside:		// containment: box contained in key
	    return (box / key);
	default:
	    w_assert1(0);
	    return false;
    }
}

//
// spatial search
//
bool 
rtree_p::spatial_srch(const nbox_t& key, nbox_t::sob_cmp_t ctype, 
			       u_char smap[], slotid_t& num_slot)
{
    register int i;
    bool done = (num_slot==-1)? true : false;
    num_slot = 0;
    int2 cnt = nrecs();
    nbox_t box;

    bm_zero(smap, cnt);
    for (i=0; i<cnt; i++) {
	const rtrec_t& tuple = rec(i);
	box.bytes2box(tuple.key(), tuple.klen());
	if (sob_cmp(key, box, ctype)) {
	    bm_set(smap, i); num_slot++;
	    if (done) break;
	}
    }

    return (num_slot>0);
}

//
// translate operation to appropriate search condition on non-leaf nodes
//
bool 
rtree_p::query(const nbox_t& key, nbox_t::sob_cmp_t ctype, u_char smap[],
			slotid_t& num_slot)
{
    w_assert3(!is_leaf());
    nbox_t::sob_cmp_t cond = (ctype==nbox_t::t_exact)? nbox_t::t_inside : nbox_t::t_overlap;
    return search(key, cond, smap, num_slot);
}

//
// leaf and non-leaf level search function
//
bool 
rtree_p::search(const nbox_t& key, nbox_t::sob_cmp_t ctype, u_char smap[],
			slotid_t& num_slot, const cvec_t* el, const shpid_t child)
{
    switch(ctype) {
	case nbox_t::t_exact: 		// exact match search: binary search
	    num_slot = 1;
	    if (!el) {
		cvec_t dummy;
		return exact_match(key, smap, dummy, child);
	    }
	    return exact_match(key, smap, *el, child);
	case nbox_t::t_overlap: 	// overlap match
	case nbox_t::t_cover: 		// coverage: any rct cover the key
	case nbox_t::t_inside:		// containment: any rct contained in the key
	    return spatial_srch(key, ctype, smap, num_slot);
	default:
	    return false;
    }
}

//
// allocate a page for rtree:
//   This has to be called within a compensating action.
//
rc_t
rtree_m::_alloc_page(
    const lpid_t&	root,
    int2		level,
    const rtree_p&	near,
    int2		dim,
    lpid_t&		pid)
{
    w_assert3(near);
    W_DO( io->alloc_pages(root.stid(), near.pid(), 1, &pid) );

    rtree_p page;
    W_DO( page.fix(pid, LATCH_EX, page.t_virgin) );

    rtctrl_t hdr;
    hdr.root = root;
    hdr.level = level;
    hdr.dim = dim;  
    W_DO( page.set_hdr(hdr) );

    return RCOK;
}

//
// detect if the rtree is empty
//
bool 
rtree_m::is_empty(const lpid_t& root)
{
    rtree_p page;
    W_IGNORE( page.fix(root, LATCH_SH) ); // PAGEFIXBUG
    return (page.nrecs()==0);
}

//
// general search for exact match
//
rc_t
rtree_m::_search(
    const lpid_t& 		root,
    const nbox_t& 		key,
    const cvec_t& 		el,
    bool& 			found,
    rtstk_t& 			pl,
    oper_t oper)
{
    //lock_mode_t lmode = (oper == t_read) ? SH : EX;
    latch_mode_t latch_m = (oper == t_read) ? LATCH_SH : LATCH_EX;

    rtree_p page;
    W_DO( page.fix(root, latch_m) );

    // read in the root page
    //    W_DO( lm->lock(root, lmode, t_medium, WAIT_FOREVER) );
    pl.push(page, -1);

    // traverse through non-leaf pages
    W_DO ( _traverse(key, el, found, pl, oper) );

    return RCOK;
}

//
// traverse the tree (for exact match)
//
rc_t
rtree_m::_traverse(
    const nbox_t&	key,
    const cvec_t& 	el,
    bool& 		found,
    rtstk_t& 		pl,
    oper_t 		oper)
{
    register int i;
    //lock_mode_t lmode = (oper == t_read) ? SH : EX;
    latch_mode_t latch_m = (oper == t_read) ? LATCH_SH : LATCH_EX;

    rtree_p page;

    lpid_t pid = pl.top().page.pid();
    int2 count = pl.top().page.nrecs();
    slotid_t num_slot = 0;
    u_char smap[rtree_p::smap_sz];

    if (! pl.top().page.is_leaf())  {
	// check for containment
	found = ((rtree_p)pl.top().page).search(key, nbox_t::t_inside, smap, num_slot);
	if (!found) { // fail to find in the sub tree
	    return RCOK;
	}
	
	int first = -1;
	for (i=0; i<num_slot; i++) {
	    first = bm_first_set(smap, count, ++first);
	    pl.update_top((int2)first); // push to the stack
	    
	    // read in the child page
	    pid.page = pl.top().page.rec(first).child(); 
	    W_DO( page.fix(pid, latch_m) );

	    // traverse its children
	    pl.push(page, -1); // push to the stack
	    W_DO ( _traverse(key, el, found, pl, oper) );
	    if (found) { return RCOK; }

	    // pop the top 2 entries from stack, check for next child
	    pl.pop();
	}

	// no luck
	found = false;
	return RCOK;
    }
    
    // reach leaf level: exact match
    found = ((rtree_p)pl.top().page).search(key, nbox_t::t_exact, smap, num_slot, &el);
    if (found) { pl.update_top((int2)bm_first_set(smap, count, 0)); }

    return RCOK;
}

//
// traverse the tree in a depth-first fashion (for range query)
//
rc_t
rtree_m::_dfs_search(
    const lpid_t&	root,
    const nbox_t&	key,
    bool&		found,
    nbox_t::sob_cmp_t		ctype,
    ftstk_t&		fl)
{
    register int i;
    rtree_p page;
    lpid_t pid = root;

    if (fl.is_empty()) { found = false; return RCOK; }

    // read in the page (no lock needed, already locked)
    pid.page = fl.pop();

    W_DO( page.fix(pid, LATCH_SH) );
    slotid_t num_slot = 0;
    slotid_t count = page.nrecs();
    u_char smap[rtree_p::smap_sz];

    if (! page.is_leaf())  {
	// check for condition
	found = page.query(key, ctype, smap, num_slot);
	if (!found) {
	    // fail to find in the sub tree, search for the next
	    // W_DO( lm->unlock(page.pid()) );
	    return RCOK;
	}

	int first = -1;
	for (i=0; i<num_slot; i++) {
	    first = bm_first_set(smap, count, ++first);
	    pid.page = page.rec(first).child();
	    // lock the child pages
    	    // W_DO( lm->lock(pid, SH, t_long, WAIT_FOREVER) );
	    fl.push(pid.page); // push to the stack
	}
	
	// W_DO( lm->unlock(page.pid()) );

	found = false;
	while (!found && !fl.is_empty()) {
	    W_DO ( _dfs_search(root, key, found, ctype, fl) );
	}

	return RCOK;
    }

    //
    // leaf page
    //
    num_slot = -1; // interested in first slot found
    found = page.search(key, ctype, smap, num_slot);

    if (found) { fl.push(pid.page); }

    return RCOK;
}
	
//
// pick branch for insertion at specified level (for forced reinsert)
//
rc_t
rtree_m::_pick_branch(
    const lpid_t&	root,
    const nbox_t&	key,
    rtstk_t&		pl,
    int2		lvl,
    oper_t		oper)
{
    slotid_t slot = 0;
    lpid_t pid = root;
    rtree_p page;
    //lock_mode_t lmode = (oper == t_read) ? SH : EX;
    latch_mode_t latch_m = (oper == t_read) ? LATCH_SH : LATCH_EX;

    if (pl.is_empty()) {
	// W_DO(lm->lock(root, lmode, t_medium, WAIT_FOREVER));
	W_DO( page.fix(root, latch_m) );
	pl.push(page, -1);
    }

    // traverse through non-leaves
    while (pl.top().page.level() > lvl) {
	((rtree_p)pl.top().page).pick_optimum(key, slot); // pick optimum path
	pl.update_top(slot); // update index
	
    	// read in child page
	pid.page = pl.top().page.rec(slot).child();
	// W_DO(lm->lock(pid, lmode, t_medium, WAIT_FOREVER));
	W_DO( page.fix(pid, latch_m) );
	pid = page.pid();
        pl.push(page, -1);
    }

    return RCOK;
}

//
// overflow treatment:
//  remove some entries in the overflow page and reinsert them
//
rc_t
rtree_m::_ov_treat(
    const lpid_t&	root,
    rtstk_t&		pl,
    const nbox_t&	key,
    rtree_p&		ret_page,
    bool*		lvl_split)
{

    register int i;
    rtree_p page = pl.top().page;
    int2 level = page.level();
    w_assert3(page.pid()!=root && !lvl_split[level]);

    // get the bounding box for current page
    const rtrec_t& tuple = pl.second().page.rec(pl.second().idx);
    nbox_t bound(tuple.key(), tuple.klen());

    // forced reinsert
    rtwork_p work_page(page.pid(), page.level(), page.dim());

    W_DO( page.ov_remove(work_page.rp(), key, bound) );
    W_DO( _propagate_remove(pl, false) );
    lvl_split[level] = true;

    pl.drop_all_but_last();
    for (i=0; i<work_page.rp()->nrecs(); i++) {
	// reinsert
	W_DO ( _reinsert(root, work_page.rp()->rec(i), pl, level, lvl_split) );
	pl.drop_all_but_last();
    }

    // search for the right path for the insertion
    W_DO( _pick_branch(root, key, pl, level, t_insert) );
    ret_page = pl.top().page;
    w_assert3(ret_page.level()==level);

    return RCOK;
}

static void sweep_n_split(int axis, wrk_branch_t work[], u_char smap[],
		int& margin, int max_num, int min_num, nbox_t* extra=NULL);

//
// split one page
//
rc_t
rtree_m::_split_page(
    const lpid_t&	root,
    rtstk_t&		pl,
    const nbox_t&	key,
    rtree_p&		ret_page,
    bool*		lvl_split)
{
    register i;
    rtree_p page(pl.pop().page);
    int2 count = page.nrecs();

    wrk_branch_t* work = new wrk_branch_t[count+1];
    if (!work) return RC(eOUTOFMEMORY);
#ifdef PURIFY
    memset(work, '\0', sizeof(wrk_branch_t) * (count +1) );
#endif
    w_auto_delete_array_t<wrk_branch_t> auto_del_work(work);

    // load up work space
    for (i=0; i<count; i++) {
	 const rtrec_t& tuple = page.rec(i);
	 work[i].rect.bytes2box(tuple.key(), tuple.klen());
	 work[i].area = 0.0;
	 work[i].idx = i;
    }
    work[count].area = 0.0;
    work[count].idx = count;
    work[count].rect = key;

    int min_margin = max_int4, margin;
    u_char save_smap[rtree_p::smap_sz], smap[rtree_p::smap_sz];

    // determine which axis and where to split
    for (i=0; i<key.dimension(); i++) {
	sweep_n_split(i, work, smap, margin, count+1, 
			(int2) (count*MIN_RATIO));
	if (margin < min_margin) {
	    min_margin = margin;
	    int bytes_of_bits_to_copy = count/8 + 1;
	    memcpy(save_smap, smap, bytes_of_bits_to_copy);
	}
    }

    // create a new sibling page
    lpid_t sibling;
    W_DO( _alloc_page(root, page.level(), page, page.dim(), sibling) );
    // W_DO( lm->lock(sibling, EX, t_long, WAIT_FOREVER) );
    rtree_p sibling_p;
    W_DO( sibling_p.fix(sibling, LATCH_EX) );

    // distribute the children to the sibling page
    for (i=count-1; i>=0; i--) {
	if (bm_is_set(save_smap, i)) {
	    // shift the tuple to sibling page
	    W_DO( sibling_p.insert(page.rec(i)) ); 
	    W_DO( page.remove(i) );
	}
    }

      // re-calculate the bounding box
    nbox_t sibling_bound(page.dim()),
	   page_bound(page.dim());
    W_DO( page.calc_bound(page_bound) );
    W_DO( sibling_p.calc_bound(sibling_bound) );
    if (bm_is_set(save_smap, count)) {
	ret_page = sibling_p;
	sibling_bound += key;
    } else {
	ret_page = page;
	page_bound += key;
    }
    
    // now to adjust the higher level
    if (page.pid() == root)  {
	// split root
	// create a duplicate for root
	lpid_t duplicate;
    	W_DO( _alloc_page(root, page.level(), page, page.dim(), duplicate) );
    	// W_DO(lm->lock(duplicate, EX, t_long, WAIT_FOREVER));
    	rtree_p duplicate_p;
	W_DO( duplicate_p.fix(duplicate, LATCH_EX) );
	
	// shift all tuples in root to duplicate
	W_DO( page.shift(0, &duplicate_p) );
	W_DO( page.set_level(page.level()+1) );

	// insert the two children in
	vec_t el((void*)&sibling, 0);
	W_DO( page.insert(sibling_bound, el, sibling.page) );
	W_DO( page.insert(page_bound, el, duplicate.page) );
	
	pl.push(page, -1); // push to stack

	// release pages
	if (sibling_p != ret_page) {
	    ret_page = duplicate_p;
	    // W_DO( lm->unlock(sibling) );
	} else {
	    // W_DO( lm->unlock(duplicate) ); }
	}
    } else {

    	rtree_p parent(pl.top().page);
	int index = pl.top().idx;
	
	// replace the tuple: result of a different bounding box
	// (should use a method to change the bounding box)
	const rtrec_t& tuple = parent.rec(index);
	nbox_t old_bound(tuple.key(), tuple.klen());
	vec_t el((const void*) tuple.elem(), tuple.elen());
	shpid_t child = tuple.child();
	
	W_DO( parent.remove(index) );
	W_DO( parent.insert(page_bound, el, child) );
	
	// release the page that doesn't contain the new entry
	if (sibling_p != ret_page) {
	    // W_DO( lm->unlock(sibling) );
	} else {
	    // W_DO( lm->unlock(duplicate) ); }
	}

	// insert to the parent
	W_DO( _new_node(root, pl, sibling_bound, sibling_p, lvl_split) );
    }

    return RCOK;
}

//
// insertion of a new node into the current page
//
rc_t
rtree_m::_new_node(
    const lpid_t&	root,
    rtstk_t&		pl,
    const nbox_t&	key,
    rtree_p&		subtree,
    bool*		lvl_split)
{
    rtree_p page(pl.top().page);
    vec_t el((const void*) &subtree.pid().page, 0);
    bool split = false;

    rc_t rc = page.insert(key, el, subtree.pid().page);
    if (rc) {
	if (rc.err_num() != eRECWONTFIT) return RC_AUGMENT(rc);

	// overflow treatment
        if (page.pid()!=root && !lvl_split[page.level()]) {
	    W_DO ( _ov_treat(root, pl, key, page, lvl_split) );
	} else {
    	    W_DO( _split_page(root, pl, key, page, lvl_split) );
	    split = true;
	}

    	// insert the new tuple to parent 
	rc = page.insert(key, el, subtree.pid().page);
    	if (rc) {
	    if (rc.err_num() != eRECWONTFIT)  return RC_AUGMENT(rc);
	    w_assert1(! split);	
    	    W_DO( _split_page(root, pl, key, page, lvl_split) );
	    W_DO( page.insert(key, el, subtree.pid().page) );
	    split = true;
	}
    }

    if (page.pid() != root) {
	// propagate the changes upwards
	W_DO ( _propagate_insert(pl, false) );
    }

    // if (split) DO( lm->unlock(page.pid()) );

    return RCOK;
}

//
// propagate the insertion upwards: adjust the bounding boxes
//
rc_t
rtree_m::__propagate_insert(
    xct_t*	 	/*xd*/,
    rtstk_t&	pl
)
{
    nbox_t child_bound(pl.top().page.dim());
    
    for (register i=pl.size()-1; i>0; i--) {
	// recalculate bound for current page for next iteration
        W_DO( ((rtree_p)pl.top().page).calc_bound(child_bound) );
	// W_DO( lm->unlock(pl.top().page.pid()) );
	pl.pop();

	// find the associated entry
	int2 index = pl.top().idx;
	const rtrec_t& tuple = pl.top().page.rec(index);
	nbox_t old_bound(tuple.key(), tuple.klen());

	// if already contained, exit
	if (old_bound / child_bound || old_bound == child_bound)
	    break;
	else
	    old_bound += child_bound;

	// replace the parent entry with updated key
	vec_t el((const void*) tuple.elem(), tuple.elen());
	shpid_t child = tuple.child();
	W_DO( pl.top().page.remove(index) );
	W_DO( ((rtree_p)pl.top().page).insert(old_bound, el, child) );

    }
    return RCOK;
}

rc_t
rtree_m::_propagate_insert(
    rtstk_t&	pl,
    bool	compensate)
{
    lsn_t anchor;
    xct_t* xd = xct();
    w_assert3(xd);
    if (xd && compensate) {
	anchor = xd->anchor();
	X_DO(__propagate_insert(xd, pl), anchor);
	SSMTEST("rtree.1");
	xd->compensate(anchor);
    } else {
	W_DO(__propagate_insert(xd, pl));
    }
    
    return RCOK;
}

//
// propagate the deletion upwards: adjust the bounding boxes
//
rc_t
rtree_m::__propagate_remove(
    xct_t	*/*xd*/,
    rtstk_t&	pl
)
{
    nbox_t child_bound(pl.top().page.dim());

    for (register i=pl.size()-1; i>0; i--) {
	// recalculate bound for current page for next iteration
        W_DO( ((rtree_p)pl.top().page).calc_bound(child_bound) );
	//W_DO(lm->unlock(pl.top().page.pid()) );
	pl.pop();

	// find the associated entry
	int2 index = pl.top().idx;
	const rtrec_t& tuple = pl.top().page.rec(index);
	nbox_t key(tuple.key(), tuple.klen());
	if (key==child_bound) { break; } // no more change needed

	// remove the old entry, insert a new one with updated key
	vec_t el((const void*) tuple.elem(), tuple.elen());
	shpid_t child = tuple.child();
	W_DO( pl.top().page.remove(index) );
	W_DO( ((rtree_p)pl.top().page).insert(child_bound, el, child));
    }
    return RCOK;
}

rc_t
rtree_m::_propagate_remove(
    rtstk_t&	pl,
    bool	compensate)
{
    lsn_t anchor;
    xct_t* xd = xct();
    w_assert3(xd);
    if (xd && compensate)  {
	anchor = xd->anchor();
	X_DO(__propagate_remove(xd,pl), anchor);
	SSMTEST("rtree.2");
	xd->compensate(anchor);
    } else {
	W_DO(__propagate_remove(xd,pl));
    }

    return RCOK;
}

//
// reinsert an entry at specified level
//
rc_t
rtree_m::_reinsert(
    const lpid_t&	root,
    const rtrec_t&	tuple,
    rtstk_t&		pl,
    int2		level,
    bool*		lvl_split)
{
    nbox_t key(tuple.key(), tuple.klen());
    vec_t el((const void*) tuple.elem(), tuple.elen());
    shpid_t child = tuple.child();
    bool split = false;

    W_DO( _pick_branch(root, key, pl, level, t_insert) );
	
    rtree_p page(pl.top().page);
    w_assert3(page.level()==level);

    rc_t rc = page.insert(key, el, child);
    if (rc) {
	if (rc.err_num() != eRECWONTFIT) return RC_AUGMENT(rc);

	// overflow treatment
	split = false;
        if (page.pid()!=root && !lvl_split[level]) {
	    W_DO ( _ov_treat(root, pl, key, page, lvl_split) );
        } else {
    	    W_DO( _split_page(root, pl, key, page, lvl_split) );
	    split = true;
	}
	rc = page.insert(key, el, child);
	if (rc)  {
	    if (rc.err_num() != eRECWONTFIT) return RC_AUGMENT(rc);
	    w_assert1(! split);
    	    W_DO( _split_page(root, pl, key, page, lvl_split) );
	    W_DO( page.insert(key, el, child) );
	    split = true;
	}
    }  

    // propagate boundary change upwards
    if (!split && page.pid()!=root) {
	W_DO( _propagate_insert(pl, false) );
    }

    // if (split) DO ( lm->unlock(page.pid()) );

    return RCOK;
}

//
// create the rtree root
//
rc_t
rtree_m::create(
    stid_t	stid,
    lpid_t&	root,
    int2 	dim)
{
    lsn_t anchor;
    xct_t* xd = xct();
    w_assert3(xd);
    anchor = xd->anchor();

    X_DO( io->alloc_pages(stid, lpid_t::eof, 1, &root), anchor );

    rtree_p page;
    X_DO( page.fix(root, LATCH_EX, page.t_virgin), anchor );

    rtctrl_t hdr;
    hdr.root = root;
    hdr.level = 1;
    hdr.dim = dim;
    X_DO( page.set_hdr(hdr), anchor );

    SSMTEST("rtree.3");
    xd->compensate(anchor);

    return RCOK;
}

//
// search for exact match for key: only return the first matched
//
rc_t
rtree_m::lookup(
    const lpid_t&	root,
    const nbox_t&	key,
    void*		el,
    smsize_t&		elen,
    bool&		found )
{
    vec_t elvec;
    rtstk_t pl;

    // do an exact match search
    W_DO ( _search(root, key, elvec, found, pl, t_read) );

    if (found) {
    	const rtrec_t& tuple = pl.top().page.rec(pl.top().idx);
    	if (elen < tuple.elen())  return RC(eRECWONTFIT);
    	memcpy(el, tuple.elem(), elen = tuple.elen());
    }

    return RCOK;
}

//
// insert a <key, elem> pair
//
rc_t
rtree_m::insert(
    const lpid_t&	root,
    const nbox_t&	key,
    const cvec_t&	elem)
{
    
    rtstk_t pl;
    bool found = false, split = false;

    // search for exact match first
    W_DO( _search(root, key, elem, found, pl, t_insert) );
    if (found)  return RC(eDUPLICATE);
	
    // pick appropriate branch
    pl.drop_all_but_last();
    W_DO( _pick_branch(root, key, pl, 1, t_insert) );
    	
    rtree_p leaf(pl.top().page);
    w_assert3(leaf.is_leaf());

    rc_t rc;
    {
	/*
	 * Turning off logging makes this a critical section:
	 */
	xct_log_switch_t log_off(OFF);
	rc = leaf.insert(key, elem);
    }

    if (rc)  {
	if (rc.err_num() != eRECWONTFIT)
	    return RC_AUGMENT(rc);

	lsn_t anchor;
    	xct_t* xd = xct();
	w_assert3(xd);
    	if(xd) anchor = xd->anchor();

	// overflow treatment
	bool lvl_split[rtstk_t::max_rtstk_sz];
	for (int i=0; i<rtstk_t::max_rtstk_sz; i++) lvl_split[i] = false;
        if (leaf.pid()!=root) {
	    X_DO ( _ov_treat(root, pl, key, leaf, lvl_split), anchor );
        } else {
    	    X_DO( _split_page(root, pl, key, leaf, lvl_split), anchor );
	    split = true;
	}

	{
	    /*
	     * Turning off logging makes this a critical section:
	     */
	    xct_log_switch_t log_off(OFF);
	    rc = leaf.insert(key, elem);
	}
    	if ( rc ) {
	    if (rc.err_num() != eRECWONTFIT)  return RC_AUGMENT(rc);
	    w_assert1(! split);

    	    X_DO( _split_page(root, pl, key, leaf, lvl_split), anchor );

	    split = true;
	    {
		/*
		 * Turning off logging makes this a critical section:
		 */
	        xct_log_switch_t log_off(OFF);
		rc = leaf.insert(key, elem);
	        if ( rc ) {
		    w_assert1(rc.err_num() != eRECWONTFIT);
		    xd->release_anchor();
		    return RC_AUGMENT(rc);
	        }
	    }
	}
	if (xd) {
	    SSMTEST("rtree.4");
	    xd->compensate(anchor);
	}
    }
	
    // propagate boundary change upwards
    if (!split && leaf.pid()!=root) {
	W_DO( _propagate_insert(pl) );
    }

    // log logical insert 
    W_DO( log_rtree_insert(leaf, 0, key, elem) );

    return RCOK;
}

//
// remove a <key, elem> pair
//
rc_t
rtree_m::remove(
    const lpid_t&	root,
    const nbox_t&	key,
    const cvec_t&	elem)
{
    rtstk_t pl;
    bool found = false;

    W_DO( _search(root, key, elem, found, pl, t_remove) );
    if (! found) 
	return RC(eNOTFOUND);
    
    rtree_p leaf(pl.top().page);
    slotid_t slot = pl.top().idx;
    w_assert3(leaf.is_leaf());

    {
	/*
	 * Turning off logging makes this a critical section:
	 */
	xct_log_switch_t log_off(OFF);
    	W_DO( leaf.remove(slot) );
    }

//  W_DO( _propagate_remove(pl) );

    // log logical remove
    W_DO( log_rtree_remove(leaf, slot, key, elem) );

    return RCOK;
}

//
// print the rtree entries
//
rc_t
rtree_m::print(const lpid_t& root)
{
    rtree_p page;
    W_DO( page.fix(root, LATCH_SH) ) ;

    if (root.page == page.root()) {
        // print real root boundary
    	nbox_t bound(page.dim());
    	page.calc_bound(bound);
    	cout << "Universe:\n";
    	bound.print(5);
    }
   
    int i;
    for (i = 0; i < 5 - page.level(); i++)  { cout << "\t"; }
    cout << "LEVEL " << page.level() << ", page " 
	 << page.pid().page << ":\n";
    page.print();
    
    lpid_t sub_tree = page.pid();
    if (page.level() != 1)  {
	for (i = 0; i < page.nrecs(); i++) {
	    sub_tree.page = page.rec(i).child();
	    W_DO( print(sub_tree) );
	}
    }
    return RCOK;
}

#ifdef UNDEF
//
// print out leaf boundaries only
//
rc_t 
rtree_m::print(const lpid_t& root)
{
    rtree_p page;
    W_DO( page.fix(root, LATCH_SH) );

    if (page.level() == 1) {
        nbox_t bound(page.dim());
        page.calc_bound(bound);
        cout << endl;
        cout << page.nrecs() << endl;
        cout << bound.bound(0) << " " << bound.bound(1) << endl;
        cout << bound.bound(0) << " " << bound.bound(3) << endl;
        cout << bound.bound(2) << " " << bound.bound(3) << endl;
        cout << bound.bound(2) << " " << bound.bound(1) << endl;
        cout << endl;
    } else {
        lpid_t sub_tree = page.pid();
        for (int i = 0; i < page.nrecs(); i++) {
            sub_tree.page = page.rec(i).child();
            W_DO( print(sub_tree) );
        }
    }
    return RCOK;
}
#endif

//
// draw rtree graphically in a gremlin format
//
rc_t
rtree_m::draw(
    const lpid_t&	root,
    bool		skip)
{
    rtree_p page;
    W_DO( page.fix(root, LATCH_SH) );

    nbox_t rbound(page.dim());
    page.calc_bound(rbound);
    CoverAll = rbound;

    if ((DrawFile = fopen("graph_out", "w")) == NULL) {
	cout << " can't open gremlin file\n";
	return RC(eOS);
    }
    
    fprintf(DrawFile, "sungremlinfile\n");
    fprintf(DrawFile, "0 0.00 0.00\n");

    CoverAll.draw(page.level()+1, DrawFile, CoverAll);
    W_DO ( _draw(root, skip) );

    fprintf(DrawFile, "-1");
    fclose(DrawFile);

    return RCOK;
}

rc_t
rtree_m::_draw(
    const lpid_t&	pid,
    bool		skip)
{
    rtree_p page;
    W_DO( page.fix(pid, LATCH_SH) );

    if (!skip || page.is_leaf()) page.draw();

    lpid_t sub_tree = page.pid();
    if (page.level() != 1)  {
	for (int i = 0; i < page.nrecs(); i++) {
	    sub_tree.page = page.rec(i).child();
	    W_DO( _draw(sub_tree, skip) );
	}
    }

    return RCOK;
}

//
// collect rtree statistics: including # of entries, # of
// leaf/non-leaf pages, fill factor, overlapping degree.
//
rc_t
rtree_m::stats(
    const lpid_t&	root,
    rtree_stats_t&	stat,
    uint2		size,
    uint2*		ovp,
    bool		audit)
{
    rtree_p page;
    W_DO( page.fix(root, LATCH_SH) );
    base_stat_t ovp_sum = 0, fill_sum = 0;
    base_stat_t num_pages_store_scan = 0;

    stat.clear();

    {
  	// calculate number of pages alloc/unalloc in the rtree  
	// by scanning the store's list of pages
	lpid_t pid;
	bool allocated;
	rc_t   rc;
	rc = io->first_page(root.stid(), pid, &allocated);
	while (!rc) {
	    if (allocated) {
		num_pages_store_scan++;
	    } else {
		stat.unalloc_pg_cnt++;
	    }
	    rc = io->next_page(pid, &allocated);
	}
	w_assert3(rc);
	if (rc.err_num() != eEOF) return rc;
    }

    stat.level_cnt = page.level();

    if (size>0 && ovp) {
	ovp[0] = page.ovp();
    }

    lpid_t sub_tree = page.pid();
    if (page.level() != 1)  {
	for (int i = 0; i < page.nrecs(); i++) {
	    sub_tree.page = page.rec(i).child();
	    W_DO( _stats(sub_tree, stat, fill_sum, size, ovp) );
	    if (size>0 && ovp) {
		if (stat.level_cnt + 1 - page.level() < size) {
		    ovp_sum += ovp[stat.level_cnt - page.level() + 1];
		}
	    }
	}
	if (size>0 && ovp) {
	    if (stat.level_cnt + 1 - page.level() < size) {
		ovp_sum /= page.nrecs();
	    }
	}
    	stat.int_pg_cnt++;
	stat.fill_percent = (uint2) (fill_sum/stat.leaf_pg_cnt); 
    } else {
        stat.leaf_pg_cnt = 1;
	stat.fill_percent = (page.used_space()*100/rtree_p::data_sz); 
	stat.entry_cnt = page.nrecs();
    }

    if (audit && num_pages_store_scan != (stat.leaf_pg_cnt+stat.int_pg_cnt)) {
	// audit failed
	return RC(fcINTERNAL);
    }

    stat.unique_cnt = stat.entry_cnt;

    return RCOK;
}

rc_t
rtree_m::_stats(
    const lpid_t&	root,
    rtree_stats_t&	stat,
    base_stat_t&	fill_sum,
    uint2		size,
    uint2*		ovp)
{
    rtree_p page;
    W_DO( page.fix(root, LATCH_SH) );
    uint    ovp_sum = 0;
    
    if (ovp && (stat.level_cnt-page.level()<size)) {
	ovp[stat.level_cnt - page.level()] = page.ovp();
    }

    lpid_t sub_tree = page.pid();
    if (page.level() != 1)  {
	for (int i = 0; i < page.nrecs(); i++) {
	    sub_tree.page = page.rec(i).child();
	    W_DO( _stats(sub_tree, stat, fill_sum, size, ovp) );
	    if (ovp && (stat.level_cnt+1-page.level() < size)) {
		ovp_sum += ovp[stat.level_cnt - page.level() + 1];
	    }
	}
	if (ovp && (stat.level_cnt+1-page.level()<size)) {
	    ovp_sum /= page.nrecs();
	}
    	stat.int_pg_cnt++;
    } else {
        stat.leaf_pg_cnt += 1;
	fill_sum +=  ((uint4)page.used_space()*100/rtree_p::data_sz);
	stat.entry_cnt += page.nrecs();
    }

    return RCOK;
}
    

//
// initialize the fetch
//
rc_t
rtree_m::fetch_init(
    const lpid_t&	root,
    rt_cursor_t&	cursor)
{
    bool found = false;
    lpid_t pid = root;
    
    // push root page on the fetch stack
    if (! cursor.fl.is_empty() ) { cursor.fl.empty_all(); }
    cursor.fl.push(pid.page);

    //W_DO(lm->lock(root, SH, t_medium, WAIT_FOREVER));

    rc_t rc = _dfs_search(root, cursor.qbox, found, cursor.cond, cursor.fl);
    if (rc) {
	cursor.fl.empty_all(); 
	return RC_AUGMENT(rc);
    }

    if (!found) { return RCOK; }

    cursor.num_slot = 0;
    pid.page = cursor.fl.pop();
    W_DO( cursor.page.fix(pid, LATCH_SH) );

    w_assert3(cursor.page.is_leaf());
    found = cursor.page.search(cursor.qbox, cursor.cond, 
				    cursor.smap, cursor.num_slot);
    w_assert3(found);
    cursor.idx = bm_first_set(cursor.smap, cursor.page.nrecs(), 0);

    return RCOK;
}

//
// fetch next qualified entry
//
rc_t
rtree_m::fetch(
    rt_cursor_t&	cursor,
    nbox_t&		key,
    void*		el,
    smsize_t&		elen,
    bool&		eof,
    bool		skip)
{
    if ((eof = (cursor.page ? false : true)) )  { return RCOK; }

    // get the key and elem
    const rtrec_t& r = cursor.page.rec(cursor.idx);
    key.bytes2box(r.key(), r.klen());
    if (elen >= r.elen())  {
	memcpy(el, r.elem(), r.elen());
	elen = r.elen();
    } 
    else if (elen == 0)  { ; }
    else { return RC(eRECWONTFIT); }

    // advance the pointer
    bool found = false;
    lpid_t root;
    cursor.page.root(root);
    lpid_t pid = cursor.page.pid();

    //	move cursor to the next eligible unit based on 'condition'
    if (skip && ++cursor.idx < cursor.page.nrecs())
        cursor.idx = bm_first_set(cursor.smap, cursor.page.nrecs(),
					cursor.idx);
    if (cursor.idx == -1 || cursor.idx >= cursor.page.nrecs())  {
        // W_DO( lm->unlock(cursor.page.pid()) );
	cursor.page.unfix();

	found = false;
	while (!found && !cursor.fl.is_empty()) {
	    rc_t rc = _dfs_search(root, cursor.qbox, found, 
				  cursor.cond, cursor.fl);
            if (rc) {
	        cursor.fl.empty_all();
		return RC_AUGMENT(rc);
	    }
        }
	if (!found) {
	    return RCOK; 
	} else {
    	    pid.page = cursor.fl.pop();
    	    W_DO( cursor.page.fix(pid, LATCH_SH) );
    	    w_assert3(cursor.page.is_leaf());
    	    found = cursor.page.search(cursor.qbox, cursor.cond, 
				           cursor.smap, cursor.num_slot);
	    w_assert3(found);
	    cursor.idx = bm_first_set(cursor.smap, cursor.page.nrecs(), 0);
	}
    }
	
    return RCOK;
}

//
// optimal split along one axis: sweep along one axis to find the split line.
//
static void 
sweep_n_split(int axis, wrk_branch_t work[], u_char smap[], int& margin,
    		int max_num, int min_num, nbox_t* extra)
{
    register i,j;
    margin = 0;
    if (min_num == 0) min_num = (int) (MIN_RATIO*max_num + 0.5);
    int split = -1, diff = max_num - min_num;

    // sort along the specified axis
    quick_sort(work, max_num, axis);

    bm_zero(smap, max_num);
    bm_set(smap, work[0].idx);

    nbox_t box1(work[0].rect);
    for (i=1; i<min_num; i++) {
	box1 += work[i].rect;
	bm_set(smap, work[i].idx);
    }

    nbox_t box2_base(work[max_num-1].rect);
    for (i=diff+1; i<max_num-1; i++)
	box2_base += work[i].rect;

    split = min_num;
    double overlap;
    double bound_area = 0;
    double min_ovp = MaxDouble, min_area = MaxDouble;

    // calculate margin and overlap for each distribution
    for (i = min_num; i <= max_num-min_num; i++) {
	nbox_t box2(box2_base);
	for (j=i; j<=diff; j++)
	    box2 += work[j].rect;

	margin += (box1.margin()+box2.margin());
	overlap = (box1^box2).area();
	if (extra) {
	    overlap += (box1^(*extra)).area();
	    overlap += (box2^(*extra)).area();
        }

	if (overlap < min_ovp) {
	    min_ovp = overlap;
	    min_area = (box1+box2).area();
	    split = i;
	} else if (overlap == min_ovp && 
		 (bound_area = (box1+box2).area()) < min_area) {
	    min_area = bound_area;
	    split = i;
	}

	box1 += work[i].rect;
    }

    for (i = min_num; i < split; i++)
	bm_set(smap, work[i].idx);
}


void 
rtld_cache_t::init_buf(int2 lvl) 
{
    buf[0].init(lvl);
    buf[1].init(lvl);
    buf[2].init(lvl);
}

//
// force one page out of the load cache: repacking all cached
// pages and flushing the first one.
//
rc_t
rtld_cache_t::force(
    rtwork_p&	ret_p,
    bool&	out,
    nbox_t*	universe)
{
    register i,j;
    out = false;

    if (_idx==0) return RCOK;

    // count the size of all entries
    int2 size = 0, cnt = 0;
    for (i=0; i<=1; i++) {
	size += buf[i].rp()->used_space();
	cnt += buf[i].rp()->nrecs();
    }

    // all tuples fits on one page, compress them to one page
    if (size < rtree_p::data_sz) {
	for (i=1; i>0; i--) {
	    for (j=buf[i].rp()->nrecs()-1; j>=0; j--)  {
		W_DO( buf[0].rp()->insert(buf[i].rp()->rec(j)) );
		W_DO( buf[1].rp()->remove(j) );
	    }
        }

        W_DO( buf[0].calc_set_bound() );
	W_DO( buf[1].calc_set_bound() );

        if (_idx==2) { buf[1].swap(buf[2]); }
	_idx--;
	return RCOK;
    }

    ret_p.init(buf[0].rp()->level());

    // check if split is necessary
    if ((buf[0].bound()^buf[1].bound()).area()/buf[0].bound().area()
	< 0.01 || universe==NULL) {
	// no need to apply split algorithm
	ret_p.swap(buf[0]); 
	buf[0].swap(buf[1]);
	if (_idx==2) buf[1].swap(buf[2]);
	_idx--;
	out = true;
	if (!last_box) {
	    last_box = new nbox_t(ret_p.bound());
	    w_assert1(last_box);
        } else {
	    *last_box = ret_p.bound();
        }
	return RCOK;
    }
	

    // load up work space
    wrk_branch_t* work = new wrk_branch_t[cnt];
    if (!work) return RC(smlevel_0::eOUTOFMEMORY);
    w_auto_delete_array_t<wrk_branch_t> auto_del_work(work);

    int cnt1 = buf[0].rp()->nrecs(), index = 0;
    for (i=0; i<=1; i++) {
	for (j=0; j<buf[i].rp()->nrecs(); j++) {
	    const rtrec_t& tuple = buf[i].rp()->rec(j);
	    work[index].rect.bytes2box(tuple.key(), tuple.klen());
	    work[index].idx = index;
	    index++;
	}
    }

    int margin;
    u_char smap[rtree_p::smap_sz*2];
    int min_cnt = cnt - rtree_p::data_sz*buf[0].rp()->nrecs()
			/ buf[0].rp()->used_space();
    min_cnt = (min_cnt + cnt/2) / 2;

    //decide which axis to split
    int x0 = buf[0].bound().center(0);
    int x1 = buf[1].bound().center(0);
    int y0 = buf[0].bound().center(1);
    int y1 = buf[1].bound().center(1);
    bool bounce_x = false, bounce_y = false;

    if (_idx == 2) {
	int x2 = buf[2].bound().center(0);
    	int y2 = buf[2].bound().center(1);

	if ((x1-x2)*(x1-x0) > 0) bounce_x = true;
	if ((y1-y2)*(y1-y0) > 0) bounce_y = true;
    }

    // split on the desired axis
    if (bounce_x && bounce_y) 
	if (ABS(x1-x0) >= ABS(y1-y0)) 
	    sweep_n_split(1, work, smap, margin, cnt, min_cnt, last_box);
	else 
	    sweep_n_split(0, work, smap, margin, cnt, min_cnt, last_box);
    else if (bounce_x)
	sweep_n_split(1, work, smap, margin, cnt, min_cnt, last_box);
    else if (bounce_y)
	sweep_n_split(0, work, smap, margin, cnt, min_cnt, last_box);
    else {
	if (ABS(x1-x0) >= ABS(y1-y0)) 
	    sweep_n_split(0, work, smap, margin, cnt, min_cnt, last_box);
	else 
	    sweep_n_split(1, work, smap, margin, cnt, min_cnt, last_box);
    }

    // shift tuples from buf[0] and buf[1] to the out page
    for (i=cnt-1; i>=0; i--) {
	if (bm_is_set(smap, i)) {
	    int idx = (i>=cnt1)? 1: 0;
	    int offset = (i>=cnt1)? i - cnt1: i;
	    W_DO( ret_p.rp()->insert(buf[idx].rp()->rec(offset)) );
	    W_DO( buf[idx].rp()->remove(offset) );
	}
    }
    
    // shift rest from buf[1] to buf[0]
    for (i=buf[1].rp()->nrecs()-1; i>=0; i--) {
	W_DO( buf[0].rp()->insert(buf[1].rp()->rec(i)) );
	W_DO( buf[1].rp()->remove(i) );
    }

    W_DO( buf[0].calc_set_bound() );
    W_DO( buf[1].calc_set_bound() );
    W_DO( ret_p.calc_set_bound() );
    
    // decide wich one goes out first
    if (_idx == 2) {
	if (universe)
    	    if (ret_p.bound().hcmp(buf[0].bound(), *universe) > 0)
	        ret_p.swap(buf[0]); 
        else if ((ret_p.bound()^buf[2].bound()).area()
	     	> (buf[0].bound()^buf[2].bound()).area())
	        ret_p.swap(buf[0]); 
    	buf[1].swap(buf[2]);
    }
	
    _idx--;
    out = true;
    if (!last_box) {
	last_box = new nbox_t(ret_p.bound());
    } else {
	*last_box = ret_p.bound();
    }

    return RCOK;
}

//
// shift tuples from temporal work page to persistent page
// an image log is generated.
//
rc_t
rtld_stk_t::tmp2real(
    rtwork_p*	tmp,
    rtree_p*	real)
{
    {
    	xct_log_switch_t toggle(smlevel_0::OFF);

    	// shift all tuples in tmp to real
    	for (register i=0; i<tmp->rp()->nrecs(); i++) {
	    W_DO( real->insert(tmp->rp()->rec(i)) );
    	}
    }

    // generate an image log
    {
    	xct_log_switch_t toggle(smlevel_0::ON);
    	W_DO( log_page_image(*real) );
    }
    return RCOK;
}

//
// heuristic table for fill factor and expansion
//
static const int _h_size = 20;	// expansion factors for 20 fills
static int expn_table[_h_size] = 
{  1636, 1124, 868, 612, 484, 
    356, 292, 228, 196, 164,
    148, 132, 124, 116, 108,
    104, 102, 101, 101, 100
};

//
// determine when to terminate inserting to the current page based
// on fill and expansion factor: if the page reached certain fill,
// and the expansion factor is over certain threshold, then stop
// insertion to the current page.
//
static bool
heuristic_cut(
    rtwork_p*		page,
    const nbox_t&	key)
{
    int offset = (page->rp()->used_space()*20/rtree_p::data_sz);
    if (offset <= 4) return false;
    else if ((page->bound()+key).area() >
	     page->bound().area()*expn_table[offset-1]/100.0)
	return true;
    else 
	return false;
}
    

//
// add an entry to the load stack:
//	If enough space in the current cache page, insert and exit.
//	Otherwise, force one cache page to disk, record the change
//	at the higher level recursively and then insert the new entry.
//
rc_t
rtld_stk_t::add_to_stk(
    const nbox_t&	key,
    const cvec_t&	el,
    shpid_t		child,
    int2		level)
{
    w_assert1(level<=_top+1);
    if (level == _top+1) { init_next_layer(); }

    // cache page at current top layer
    rtwork_p* page = layers[level].top();
    
    if (dc.h) {
	// check heuristics 
	if (heuristic_cut(page, key)) { 
	    // fill factor and expansion factor reached thresholds,
	    // skip to perform the force and the insert.
	} else {
	    rc_t rc = page->rp()->insert(key, el, child);
	    if (rc) {
		// if page full, go to perform the force and the insert. 
		if (rc.err_num() != smlevel_0::eRECWONTFIT) {
		    return RC_AUGMENT(rc);
		}
	    } else { 
	    // insert successful, record bounding box change on cache page
		page->expn_bound(key);
		return RCOK; 
	    }
	}
    } else {
	rc_t rc = page->rp()->insert(key, el, child);
	if (rc) {
	    // if page full, go to perform the force and the insert. 
	    if (rc.err_num() != smlevel_0::eRECWONTFIT) {
		return RC_AUGMENT(rc);
	    }
	} else { 
	    // insert successful, record bounding box change on cache page
	    page->expn_bound(key);
	    return RCOK;
	}
    }

    if (layers[level].count()==3) {
        // cache full, force one cache page out to disk
	rtwork_p out_page;
	bool out=false;

	W_DO( layers[level].force(out_page, out, dc.universe) );
	if (out) {
	    lpid_t npid;
	    vec_t e;

	    // write one cache page to disk page
	    W_DO( rtree_m::_alloc_page(rp.pid(), level+1,
				       rp, rp.dim(), npid) );


	    rtree_p np;
	    W_DO( np.fix(npid, LATCH_EX) );
	    W_DO( tmp2real(&out_page, &np) );

	    num_pages++;
	    if (level==0) { 
		leaf_pages++;
		fill_sum += (uint4) (np.used_space()*100/rtree_p::data_sz);
	    }

	    // insert to the higher level
	    W_DO( add_to_stk(out_page.bound(), e, npid.page, level+1) );

	}
    }

    // get the next empty cache page and insert the new entry
    layers[level].incr_cnt();
    W_DO( layers[level].top()->rp()->insert(key, el, child) );
    layers[level].top()->expn_bound(key);

    return RCOK;
}

//
// forcing out all remaining pages in the load cache at the end
//
rc_t
rtld_stk_t::wrap_up()
{
    rtwork_p out_page;
    bool out;
    lpid_t npid;	
    rtree_p np;
    vec_t e;
    
    // examine all non-root layers
    for (register i=0; i<_top; i++) {
	do {
	    out = false;
	    while (!out && layers[i].count() > 1) {
		W_DO( layers[i].force(out_page, out, dc.universe) );
	    }
	    if (out) {
	    	// write each cache page to disk page
	    	W_DO( rtree_m::_alloc_page(rp.pid(), i+1, 
					rp, rp.dim(), npid) );
	    	W_DO( np.fix(npid, LATCH_EX) );
	    	W_DO( tmp2real(&out_page, &np) );
	    
	    	num_pages++;
	    	if (i==0) {
		    leaf_pages++;
		    fill_sum += (uint4)(np.used_space()*100/rtree_p::data_sz);
		}

	    	// insert to the higher level
	    	W_DO( add_to_stk(out_page.bound(), e, npid.page, i+1) );

	    }
        } while (layers[i].count() > 1);

	// process the final cache page in the layer
	W_DO(rtree_m::_alloc_page(rp.pid(), i+1, rp, rp.dim(), npid));
	W_DO( np.fix(npid, LATCH_EX) );
	W_DO( tmp2real(layers[i].bottom(), &np) );
	    
	num_pages++;
	if (i==0) { 
	    leaf_pages++;
	    fill_sum += (uint4) (np.used_space()*100/rtree_p::data_sz);
	}
	
	// insert to the higher level
	W_DO( add_to_stk(layers[i].bottom()->bound(), e, npid.page, i+1) );
    }

    // now deal with the root layer
    out = false;
    while (!out && layers[_top].count() > 1) {
	W_DO( layers[_top].force(out_page, out, dc.universe) );
    }

    if (out) {
	// more than one cache pages: 
	//	means real root is one layer above
	num_pages++;
        W_DO( rp.set_level(_top+2) );
	rtree_p np;

	do {
	    // write each cache page to disk page
	    W_DO( rtree_m::_alloc_page(rp.pid(),_top+1,rp,rp.dim(),npid) );
	    W_DO( np.fix(npid, LATCH_EX) );
	    W_DO( tmp2real(&out_page, &np) );
	    	
	    num_pages++;
	    if (_top==0) {
		leaf_pages++;
	    	fill_sum += (uint4) (np.used_space()*100/rtree_p::data_sz);
	    }

	    W_COERCE( rp.insert(out_page.bound(), e, np.pid().page) );

	    out = false;
	    while (!out && layers[_top].count() > 1) {
		W_DO( layers[_top].force(out_page, out, dc.universe) );
	    }
        } while (out);

	// process the final cache page
	W_DO( rtree_m::_alloc_page(rp.pid(), _top+1, rp, rp.dim(), npid) );
	W_DO( np.fix(npid, LATCH_EX) );
	W_DO( tmp2real(layers[_top].bottom(), &np) );
	W_COERCE( rp.insert(layers[_top].bottom()->bound(),
			    e, np.pid().page) );
	num_pages++;
	if (_top==0) {
	    leaf_pages++;
	    fill_sum += (uint4) (np.used_space()*100/rtree_p::data_sz);
	}
	height = _top+2;

    } else {

	// directly copy to root page
        W_DO( rp.set_level(_top+1) );
	W_DO( tmp2real(layers[_top].bottom(), &rp) );
	num_pages++;
	if (_top==0) {
	    leaf_pages++;
	    fill_sum += (uint4) (rp.used_space()*100/rtree_p::data_sz);
	}
	height = _top+1;
    }

    return RCOK;
}

#undef SM_LEVEL
#include <sm_int_4.h>
//
// bulk load: the src file should be already sorted in spatial order.
//
rc_t
rtree_m::bulk_load(
    const lpid_t& root,		// I- root of rtree
    stid_t src, 		// I- store containing new records
    const rtld_desc_t& desc,	// I- load descriptor
    rtree_stats_t& stats)	// O- index stats
{
    stats.clear();
    if (!is_empty(root)) {
	 return RC(eNDXNOTEMPTY);
    }

    lsn_t anchor;
    xct_t* xd = xct();
    w_assert3(xd);
    if (xd) anchor = xd->anchor();

    lpid_t pid;
    rtree_p rp;
    X_DO( rp.fix(root, LATCH_EX), anchor );
    rtld_stk_t ld_stack(rp, desc);

    nbox_t key(rp.dim());
    int klen = key.klen();

    /*
     *  go thru the file page by page
     */
    int i = 0;              // toggle
    file_p page[2];         // page[i] is current page
    const record_t* pr = 0; // previous record
    base_stat_t cnt=0, uni_cnt=0;

    X_DO( fi->first_page(src, pid), anchor );
    for (bool eof = false; ! eof; ) {
	X_DO( page[i].fix(pid, LATCH_SH), anchor );
	for (slotid_t s = page[i].next_slot(0); s; s = page[i].next_slot(s)) {
	    const record_t* r;
	    W_COERCE( page[i].get_rec(s, r) );

	    key.bytes2box(r->hdr(), klen);
	    vec_t el(r->body(), (int)r->body_size());

	    ++cnt;
	    if (!pr) ++ uni_cnt;
	    else {
		// check unique
		if (memcmp(pr->hdr(), r->hdr(), klen)) 
		    ++ uni_cnt;
	    }

	    X_DO( ld_stack.add_to_stk(key, el, 0, 0), anchor );
	    pr = r;
        }
	i = 1 - i;
	X_DO( fi->next_page(pid, eof), anchor );
    }

    X_DO( ld_stack.wrap_up(), anchor );

    if (xd)  {
	SSMTEST("rtree.5");
	xd->compensate(anchor);
    }

    stats.entry_cnt = cnt;
    stats.unique_cnt = uni_cnt;
    stats.level_cnt = ld_stack.height;
    stats.leaf_pg_cnt = ld_stack.leaf_pages;
    stats.int_pg_cnt = ld_stack.num_pages - stats.leaf_pg_cnt;
    stats.fill_percent = (uint2) (ld_stack.fill_sum/ld_stack.leaf_pages);

    return RCOK;
}

//
// bulk load: the stream should be already sorted in spatial order
//
rc_t
rtree_m::bulk_load(
    const lpid_t& root,		// I- root of rtree
    sort_stream_i& sorted_stream,// IO - sorted stream
    const rtld_desc_t& desc,	// I- load descriptor
    rtree_stats_t& stats)	// O- index stats
{
    memset(&stats, 0, sizeof(stats));
    if (!is_empty(root)) {
	 return RC(eNDXNOTEMPTY);
    }

    /*
     *  go thru the sorted stream
     */
    uint4 cnt=0, uni_cnt=0;

    {
    	rtree_p rp;
	W_DO( rp.fix(root, LATCH_EX) );
    	rtld_stk_t ld_stack(rp, desc);

    	nbox_t box(rp.dim());
    	int klen = box.klen();

    	char* tmp = new char[klen];
	w_auto_delete_array_t<char> auto_del_tmp(tmp);

    	char* prev_tmp = new char[klen];
	w_auto_delete_array_t<char> auto_del_prev_tmp(prev_tmp);
	bool prev = false;
	bool eof;
	vec_t key, el;
	W_DO ( sorted_stream.get_next(key, el, eof) );

	while (!eof) {
	    ++cnt;

	    key.copy_to(tmp, klen);
	    box.bytes2box(tmp, klen);

	    if (!prev) {
		prev = true;
		memcpy(prev_tmp, tmp, klen);
		uni_cnt++;
	    } else {
		if (memcmp(prev_tmp, tmp, klen)) {
		    uni_cnt++;
		    memcpy(prev_tmp, tmp, klen);
		}
	    }

	    W_DO( ld_stack.add_to_stk(box, el, 0, 0) );
	    key.reset();
	    el.reset();
	    W_DO ( sorted_stream.get_next(key, el, eof) );
	}
    
	W_DO( ld_stack.wrap_up() );

	stats.level_cnt = ld_stack.height;
	stats.leaf_pg_cnt = ld_stack.leaf_pages;
	stats.int_pg_cnt = ld_stack.num_pages - stats.leaf_pg_cnt;
	stats.fill_percent = (uint2) (ld_stack.fill_sum / ld_stack.leaf_pages);
    }

    stats.entry_cnt = cnt;
    stats.unique_cnt = uni_cnt;
    return RCOK;
}
