/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: btree_bl.cc,v 1.4 1997/06/15 03:14:22 solomon Exp $
 */
#define SM_SOURCE
#define BTREE_C

#include "sm_int_4.h"
// need stored streams
#include "sm.h"

#if defined(USE_OLD_BTREE_IMPL)
#include "btree_p.old.h"
#else
#include "btree_p.h"
#endif
#include "btree_impl.h"
#include "sort.h"
#include "sm_du_stats.h"
#include <debug.h>
#include <crash.h>

#ifdef __GNUG__
template class w_auto_delete_array_t<record_t *>; 
#endif

/*********************************************************************
 *
 *  class btsink_t
 *
 *  Manages bulk load of a btree. 
 *
 *  User calls put() to add <key,el> into the sink, and then
 *  call map_to_root() to map the current root to the original 
 *  root page of the btree, and finalize the bulkload.
 *
 *  CC Note: 
 *	Btree must be locked EX.
 *  Recovery Note: 
 *	Logging is turned off during insertion into the btree.
 *	However, as soon as a page fills up, a physical 
 *	page image log is generated for the page.
 *
 *********************************************************************/
class btsink_t : private btree_m {
public:
    NORET	btsink_t(const lpid_t& root, rc_t& rc);
    NORET	~btsink_t() {};
    
    rc_t	put(const cvec_t& key, const cvec_t& el);
    rc_t	map_to_root();

    uint2	height()	{ return _height; }
    uint4	num_pages()	{ return _num_pages; }
    uint4	leaf_pages()	{ return _leaf_pages; }
private:
    uint2	_height;	// height of the tree
    uint4	_num_pages;	// total # of pages
    uint4	_leaf_pages;	// total # of leaf pages

    lpid_t	_root;		// root of the btree
    btree_p	_page[20];	// a stack of pages (path) from root
				// to leaf
    slotid_t	_slot[20];	// current slot in each page of the path
    shpid_t	_left_most[20];	// id of left most page in each level
    int 	_top;		// top of the stack

    rc_t	_add_page(int i, shpid_t pid0);
};

/*********************************************************************
 *
 *  btree_impl::_handle_dup_keys(sink, slot, prevp, curp, count, r, pid)
 *
 *  context: called during bulk load
 *
 *  assumptions: entire btre is locked EX, so there's no concurrency
 *
 *
 *********************************************************************/
rc_t
btree_impl::_handle_dup_keys(
    btsink_t*           sink,           // IO- load stack
    slotid_t&           slot,           // IO- current slot
    file_p*             prevp,          // I-  previous page
    file_p*             curp,           // I-  current page
    int&                count,          // O-  number of duplicated keys
    record_t*&  	r,              // O-  last record
    lpid_t&             pid,            // IO- last pid
    int			nkc,
    const key_type_s*	kc)
{
    count = 0;
    int max_rec_cnt = 500;

    record_t** recs = new record_t* [max_rec_cnt];
    if (!recs)  { return RC(eOUTOFMEMORY); }
    w_auto_delete_array_t<record_t*> auto_del_recs(recs);

    bool eod = false, eof = false;
    register i;

    if (slot==1) {
        // previous rec is on the previous page
        W_COERCE( prevp->get_rec(prevp->num_slots()-1, r) );
    } else {
        W_COERCE( curp->get_rec(slot-1, r) );
    }
    recs[count++] = r;

    W_COERCE( curp->get_rec(slot, r) );
    recs[count++] = r;

    slotid_t s = slot;
    while ((s = curp->next_slot(s)))  {
        W_COERCE( curp->get_rec(s, r) );
        if (r->hdr_size() == recs[0]->hdr_size() &&
            !memcmp(r->hdr(), recs[0]->hdr(), r->hdr_size())) {
            if (r->body_size() == recs[0]->body_size() &&
                !memcmp(r->body(), recs[0]->body(), (int)r->body_size())) {
                return RC(eDUPLICATE);
            }
            if (count == max_rec_cnt) {
                max_rec_cnt *= 2;
                record_t** tmp = new record_t* [max_rec_cnt];
		if (!tmp)  { return RC(eOUTOFMEMORY); }
                memcpy(tmp, recs, count*sizeof(recs));
                delete [] recs;
                recs = tmp;
		auto_del_recs.set(recs);
            }
            recs[count++] = r;
        } else {
            eod = true;
            break;
        }
    }

    int page_cnt = 0, max_page_cnt = 100;
    file_p* pages = new file_p[max_page_cnt];
    if (!pages)  { return RC(eOUTOFMEMORY); }
    w_auto_delete_array_t<file_p> auto_del_pages(pages);

    if (!eod)  {
        W_DO( fi->next_page(pid, eof) );
    }

    while (!eof && !eod) {
        W_DO( pages[page_cnt].fix(pid, LATCH_SH) );
	s = 0;
	while ((s = pages[page_cnt].next_slot(s)))  {
            W_COERCE( pages[page_cnt].get_rec(s, r) );

            if (r->hdr_size() == recs[0]->hdr_size() &&
               !memcmp(r->hdr(), recs[0]->hdr(), r->hdr_size())) {
                if (r->body_size() == recs[0]->body_size() &&
		    !memcmp(r->body(), recs[0]->body(), (int)r->body_size())) {
                    return RC(eDUPLICATE);
                }
                if (count==max_rec_cnt) {
                    max_rec_cnt *= 2;
                    record_t** tmp = new record_t* [max_rec_cnt];
		    if (!tmp)  { return RC(eOUTOFMEMORY); }
                    memcpy(tmp, recs, count*sizeof(recs));
                    delete [] recs;
                    recs = tmp;
		    auto_del_recs.set(recs);
                }
                recs[count++] = r;
            } else {
                eod = true;
                break;
            }
        }
        page_cnt++;
        if (!eod) {
            W_DO( fi->next_page(pid, eof) );
            if (page_cnt >= max_page_cnt) {
		// BUGBUG: panic, too many duplicate key entries
		W_FATAL(eINTERNAL);
	    }
        }
    }

    // sort the recs : use selection sort
    for (i = 0; i < count - 1; i++) {
        for (register j = i + 1; j < count; j++) {
            vec_t el(recs[i]->body(), (int)recs[i]->body_size());
            if (el.cmp(recs[j]->body(), (int)recs[j]->body_size()) > 0) {
                record_t* tmp = recs[i];
                recs[i] = recs[j];
                recs[j] = tmp;
            }
        }
    }

//  qsort(recs, count, sizeof(void*), elm_cmp);

    vec_t key(recs[0]->hdr(), recs[0]->hdr_size());
    for (i = 0; i < count; i++) {
        cvec_t el(recs[i]->body(), (int)recs[i]->body_size());
	cvec_t* real_key=0;
	W_DO(btree_m::_scramble_key(real_key, key, nkc, kc));
        W_DO( sink->put(*real_key, el) );
    }

    if (page_cnt>0) {
        *curp = pages[page_cnt-1];
    }
    slot = s;

    if (eof) pid = lpid_t::null;

    return RCOK;
}

/*********************************************************************
 *
 *  btree_m::purge(root, check_empty)
 *
 *  Remove all pages of a btree except the root. "Check_empty" 
 *  indicates whether to check if the tree is empty before purging.
 #  NOT USED 
 *
 *********************************************************************/
rc_t
btree_m::purge(
    const lpid_t& 	root,		// I-  root of btree
    bool		check_empty)
{
    if (check_empty)  {
	bool flag;
	W_DO( is_empty(root, flag) );
	if (! flag)  {
	    return RC(eNDXNOTEMPTY);
	}
    }

    lsn_t anchor;
    xct_t* xd = xct();
    w_assert3(xd);
    anchor = xd->anchor();

    lpid_t pid;
    X_DO( io->first_page(root.stid(), pid), anchor );
    while (pid.page)  {
	/*
	 *  save current pid, get next pid, free current pid.
	 */
	lpid_t cur = pid;
	rc_t rc = io->next_page(pid);
	if (cur.page != root.page)  {
	    X_DO( io->free_page(cur), anchor );
	}
	if (rc)  {
	    if (rc.err_num() != eEOF)  {
		xd->release_anchor();
		return RC_AUGMENT(rc);
	    }
	    break;
	}
    }

    btree_p page;
    X_DO( page.fix(root, LATCH_EX), anchor );
    X_DO( page.set_hdr(root.page, 1, 0, 0), anchor );

    SSMTEST("btree.bulk.3");
    xd->compensate(anchor);
    
    W_COERCE( log_btree_purge(page) );

    return RCOK;
}


/*********************************************************************
 *
 *  btree_m::bulk_load(root, src, unique, cc, stats)
 *
 *  Bulk load a btree at root using records from store src.
 *  The key and element of each entry is stored in the header and
 *  body part, respectively, of records from src store. 
 *  NOTE: src records must be sorted in ascending key order.
 *
 *  Statistics regarding the bulkload is returned in stats.
 *
 *********************************************************************/
rc_t
btree_m::bulk_load(
    const lpid_t&	root,		// I-  root of btree
    stid_t		src,		// I-  store containing new records
    int			nkc,
    const key_type_s*	kc,
    bool		unique,		// I-  true if btree is unique
    concurrency_t	cc_unused,	// I-  concurrency control mechanism
    btree_stats_t&	stats)		// O-  index stats
{
    w_assert1(kc && nkc > 0);

    // keep compiler quiet about unused parameters
    if (cc_unused) {}

    /*
     *  Set up statistics gathering
     */
    stats.clear();
    base_stat_t uni_cnt = 0;
    base_stat_t cnt = 0;

    /*
     *  Btree must be empty for bulkload.
     */
    W_DO( purge(root, true) );
	
    /*
     *  Create a sink for bulkload
     */
    rc_t rc;
    btsink_t sink(root, rc);
    if (rc) return RC_AUGMENT(rc);

    /*
     *  Go thru the src file page by page
     */
    int i = 0;		// toggle
    file_p page[2];		// page[i] is current page

    const record_t* pr = 0;	// previous record
    lpid_t pid;
    bool eof = false;
    bool skip_last = false;
    for (rc = fi->first_page(src, pid);
	 !rc.is_error() && !eof;
	  rc = fi->next_page(pid, eof))     {
	/*
	 *  for each page ...
	 */
	W_DO( page[i].fix(pid, LATCH_SH) );
	w_assert3(page[i].pid() == pid);

	slotid_t s = page[i].next_slot(0);
	if (! s)  {
	    /*
	     *  do nothing. skip over empty page, so do not toggle
	     */
	    continue;
	} 
	for ( ; s; s = page[i].next_slot(s))  {
	    /*
	     *  for each slot in page ...
	     */
	    record_t* r;
	    W_COERCE( page[i].get_rec(s, r) );

	    if (pr) {
		cvec_t key(pr->hdr(), pr->hdr_size());
		cvec_t el(pr->body(), (int)pr->body_size());

		/*
		 *  check uniqueness and sort order
		 */
		if (key.cmp(r->hdr(), r->hdr_size()))  {
		    /*
		     *  key of r is greater than the previous key
		     */
		    ++cnt;
		    cvec_t* real_key = 0;
		    W_DO(_scramble_key(real_key, key, nkc, kc));
		    W_DO( sink.put(*real_key, el) );
		    skip_last = false;
		} else {
		    /*
		     *  key of r is equal to the previous key
		     */
		    if (unique) {
			return RC(eDUPLICATE);
		    }

		    /*
		     * temporary hack for duplicated keys:
		     *  sort the elem in order before loading
		     */
		    int dup_cnt;
		    W_DO( btree_impl::_handle_dup_keys(&sink, s, &page[1-i],
					   &page[i], dup_cnt, r, pid,
					   nkc, kc) );
		    cnt += dup_cnt;
		    eof = (pid==lpid_t::null);
		    skip_last = eof ? true : false;
		} 

		++uni_cnt;
	    }

	    if (page[1-i])  page[1-i].unfix();
	    pr = r;

	    if (!s) break;
	}
	i = 1 - i;	// toggle i
	if (eof) break;
    }

    if (rc)  {
	return rc.reset();
    }

    if (!skip_last && pr) {
	cvec_t key(pr->hdr(), pr->hdr_size());
	cvec_t el(pr->body(), (int)pr->body_size());
	cvec_t* real_key;
	W_DO(_scramble_key(real_key, key, nkc, kc));
	W_DO( sink.put(*real_key, el) );
	++uni_cnt;
	++cnt;
    }

    if (pr) {
    	W_DO( sink.map_to_root() );
    }

    stats.level_cnt = sink.height();
    stats.leaf_pg_cnt = sink.leaf_pages();
    stats.int_pg_cnt = sink.num_pages() - stats.leaf_pg_cnt;

    stats.leaf_pg.unique_cnt = uni_cnt;
    stats.leaf_pg.entry_cnt = cnt;

    return RCOK;
}


/*********************************************************************
 *
 *  btree_m::bulk_load(root, sorted_stream, unique, cc, stats)
 *
 *  Bulk load a btree at root using records from sorted_stream.
 *  Statistics regarding the bulkload is returned in stats.
 *
 *********************************************************************/
rc_t
btree_m::bulk_load(
    const lpid_t&	root,		// I-  root of btree
    sort_stream_i&	sorted_stream,	// IO - sorted stream	
    int			nkc,
    const key_type_s*	kc,
    bool		unique,		// I-  true if btree is unique
    concurrency_t	cc_unused,	// I-  concurrency control
    btree_stats_t&	stats)		// O-  index stats
{
    w_assert1(kc && nkc > 0);

    // keep compiler quiet about unused parameters
    if (cc_unused) {}

    /*
     *  Set up statistics gathering
     */
    stats.clear();
    base_stat_t uni_cnt = 0;
    base_stat_t cnt = 0;

    /*
     *  Btree must be empty for bulkload
     */
    W_DO( purge(root, true) );

    /*
     *  Create a sink for bulkload
     */
    rc_t rc;
    btsink_t sink(root, rc);
    if (rc) return RC_AUGMENT(rc);

    /*
     *  Allocate space for storing prev keys
     */
    char* tmp = new char[page_s::data_sz];
    if (! tmp)  {
	return RC(eOUTOFMEMORY);
    }
    w_auto_delete_array_t<char> auto_del_tmp(tmp);
    vec_t prev_key(tmp, page_s::data_sz);

    /*
     *  Go thru the sorted stream
     */
    bool pr = false;	// flag for prev key
    bool eof = false;

    vec_t key, el;
    W_DO ( sorted_stream.get_next(key, el, eof) );

    while (!eof) {
	++cnt;

	if (! pr) {
	    ++uni_cnt;
	    pr = true;
	    prev_key.copy_from(key);
	    prev_key.reset().put(tmp, key.size());
	} else {
	    // check unique
	    if (key.cmp(prev_key))  {
		++uni_cnt;
		prev_key.reset().put(tmp, page_s::data_sz);
		prev_key.copy_from(key);
		prev_key.reset().put(tmp, key.size());
	    } else {
		if (unique) {
		    return RC(eDUPLICATE);
		}
		// BUGBUG: need to sort elems for duplicate keys
		return RC(eNOTIMPLEMENTED);
	    }
	}

	cvec_t* real_key;
	W_DO(_scramble_key(real_key, key, nkc, kc));
	W_DO( sink.put(*real_key, el) ); 
	key.reset();
	el.reset();
	W_DO ( sorted_stream.get_next(key, el, eof) );
    }

    W_DO( sink.map_to_root() );
	
    stats.level_cnt = sink.height();
    stats.leaf_pg_cnt = sink.leaf_pages();
    stats.int_pg_cnt = sink.num_pages() - stats.leaf_pg_cnt;

    sorted_stream.finish();

    stats.leaf_pg.unique_cnt = uni_cnt;
    stats.leaf_pg.entry_cnt = cnt;

    return RCOK;
}


/*********************************************************************
 *
 *  btsink_t::btsink_t(root_pid, rc)
 *
 *  Construct a btree sink for bulkload of btree rooted at root_pid. 
 *  Any errors during construction in returned in rc.
 *
 *********************************************************************/
btsink_t::btsink_t(const lpid_t& root, rc_t& rc)
    : _height(0), _num_pages(0), _leaf_pages(0),
      _root(root), _top(0)
{
    btree_p rp;
    if (rc = rp.fix(root, LATCH_SH))  return;
    
    rc = _add_page(0, 0);
    _left_most[0] = _page[0].pid().page;
}



/*********************************************************************
 *
 *  btsink_t::map_to_root()
 *
 *  Map current running root page to the real root page. Deallocate
 *  original running root page after the mapping.
 *
 *********************************************************************/

rc_t
btsink_t::map_to_root()
{
    lsn_t anchor;
    xct_t* xd = xct();
    w_assert1(xd);
    if (xd)  anchor = xd->anchor();

    for (int i = 0; i <= _top; i++)  {
	X_DO( log_page_image(_page[i]), anchor );
    }

    /*
     *  Fix root page
     */
    btree_p rp;
    X_DO( rp.fix(_root, LATCH_EX), anchor );

    lpid_t child_pid;
    {
	btree_p cp = _page[_top];
	child_pid = cp.pid();

	_height = cp.level();

	if (child_pid == _root)  {
	    /*
	     *  No need to remap.
	     */
	    xd->release_anchor();
	    return RCOK;
	}

	/*
	 *  Shift everything from child page to root page
	 */
	X_DO( rp.set_hdr(_root.page, cp.level(), cp.pid0(), 0), anchor );
	w_assert1( !cp.next() && ! cp.prev());
	w_assert1( rp.nrecs() == 0);
	X_DO( cp.shift(0, rp), anchor );
    }
    _page[_top] = rp;

    if (xd)  {
	SSMTEST("btree.bulk.2");
	xd->compensate(anchor);
    }

    /*
     *  Free the child page. It has been copied to the root.
     */
    W_DO( io->free_page(child_pid) );

    return RCOK;
}



/*********************************************************************
 *
 *  btsink_t::_add_page(i, pid0)
 *
 *  Add a new page at level i. -- actually, it makes the page level
 *   i+1, since the btree ends up with leaves at level 1.
 *  Used only for bulk load, so it turns logging on & off.
 *
 *********************************************************************/
rc_t
btsink_t::_add_page(const int i, shpid_t pid0)
{
    lsn_t anchor;
    xct_t* xd = xct();
    if (xd) anchor = xd->anchor();

    {

	btree_p lsib = _page[i];
	/*
	 *  Allocate a new page.  I/O layer turns logging on when
	 *  necessary
	 */
	X_DO( btree_impl::_alloc_page(_root, 
		i+1, (_page[i] ? _page[i].pid() : _root), _page[i], pid0), anchor );

    
	/*
	 *  Update stats
	 */
	_num_pages++;
	if (i == 0) _leaf_pages++;

	{
	    xct_log_switch_t toggle(OFF);
	    /*
	     *  Link up
	     */
	    shpid_t left = 0;
	    if (lsib != 0) {
		left = lsib.pid().page;
	    }
	    DBG(<<"Adding page " << _page[i].pid()
		<< " at level " << _page[i].level()
		<< " prev= " << left
		<< " next=0 " 
		);
	    X_DO( _page[i].link_up(left, 0), anchor );

	    if (lsib != 0) {
		// already did:
		// left = lsib.pid().page;

		DBG(<<"Linking page " << lsib.pid()
		    << " at level " << lsib.level()
		    << " prev= " << lsib.prev()
		    << " next= "  << _page[i].pid().page
		    );
		X_DO( lsib.link_up(lsib.prev(), _page[i].pid().page), anchor );

		xct_log_switch_t toggle(ON);
		X_DO( log_page_image(lsib), anchor );
	    }
	}
    }

    if (xd)  {
	SSMTEST("btree.bulk.1");
	xd->compensate(anchor);
    }

    /*
     *  Current slot for new page is 0
     */
    _slot[i] = 0;
    
    return RCOK;
}




/*********************************************************************
 *
 *  btsink_t::put(key, el)
 *
 *  Insert a <key el> pair into the page[0] (leaf) of the stack
 *
 *********************************************************************/
rc_t
btsink_t::put(const cvec_t& key, const cvec_t& el)
{
    /*
     *  Turn OFF logging. Insertions into btree pages are not logged
     *  during bulkload. Instead, a page image log is generated
     *  when the page is filled (in _add_page()).
     *
     *  NB: we'll turn it off and on several times because while
     *  it's off, no other threads can run in this tx. 
     */
    rc_t rc;
    {
	xct_log_switch_t toggle(OFF);

	/*
	 *  Try inserting into the page[0] (leaf)
	 */
	rc = _page[0].insert(key, el, _slot[0]++);
    }
    if (rc) {
    
	if (rc.err_num() != eRECWONTFIT)  {
	    return RC_AUGMENT(rc);
	}
	
	/*
	 *  page[0] is full --- add a new page and and re-insert
	 *  NB: _add_page turns logging on when it needs to
	 */
	W_DO( _add_page(0, 0) );

	{
	    xct_log_switch_t toggle(OFF);
	    W_COERCE( _page[0].insert(key, el, _slot[0]++) );
	}

	/*
	 *  Propagate up the tree
	 */
	int i;
	for (i = 1; i <= _top; i++)  {
	    {
		xct_log_switch_t toggle(OFF);
		rc = _page[i].insert(key, el, _slot[i]++,
				     _page[i-1].pid().page);
	    }
	    if (rc)  {
		if (rc.err_num() != eRECWONTFIT)  {
		    return RC_AUGMENT(rc);
		}

		/*
		 *  Parent is full
		 *      --- add a new page for the parent and
		 *	--- continue propagation to grand-parent
		 */
		W_DO(_add_page(i, _page[i-1].pid().page));
		
	    } else {
		
		/* parent not full --- done */
		break;
	    }
	}

	/*
	 *  Check if we need to grow the tree
	 */
	if (i > _top)  {
	    ++_top;
	    W_DO( _add_page(_top, _left_most[_top-1]) );
	    _left_most[_top] = _page[_top].pid().page;
	    {
		xct_log_switch_t toggle(OFF);
		W_COERCE( _page[_top].insert(key, el, _slot[_top]++,
					 _page[_top-1].pid().page) );
	    }
	}
    }

    return RCOK;
}

