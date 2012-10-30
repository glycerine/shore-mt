/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sort.cc,v 1.85 1997/07/30 19:48:29 solomon Exp $
 */

#define SM_SOURCE
#define SORT_C

#ifdef __GNUG__
#    pragma implementation "sort_s.h"
#    pragma implementation "sort.h"
#endif

#include "sm_int_4.h"
#include "lgrec.h"
#include "sm.h"

#ifdef __GNUG__
template class w_auto_delete_array_t<int2>;
template class w_auto_delete_array_t<rid_t>;
template class w_auto_delete_array_t<run_scan_t>;
#endif


//
// key struct for sorting stream
//
struct sort_key_t {
    void* val;          // pointer to key
    void* rec;          // pointer to data
    uint2 klen;         // key length
    uint2 rlen;         // record data length (if large, in place indx size)

    NORET sort_key_t() {
                val = rec = 0;
                klen = rlen = 0;
          };
    NORET ~sort_key_t() {
                if (val) delete val;
                if (rec) delete rec;
          };
};

struct file_sort_key_t {
    const void* val;    // pointer to key
    const void* rec;    // pointer to data
    uint2 klen;         // key length
    uint2 rlen;         // record data length (if large, in place indx size)
    void* hdr;          // pointer to header
    uint4 blen;         // real body size
    uint2 hlen;         // record header length

    NORET file_sort_key_t() {
                hdr = 0;
                klen = hlen = rlen = 0;
                blen = 0;
          };
    NORET ~file_sort_key_t() { if (hdr) delete hdr; }
};

//
// sort descriptor
//
struct sort_desc_t {
    stid_t tmp_fid;     // fid for the temporary file
    sdesc_t* sdesc; 	// information about the file

    char** keys;   	// keys to be sorted
    char** fkeys;   	// file keys to be sorted
    uint rec_count;     // number of records for this run
    uint max_rec_cnt;   // max # of records for this run

    uint uniq_count;    // # of unique entries in runs

    rid_t* run_list;    // store first rid for all the runs
    uint2 max_list_sz;  // max size for run list array
    uint2 run_count;    // current size

    uint num_pages;     // number of pages
    uint total_rec;     // total number of records

    PFC cmp;            // comparison function
    const void *cdata;  // any extra data needed by the cmp function
    rid_t last_marker;  // rid of last marker
    rid_t last_rid;     // rid of last record in last run

    NORET sort_desc_t();
    NORET ~sort_desc_t();

    void  free_space() {
	uint total = total_rec < max_rec_cnt ? total_rec : max_rec_cnt;
        if (keys) { for (uint i=0; i<total; i++) 
			delete ((sort_key_t*) keys[i]);
			delete [] keys; keys = 0; max_rec_cnt = 0; }
        if (fkeys) { for (uint i=0; i<total; i++)
			delete ((file_sort_key_t*) fkeys[i]);
			delete [] fkeys; fkeys = 0; max_rec_cnt = 0; }
        uniq_count = 0;
    }

};

#ifdef notdef
// Taken out because character comparison differs
// on different machines, and we want something predictable
// for everything ...!!!

//
// Comparison function for chars
//
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
static int
_char_cmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *cdata)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (char*) kval1) - (* (char*) kval2);
}

#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
static int
_char_rcmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *cdata)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (char*) kval2) - (* (char*) kval1);
}
#endif

/**
 * A structure which holds the comparison information necessary for an
 * artificially-created "reverse" comparison function in the case that
 * a custom function is provided.
 */
struct rcustom_info {
  const void *cdata;
  PFC comp_func;
};
/**
 * Comparison function which reverses the output of a custom comparison
 * function.
 */
static int
_custom_rcmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *cdata)
{
  struct rcustom_info *rcinfo = (struct rcustom_info *)cdata;
  return (-(rcinfo->comp_func(klen1, kval1, klen2, kval2, rcinfo->cdata)));
}

//
// Comparison function for 1-byte unsigned integers and characters.
// NB: we use unsigned 1-byte compare for characters because the
// native char comparison on some machines is unsigned, some signed,
// and we want predictable results here.
//
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
static int
_uint1_cmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (w_base_t::uint1_t*) kval1) - (* (w_base_t::uint1_t*) kval2);
}

static int
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
_uint1_rcmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (w_base_t::uint1_t*) kval2) - (* (w_base_t::uint1_t*) kval1);
}

//
// Comparison function for 2-byte unsigned integers
//
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
static int
_uint2_cmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (w_base_t::uint2_t*) kval1) - (* (w_base_t::uint2_t*) kval2);
}

static int
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
_uint2_rcmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (w_base_t::uint2_t*) kval2) - (* (w_base_t::uint2_t*) kval1);
}

//
// Comparison function for 4-byte unsigned integers
//
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
static int
_uint4_cmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (w_base_t::uint4_t*) kval1) - (* (w_base_t::uint4_t*) kval2);
}

static int
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
_uint4_rcmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (w_base_t::uint4_t*) kval2) - (* (w_base_t::uint4_t*) kval1);
}

//
// Comparison function for 1-byte integers
//
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
static int
_int1_cmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (w_base_t::int1_t*) kval1) - (* (w_base_t::int1_t*) kval2);
}

static int
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
_int1_rcmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (w_base_t::int1_t*) kval2) - (* (w_base_t::int1_t*) kval1);
}

//
// Comparison function for 2-byte integers
//
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
static int
_int2_cmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (w_base_t::int2_t*) kval1) - (* (w_base_t::int2_t*) kval2);
}

static int
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
_int2_rcmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (w_base_t::int2_t*) kval2) - (* (w_base_t::int2_t*) kval1);
}

//
// Comparison function for 4-byte integers
//
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
static int
_int4_cmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (w_base_t::int4_t*) kval1) - (* (w_base_t::int4_t*) kval2);
}

static int
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
_int4_rcmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    return (* (w_base_t::int4_t*) kval2) - (* (w_base_t::int4_t*) kval1);
}

//
// Comparison function for floats
//
#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
static int
_float_cmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    w_base_t::f4_t tmp = (* (w_base_t::f4_t*) kval1) - (* (w_base_t::f4_t*) kval2);
    return tmp < 0 ? -1 : ((tmp > 0) ? 1 : 0);
}

#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
static int
_double_cmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    w_base_t::f8_t tmp = (* (w_base_t::f8_t*) kval1) - (* (w_base_t::f8_t*) kval2);
    return tmp < 0 ? -1 : ((tmp > 0) ? 1 : 0);
}

#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
static int
_float_rcmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    w_base_t::f4_t tmp = (* (w_base_t::f4_t*) kval2) - (* (w_base_t::f4_t*) kval1);
    return tmp < 0 ? -1 : ((tmp > 0) ? 1 : 0);
}

#ifndef W_DEBUG
#define klen1 /* klen1 not used */
#define klen2 /* klen2 not used */
#endif
static int
_double_rcmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
#undef klen1
#undef klen2
{
    w_assert3(klen1 == klen2);
    w_base_t::f8_t tmp = (* (w_base_t::f8_t*) kval2) - (* (w_base_t::f8_t*) kval1);
    return tmp < 0 ? -1 : ((tmp > 0) ? 1 : 0);
}

//
// Comparison function for strings 
//
static int 
_string_cmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
{
    register char* p1 = (char*) kval1;
    register char* p2 = (char*) kval2;
    register result = 0;
    for (uint4 i = klen1 < klen2 ? klen1 : klen2;
	 i > 0 && ! (result = *p1 - *p2);
	 i--, p1++, p2++);
    return result ? result : klen1 - klen2;
}

static int 
_string_rcmp(uint4 klen1, const void* kval1, uint4 klen2, const void* kval2, const void *)
{
    register char* p2 = (char*) kval1;
    register char* p1 = (char*) kval2;
    register result = 0;
    for (uint4 i = klen2 < klen1 ? klen2 : klen1;
	 i > 0 && ! (result = *p1 - *p2);
	 i--, p1++, p2++);
    return result ? result : klen2 - klen1;
}

// TODO: Mark, praveen : no statics !!
static nbox_t _universe_(2);
static nbox_t _box_(2);

//
// Comparison function for rectangles (based on hilbert curve 
//
static int _spatial_cmp(uint4 klen1, const void* kval1, uint4 klen2,
			const void* kval2, const void *)
{
    w_assert3(klen1 == klen2);

    nbox_t box1((char*)kval1, (int)klen1),
	   box2((char*)kval2, (int)klen2);
    
//    return (box1.hvalue(_universe_) - box2.hvalue(_universe_));
    return (box1.hcmp(box2, _universe_));
}

static int _spatial_rcmp(uint4 klen1, const void* kval1, uint4 klen2,
			const void* kval2, const void *)
{
    w_assert3(klen1 == klen2);

    nbox_t box1((char*)kval1, (int)klen1),
	   box2((char*)kval2, (int)klen2);
    
    return (box2.hvalue(_universe_) - box1.hvalue(_universe_));
//    return (box1.hcmp(box2, _universe_));
}

//
// Get comparison function
//
static PFC 
get_cmp_func(key_info_t &k_info, bool up)
{
    if (up) {
      switch(k_info.type) {
      	// case key_info_t::t_custom:
        case sortorder::kt_custom:
	        return k_info.cmp;

      	// case key_info_t::t_float:	
      	case sortorder::kt_f4:	
		return _float_cmp;

      	case sortorder::kt_f8:	
		return _double_cmp;

      	// case key_info_t::t_string:
      	case sortorder::kt_b:
		return _string_cmp;

      	// case key_info_t::t_spatial:
      	case sortorder::kt_spatial:
		return _spatial_cmp;

      	// case key_info_t::t_char:	 use u1
		// return _char_cmp;
      	case sortorder::kt_u1:
		return _uint1_cmp;

      	case sortorder::kt_u2:
		return _uint2_cmp;

      	case sortorder::kt_u4:
		return _uint4_cmp;

      	case sortorder::kt_i1:
		return _int1_cmp;

      	case sortorder::kt_i2:
		return _int2_cmp;

      	//case key_info_t::t_int:
      	case sortorder::kt_i4:
      	default: 			
		return _int4_cmp;
      }
    } else {
      switch(k_info.type) {
      	// case key_info_t::t_custom:
        case sortorder::kt_custom:
	  return _custom_rcmp;

      	// case key_info_t::t_float:	
      	case sortorder::kt_f4:	
		return _float_rcmp;

      	case sortorder::kt_f8:	
		return _double_rcmp;

      	// case key_info_t::t_string: // TODO: byte? u1?
      	case sortorder::kt_b:
		return _string_rcmp;

      	//case key_info_t::t_spatial:
      	case sortorder::kt_spatial:
		return _spatial_rcmp;

      	// case key_info_t::t_char:	 use u1
      	// case sortorder::kt_u1:	
		// return _char_rcmp;
      	case sortorder::kt_u1:
		return _uint1_rcmp;

      	case sortorder::kt_u2:
		return _uint2_rcmp;

      	case sortorder::kt_u4:
		return _uint4_rcmp;

      	case sortorder::kt_i1:
		return _int1_rcmp;

      	case sortorder::kt_i2:
		return _int2_rcmp;

      	// case key_info_t::t_int:
      	case sortorder::kt_i4:
      	default: 			
		return _int4_rcmp;
      }
    }
}

static const int _marker_ = -99999999;

sort_desc_t::sort_desc_t() 
{
    rec_count = num_pages = total_rec = uniq_count = 0;
    run_count = 0;
    max_list_sz = 20;
    run_list = new rid_t[max_list_sz];
    max_rec_cnt = 0;
    keys = 0;
    fkeys = 0;
    last_rid = last_marker = rid_t::null;
    tmp_fid = stid_t::null;
}

sort_desc_t::~sort_desc_t() 
{
    if (run_list) delete [] run_list;
    free_space();
}

NORET
run_scan_t::run_scan_t()
{
    eof = true;
    single = false;
    slot = i = 0;
    fp = 0;
}

NORET
run_scan_t::~run_scan_t()
{
    if (fp) delete [] fp;
}

rc_t
run_scan_t::init(rid_t& begin, const key_info_t& k, bool unique=false)
{
    i = 0;
    pid = begin.pid;
    slot = begin.slot - 1;
    kinfo = k;
    cmp = k.cmp;
    cdata = k.comp_data;
    eof = false;

    // toggle_base = (_unique = unique) ? 2 : 1;
    // toggle in all cases
    _unique = unique;
    toggle_base = 2;

    fp = new file_p[toggle_base];

    // open scan on the file
    W_DO( fp[0].fix(pid, LATCH_SH) );
    W_DO( next(eof) );

    if (unique) {
	W_DO( ss_m::SSM->fi->next_page(pid, eof) );
	if (eof) { 
	    single = true; eof = false; 
	} else { 
	    single = false; 
	    W_DO( fp[1].fix(pid, LATCH_SH) );
	}
    } 

    return RCOK;
}

rc_t
run_scan_t::current(const record_t*& rec)
{
    if (eof) { rec = NULL; }
    else { 
	rec = cur_rec; 
	w_assert1(fp->is_fixed());
	DBG(<<" current: i " << i
		<< " slot " << slot
		<< " pid " << pid
		<< " single " << single
	    );
    }

    return RCOK;
}

rc_t
run_scan_t::next(bool& end)
{
    FUNC(run_scan_t::next);
    end = false;
    if (eof) { end = true; return RCOK; }

    DBG(<<" next: i " << i
	<< " slot " << slot
	<< " pid " << pid
	<< " single " << single
    );

    // scan next record
    slot = fp[i].next_slot(slot);
    if (slot==0) {
	DBG(<<"new page");
	cur_rec = NULL;
	if (_unique) { // unique case 
	    // here
	    if (single) { eof = true; end = true; return RCOK; }
	    W_DO( ss_m::SSM->fi->next_page(pid, eof) );
	    if (eof) { single = true; eof = false; }
	    else  {
		W_DO( fp[i].fix(pid, LATCH_SH) );
	    }
	    // for unique case, we replace the current page and
	    // move i to the next page since we always have both
	    // pages fixed.
	    i = (i+1)%toggle_base;
	} else {
	    W_DO( ss_m::SSM->fi->next_page(pid, eof) );
	    if (eof) { end = true; return RCOK; }
	    i = (i+1)%toggle_base;
            W_DO( fp[i].fix(pid, LATCH_SH) );
        }
	slot = fp[i].next_slot(0);
    }

    DBG(<<" next: i " << i
	<< " slot " << slot
	<< " pid " << pid
	<< " single " << single
    );
    W_DO(fp[i].get_rec(slot, cur_rec));

    w_assert1(cur_rec);
    w_assert1(fp[i].is_fixed());

    if ( cur_rec->body_size()==sizeof(int)
	    && *(int*)cur_rec->body() == _marker_
	    && cur_rec->hdr_size()==0 ) {
	end = eof = true;
        fp[i].unfix();
	cur_rec = NULL;
    }

    return RCOK;
}

//
// detect if two records are duplicates (only works for small objects)
//
static bool duplicate_rec(const record_t* rec1, const record_t* rec2)
{
    // check if two records are identical

    if (rec1->body_size() != rec2->body_size() ||
	rec1->hdr_size() != rec2->hdr_size())
	return false;
    
    if (rec1->body_size()>0 && 
	memcmp(rec1->body(), rec2->body(), (int)rec1->body_size()))
	return false;

    if (rec1->hdr_size()>0 && 
	memcmp(rec1->hdr(), rec2->hdr(), rec1->hdr_size()))
	return false;
    
    return true;
}

//
// record size (for small object, it's the real size, for large record, it's
// the header size)
//
static inline uint rec_size(const record_t* r)
{
   return ((r->tag.flags & t_small) ? r->body_size() 
		: ((r->tag.flags & t_large_0) ? 
			sizeof(lg_tag_chunks_s) :
			sizeof(lg_tag_indirect_s)));
}

static void create_heap(int2 heap[], int heap_size, int num_runs,
				run_scan_t sc[])
{
    register r;
    register s, k;
    register winner;
    int tmp;

    for (s=0; s<heap_size; heap[s++] = heap_size);
    for (r = 0, k = heap_size >> 1; r < heap_size; r+=2, k++) {
	if (r < num_runs - 1) { /* two real competitors */
	    if (sc[r] > sc[r+1])
		heap[k] = r, winner = r+1;
	    else
		heap[k] = r+1, winner = r;
	}
	else {
	    heap[k] = -1;               /* an artifical key */
	    winner = (r>=num_runs)? -1 : r;
	}

	for (s = k >> 1;; s >>= 1) { /* propagate the winner upwards */
	    if (heap[s] == heap_size) { /* no one has reach here yet */
		heap[s] = winner; break;
	    }

	    if (winner < 0) /* a dummy key */
		winner = heap[s], heap[s] = -1;
	    else if (sc[winner] > sc[heap[s]]) { 
		tmp = winner; winner = heap[s]; heap[s] = tmp;
	    }
	}

    }
}

static int heap_top(int2 heap[], int heap_size, int winner, run_scan_t sc[])
{
    register    s;      /* a heap index */
    register    r;      /* a run num */

    for (s = (heap_size + winner) >> 1; s > 0; s >>= 1) {
	if ((r = heap[s]) < 0)
	    continue;
	if (sc[winner].is_eof())
	    heap[s] = -1, winner = r;
	else if (sc[winner] > sc[r])
	    heap[s] = winner, winner = r;
    }

    return (winner);
}

//
// Remove duplicate entries
//
rc_t 
sort_stream_i::remove_duplicates()
{
    register uint pos=0, prev=0;
    sd->uniq_count = 0;

    if (!(sd->last_rid==rid_t::null)) {
        // read in the previous rec
	file_p tmp;
	const record_t* r;
	W_DO( fi->locate_page(sd->last_rid, tmp, LATCH_SH) );
	W_COERCE( tmp.get_rec(sd->last_rid.slot, r) );

        uint2 hlen = r->hdr_size();
        uint4 rlen = r->body_size();

	if (r->is_small()) {
	    // compare against the previous one
	    if (_file_sort) {
		while(pos<sd->rec_count) {
		    file_sort_key_t* fk = (file_sort_key_t*)sd->fkeys[pos];
		    // If a custom comparison function was provided,
		    // it must determine if two items are equal
		    if (ki.type == key_info_t::t_custom) {
		      if ((ki.where == key_info_t::t_hdr) &&
			  (ki.cmp((ki.len ? ki.len : fk->klen),
				  (const void *)(fk->hdr + ki.offset), 
				  (ki.len ? ki.len : hlen),
				  (const void *)(r->hdr() + ki.offset),
				  ki.comp_data)))
			break;
		      else if ((ki.where == key_info_t::t_body) &&
			       (ki.cmp((ki.len ? ki.len : fk->rlen),
				       (const void *)(fk->rec+ki.offset), 
				       (ki.len ? ki.len : rlen),
				       (const void *)(r->body()+ki.offset),
				       ki.comp_data)))
			break;
		      pos++;
		    } else {
		      // Otherwise a memcmp will suffice
		      if (fk->hlen==(int)hlen && fk->rlen==(int)rlen) {
			if (hlen>0 && 
			    memcmp(fk->hdr+fk->klen, 
				   r->hdr()+fk->klen, hlen-fk->klen))
			  break;
			if (rlen>0 && memcmp(fk->rec, r->body(), (int)rlen))
			  break;
			pos++;
		      } else { break; }
		    }
		}
	    } else {
		while(pos<sd->rec_count) {
		    sort_key_t* k = (sort_key_t*)sd->keys[pos];
		    // If a custom comparison function was provided,
		    // it must determine if two items are equal
		    if (ki.type == key_info_t::t_custom) {
		      if ((ki.where == key_info_t::t_hdr) &&
			  (ki.cmp(k->klen, k->val, hlen, r->hdr(),
				  ki.comp_data)))
			break;
		      else if ((ki.where == key_info_t::t_body) &&
			       (ki.cmp(k->rlen, k->rec, 
				       rlen, r->body(), ki.comp_data)))
			break;
		      pos++;
		    } else {
		      // Otherwise a memcmp will suffice
		      if (k->klen==(int)hlen && k->rlen==(int)rlen) {
			if (hlen>0 && memcmp(k->val, r->hdr(), hlen))
			  break;
			if (rlen>0 && memcmp(k->rec, r->body(), (int)rlen))
			  break;
		    	pos++;
		      } else { break; }
		    }
		}
	    }
	}

	if (pos>=sd->rec_count) {
	    // none of them different, done
	    return RCOK;
	} else {
	    // move current to the first
	    prev = pos++;
	    sd->uniq_count++;
	}

    } else {
        pos = 1;
	sd->uniq_count++;
    }

    while (1) {
	if (_file_sort) {
          while(pos<sd->rec_count) {
	    file_sort_key_t *fk1 = (file_sort_key_t*)sd->fkeys[pos],
			    *fk2 = (file_sort_key_t*)sd->fkeys[pos-1];
	    // If a custom comparison function was provided,
	    // it must determine if two items are equal
	    if (ki.type == key_info_t::t_custom) {
	      if ((ki.where == key_info_t::t_hdr) &&
		  (ki.cmp((ki.len ? ki.len : fk1->hlen),
			  (const void *)(fk1->hdr + ki.offset),
			  (ki.len ? ki.len : fk2->hlen),
			  (const void *)(fk2->hdr + ki.offset),
			  ki.comp_data)))
		break;
	      else if ((ki.where == key_info_t::t_body) &&
		       (ki.cmp((ki.len ? ki.len : fk1->rlen),
			       (const void *)(fk1->rec + ki.offset),
			       (ki.len ? ki.len : fk2->rlen),
			       (const void *)(fk2->rec + ki.offset),
			       ki.comp_data)))
		break;
	      pos++;
	    } else {
	      // Otherwise a memcmp will suffice
	      if (fk1->hlen == fk2->hlen && fk1->rlen == fk2->rlen) {
                if (fk1->hlen>0 && 
		    memcmp(fk1->hdr+fk1->klen, fk2->hdr+fk2->klen,
			   fk1->hlen - fk1->klen))
		  break;
                if (fk1->rlen>0 && memcmp(fk1->rec, fk2->rec, fk1->rlen))
		  break;
                pos++;
	      } else { break; }
	    }
          }
	} else {
	    while(pos<sd->rec_count) {
	    	sort_key_t *k1 = (sort_key_t*)sd->keys[pos],
			   *k2 = (sort_key_t*)sd->keys[pos-1];
		// If a custom comparison function was provided,
		// it must determine if two items are equal
		if (ki.type == key_info_t::t_custom) {
		  if ((ki.where == key_info_t::t_hdr) &&
		      (ki.cmp(k1->klen, k1->val, 
			      k2->klen, k2->val, 
			      ki.comp_data)))
		    break;
		  else if ((ki.where == key_info_t::t_body) &&
			   (ki.cmp(k1->rlen, k1->rec,  
				   k2->rlen, k2->rec, 
				   ki.comp_data)))
		    break;
		  pos++;
		} else {
		  // Otherwise a memcmp will suffice
		  if (k1->klen == k2->klen && k1->rlen == k2->rlen) {
                    if (k1->klen>0 &&
			memcmp(k1->val, k2->val, k1->klen))
		      break;
               	    if (k1->rlen>0 && memcmp(k1->rec, k2->rec, k1->rlen))
		      break;
                    pos++;
		  } else { break; }
		}
            }
	}
        if (pos>=sd->rec_count) break;

	// move the entry up
	if (_file_sort) {
	    char *tmp = sd->fkeys[sd->uniq_count-1];
	    sd->fkeys[sd->uniq_count-1] = sd->fkeys[prev];
	    sd->fkeys[prev] = tmp;
	} else {
	    char* tmp = sd->keys[sd->uniq_count-1];
	    sd->keys[sd->uniq_count-1] = sd->keys[prev];
	    sd->keys[prev] = tmp;
	}
	sd->uniq_count++;
	prev = pos++;
    }
    if (prev) {
	if (_file_sort) {
	    char *tmp = sd->fkeys[sd->uniq_count-1];
	    sd->fkeys[sd->uniq_count-1] = sd->fkeys[prev];
	    sd->fkeys[prev] = tmp;
	} else {
	    char* tmp = sd->keys[sd->uniq_count-1];
	    sd->keys[sd->uniq_count-1] = sd->keys[prev];
	    sd->keys[prev] = tmp;
	}
    }
	
    return RCOK;
}

//
// find out the type of the record: small, large_0, large_1
//
static inline recflags_t
lgrec_type(uint2 rec_sz)
{
    if (rec_sz==sizeof(lg_tag_chunks_s))
	return t_large_0;
    else if (rec_sz==sizeof(lg_tag_indirect_s))
	return t_large_1;
    
    W_FATAL(fcINTERNAL);
    return t_badflag;
}

const MAXSTACKDEPTH = 30;
const LIMIT = 10;

static int
qsort_cmp(const void* k1, const void* k2, PFC comp_func, const void *cdata)
{
  return comp_func(  ((sort_key_t*)k1)->klen,
		     ((sort_key_t*)k1)->val,
		     ((sort_key_t*)k2)->klen,
		     ((sort_key_t*)k2)->val,
		     cdata);
}

static int
fqsort_cmp(const void* k1, const void* k2, PFC comp_func, const void *cdata)
{
  return comp_func(((file_sort_key_t*)k1)->klen,
		   ((file_sort_key_t*)k1)->val,
		   ((file_sort_key_t*)k2)->klen,
		   ((file_sort_key_t*)k2)->val,
		   cdata);
}

static void QuickSort(char* a[], int cnt, 
		      int (*compare)(const void*, const void*, 
				     PFC comp, const void *cdata), 
		      PFC cmpfunc, const void *cdata)
{
    int stack[MAXSTACKDEPTH][2];
    int sp = 0;
    int l, r;
    char* tmp;
    register i, j;
    register char* pivot;
    long randx = 1;

    for (l = 0, r = cnt - 1; ; ) {
	if (r - l < LIMIT) {
	    if (sp-- <= 0) break;
	    l = stack[sp][0], r = stack[sp][1];
	    continue;
	}
	randx = (randx * 1103515245 + 12345) & 0x7fffffff;
	pivot = a[l + randx % (r - l)];
	for (i = l, j = r; i <= j; )  {
	    while (compare(a[i], pivot, cmpfunc, cdata) < 0) i++;
	    while (compare(pivot, a[j], cmpfunc, cdata) < 0) j--;
	    if (i < j) { tmp=a[i]; a[i]=a[j]; a[j]=tmp; }
	    if (i <= j) i++, j--;
	}

	if (j - l < r - i) {
	    if (i < r) stack[sp][0] = i, stack[sp++][1] = r;
            r = j;
	} else {
	    if (l < j) stack[sp][0] = l, stack[sp++][1] = j;
	    l = i;
	}
    }

    for (i = 1; i < cnt; a[j+1] = pivot, i++)
	for (j = i - 1, pivot = a[i];
	     j >= 0 && (compare(pivot, a[j], cmpfunc, cdata) < 0);
             a[j+1] = a[j], j--);
}

//
// Sort the entries in the buffer of the current run, 
// and flush them out to disk page.
//
rc_t 
sort_stream_i::flush_run()
{
    FUNC(sort_stream_i::flush_run);
    register int i;
    if (sd->rec_count==0) return RCOK;

    if (sd->tmp_fid==stid_t::null) {
	// not last pass so use temp file
	if (_once) {
	    W_DO( SSM->_create_file(sp.vol, sd->tmp_fid, 
				_property, _logical_id) );
	} else {
	    W_DO( SSM->_create_file(sp.vol, sd->tmp_fid, 
				t_temporary, serial_t::null) );
	}
    }
    
#ifdef OLD_SORT
    W_COERCE( dir->access(sd->tmp_fid, sd->sdesc, NL) );
    w_assert1(sd->sdesc);
#else
    W_COERCE( dir->access(sd->tmp_fid, sd->sdesc, NL) );
    w_assert1(sd->sdesc);

    file_p last_page;
#endif

    rid_t rid, first;

    _universe_ = ki.universe;

    if (_file_sort) {
	QuickSort(sd->fkeys, sd->rec_count, fqsort_cmp, 
		  sd->cmp, sd->cdata);
    } else {
	QuickSort(sd->keys, sd->rec_count, qsort_cmp, sd->cmp, sd->cdata);
	if (ki.len==0 && ki.type!=key_info_t::t_string &&
	    ki.type!=key_info_t::t_custom) {
	    ki.len = ((file_sort_key_t*)sd->keys[0])->klen;
	}
    }

    // remove duplicates if needed
    if (sp.unique) {
    	W_DO ( remove_duplicates() );
    	if (sd->uniq_count==0) return RCOK;
    }

    // load the sorted <key, elem> pair to temporary file
    int count = sp.unique ? sd->uniq_count : sd->rec_count;
    if (_file_sort) {
	for (i=0; i<count; i++) {
	    const file_sort_key_t* k = (file_sort_key_t*) sd->fkeys[i];
	    vec_t hdr, data(k->rec, k->rlen);
	    if (_once) {
		hdr.put(k->hdr+k->klen, k->hlen-k->klen);
		if (sp.keep_lid) {
		    serial_t serial_no;
		    memcpy(&serial_no, k->hdr+k->klen-sizeof(serial_t),
				sizeof(serial_t));
#ifdef OLD_SORT
	    	    W_DO ( fi->create_rec(sd->tmp_fid, hdr, k->rlen,
				data, serial_no, *sd->sdesc, rid) );
#else
	    	    W_DO ( fi->create_rec_at_end(sd->tmp_fid, hdr, k->rlen,
				data, serial_no, *sd->sdesc, last_page, rid) );
#endif
		    W_DO ( lid->associate(sp.lvid, serial_no, rid) );
		} else {
#ifdef OLD_SORT
	    	    W_DO ( fi->create_rec(sd->tmp_fid, hdr, k->rlen,
				data, serial_t::null, *sd->sdesc, rid) );
#else
	    	    W_DO ( fi->create_rec_at_end(sd->tmp_fid, hdr, k->rlen,
				data, serial_t::null, *sd->sdesc, 
				last_page, rid) );
#endif
		}
	    } else {
		hdr.put(k->hdr, k->hlen);
#ifdef OLD_SORT
	    	W_DO ( fi->create_rec(sd->tmp_fid, hdr, k->rlen,
			data, serial_t::null, *sd->sdesc, rid) );
#else
	    	W_DO ( fi->create_rec_at_end(sd->tmp_fid, hdr, k->rlen,
			data, serial_t::null, *sd->sdesc, 
			last_page, rid) );
#endif
	    }
	    if (k->blen > k->rlen) {
                // HACK:
                // for large object, we need to reset the tag
                // because we only did a shallow copy of the body
		W_DO( fi->update_rectag(rid, k->blen, lgrec_type(k->rlen)) );
 	    }
	    if (rid.slot == 1) ++sd->num_pages;
	    if (i == 0) first = rid;
	}
    } else {
	for (i=0; i<count; i++) {
	    const sort_key_t* k = (sort_key_t*) sd->keys[i];
	    vec_t hdr(k->val, k->klen), data(k->rec, k->rlen);
#ifdef OLD_SORT
	    W_DO ( fi->create_rec(sd->tmp_fid, hdr, k->rlen,
			data, serial_t::null, *sd->sdesc, rid) );
#else
	    W_DO ( fi->create_rec_at_end(sd->tmp_fid, hdr, k->rlen,
			data, serial_t::null, *sd->sdesc, 
			last_page, rid) );
#endif
	    if (rid.slot == 1) ++sd->num_pages;
	    if (i == 0) first = rid;
	}
    }
    sd->last_rid = rid;

    if (!_once) {
    	// put a marker to distinguish between different runs
    	vec_t hdr, data((void*)&_marker_, sizeof(int));
#ifdef OLD_SORT
    	W_DO( fi->create_rec(sd->tmp_fid, hdr, sizeof(int), data,
                           	serial_t::null, *sd->sdesc, rid) );
#else
    	W_DO( fi->create_rec_at_end(sd->tmp_fid, hdr, sizeof(int), data,
                           	serial_t::null, *sd->sdesc, 
				last_page, rid) );
#endif
    }

    sd->last_marker = rid;
    sd->total_rec += sd->rec_count;

    // record first rid for the current run
    if (sd->run_count == sd->max_list_sz) {
        // expand the run list space
        rid_t* tmp = new rid_t[sd->max_list_sz<<1];
        memcpy(tmp, sd->run_list, sd->run_count*sizeof(rid));
        delete [] sd->run_list;
        sd->run_list = tmp;
        sd->max_list_sz <<= 1;
    }
    sd->run_list[sd->run_count++] = first;

    // reset count for new run
    sd->rec_count = 0;
    buf_space.reset();
    return RCOK;
}

rc_t
sort_stream_i::flush_one_rec(const record_t *rec, rid_t& rid, 
			     const stid_t& out_fid, file_p& last_page)
{
    // for regular sort
    uint rlen = rec_size(rec);
    vec_t hdr(rec->hdr(), rec->hdr_size()),
	  data(rec->body(), (int)rlen);

#ifdef OLD_SORT
    W_DO( fi->create_rec(out_fid, hdr, rlen, data, 
			 serial_t::null, *sd->sdesc, rid) );
#else
    W_DO( fi->create_rec_at_end(out_fid, hdr, rlen, data, 
				serial_t::null, *sd->sdesc, 
				last_page, rid) );
#endif

    if (!(rec->tag.flags & t_small)) {
	// large object, patch rectag for shallow copy
	W_DO( fi->update_rectag(rid, rec->tag.body_len, rec->tag.flags) );
    }
    
    return RCOK;
}

rc_t
sort_stream_i::merge(bool skip_last_pass=false)
{
    uint4 i, j, k;
    FUNC(sort_stream_i::merge)

//    if (sp.unique) { sp.run_size >>= 1; }

    for (i = sp.run_size-1, heap_size = 1; i>0; heap_size <<= 1, i>>=1);
    int2* m_heap = new int2[heap_size];
    w_assert1(m_heap);
    w_auto_delete_array_t<int2> auto_del_heap(m_heap);

    num_runs = sd->run_count;

    if (sd->run_count<=1) return RCOK;

    uint4 out_parts = sd->run_count;
    int in_parts = 0;
    int num_passes = 1;
    for (i=sp.run_size; i<sd->run_count; i*=sp.run_size, num_passes++);
    
    uint4 in_list_cnt = 0, out_list_cnt = sd->run_count;
    stid_t& out_file = sd->tmp_fid;
    rid_t  *out_list = sd->run_list, *list_buf = new rid_t[out_list_cnt+1],
	   *in_list = list_buf;
    w_assert1(list_buf);
    w_auto_delete_array_t<rid_t> auto_del_list(list_buf);

    for (i=0; out_parts>1; i++) {
	in_parts = out_parts;
	out_parts = (in_parts - 1) / sp.run_size + 1;
	
	{ rid_t* tmp = in_list; in_list = out_list, out_list = tmp; }
	in_list_cnt = out_list_cnt;
	out_list_cnt = 0;
	stid_t in_file = out_file;
	bool last_pass = (out_parts==1);

	if (skip_last_pass && last_pass) {
	    if (sd->run_list != in_list) {
	       // switched, we need to copy the out list
	       memcpy(sd->run_list, in_list, (int)in_list_cnt*sizeof(rid_t));
	    }
	    num_runs = in_parts;
	    sd->tmp_fid = in_file;
	    return RCOK;
	}

    	if (last_pass) {
	    // last pass may not be on temporary file
            // (file should use logical_id)
	    W_DO( SSM->_create_file(sp.vol, out_file, sp.property,
					sp.logical_id) );
    	} else {
	    // not last pass so use temp file
	    W_DO( SSM->_create_file(sp.vol, out_file, 
					t_temporary,
					serial_t::null) );
    	}


	W_COERCE( dir->access(out_file, sd->sdesc, NL) );
	w_assert1(sd->sdesc);
#ifndef OLD_SORT
	file_p last_page;
#endif

	int b;
	for (j=b=0; j<out_parts; j++, b+=sp.run_size) {
	    num_runs = (j==out_parts-1) ? (in_parts-b) : sp.run_size;
	    run_scan_t* rs = new run_scan_t[num_runs];
    	    w_assert1(rs);
	    w_auto_delete_array_t<run_scan_t> auto_del_run(rs);
	    for (k = 0; k<num_runs; k++) {
		W_DO( rs[k].init(in_list[b+k], ki, sp.unique) );
	    }

	    if (num_runs == 1) m_heap[0] = 0;
	    else {
		for (k = num_runs-1, heap_size = 1; k > 0;
			heap_size <<= 1, k >>= 1);
		create_heap(m_heap, heap_size, num_runs, rs);
	    }

	    bool eof, new_part = true;
	    rid_t rid;
	    const record_t *rec = 0, *old_rec = 0;
	    bool first_rec = true;

	    register uint2 r;
	    for (r = m_heap[0]; num_runs > 1; r = heap_top(m_heap, heap_size, r, rs)) {
	    	W_DO( rs[r].current(rec) );
	    	if (sp.unique) {
		    if (first_rec) {
		    	old_rec = rec;
		    	first_rec = false;
		    } else {
			W_DO(flush_one_rec(old_rec, rid, out_file, last_page));
		    	old_rec = rec;
		        if (new_part) {
		            out_list[out_list_cnt++] = rid;
		            new_part = false;
	    	    	}
		    }
	    	} else { 
		    W_DO( flush_one_rec(rec, rid, out_file, last_page) );
		    if (new_part) {
		        out_list[out_list_cnt++] = rid;
		        new_part = false;
	    	    }
                }
		W_DO( rs[r].next(eof) );
		if (eof) --num_runs;
	    }

	    do { //  for the rest in last run for current merge
	    	W_DO( rs[r].current(rec) );
	    	if (sp.unique) {
		    if (first_rec) {
		    	old_rec = rec;
		    	first_rec = false;
		    } else {
		    	W_DO(flush_one_rec(old_rec, rid, out_file, last_page));
		    	old_rec = rec;
		        if (new_part) {
		            out_list[out_list_cnt++] = rid;
		            new_part = false;
	    	    	}
		    }
	    	} else { 
		    W_DO( flush_one_rec(rec, rid, out_file, last_page) );

		    if (new_part) {
		        out_list[out_list_cnt++] = rid;
		        new_part = false;
	    	    }
		}

		W_DO( rs[r].next(eof) );

	    } while (!eof);

	    if (sp.unique) {
	    	W_DO( flush_one_rec(old_rec, rid, out_file, last_page) );
	    	if (new_part) {
		    out_list[out_list_cnt++] = rid;
		    new_part = false;
	    	}
	    }
	    if (!last_pass) {
	    	// put a marker to distinguish between different runs
	    	vec_t hdr, data((void*)&_marker_, sizeof(int));
#ifdef OLD_SORT
	    	W_DO( fi->create_rec(out_file, hdr, sizeof(int), data,
				serial_t::null, *sd->sdesc, rid) );
#else
	    	W_DO( fi->create_rec_at_end(out_file, hdr, sizeof(int), data,
				serial_t::null, *sd->sdesc, last_page, rid) );
#endif
	    }
	}
	W_DO ( SSM->_destroy_file(in_file) );
    }

    return RCOK;
}

NORET
sort_stream_i::sort_stream_i() : xct_dependent_t(xct()) 
{
    _file_sort = sorted = eof = false;
    empty = true;
    heap=0;
    sc=0;
    sd = new sort_desc_t; 
    _once = false;
    _logical_id = serial_t::null;
}

NORET
sort_stream_i::sort_stream_i(const key_info_t& k, const sort_parm_t& s,
			     uint est_rec_sz) : xct_dependent_t(xct())
{
    _file_sort = sorted = eof = false;
    empty = true;
    heap=0;
    sc=0;
    
    sd = new sort_desc_t; 
    ki = k;
    sp = s;

    sp.run_size = (sp.run_size<3) ? 3 : sp.run_size;
    if (est_rec_sz==0) {
	// no hint, just get a rough estimate
	est_rec_sz = (k.est_reclen ? k.est_reclen : 20);
    }

    sd->max_rec_cnt = (uint) (file_p::data_sz / 
	(align(sizeof(rectag_t)+ALIGNON+est_rec_sz)+sizeof(page_s::slot_t))
	* sp.run_size);

    // If the comparison function is specified, create the structure 
    // that will be used as the data for the inverse function
    if ((ki.type == key_info_t::t_custom) && (!sp.ascending)) {
      struct rcustom_info *newdata = new struct rcustom_info();
      newdata->comp_func = ki.cmp;
      newdata->cdata = ki.comp_data;
      ki.comp_data = newdata;
    }
    sd->cmp = get_cmp_func(ki, sp.ascending);
    sd->cdata = ki.comp_data;
    _once = false;
    _logical_id = serial_t::null;
}

NORET
sort_stream_i::~sort_stream_i()
{
    if (heap) delete [] heap;
    if (sc) delete [] sc;
    if (sd) {
        if (sd->tmp_fid!=stid_t::null) {
	    W_IGNORE ( SSM->_destroy_file(sd->tmp_fid) );
        }
        delete sd;
    }
    if ((ki.type == key_info_t::t_custom) && (!sp.ascending))
      delete ((struct rcustom_info *)ki.comp_data);
}

void 
sort_stream_i::finish()
{
    if (heap) {
	delete [] heap; heap = 0;
    }
    if (sc) {
	delete [] sc; sc = 0;
    }
    if (sd) {
	if (sd->tmp_fid!=stid_t::null) {
	    if (xct())
	    	W_COERCE ( SSM->_destroy_file(sd->tmp_fid) );
        }
        delete sd;
	sd = 0;
    }
}

void
sort_stream_i::xct_state_changed(
    xct_state_t         /*old_state*/,
    xct_state_t         new_state)
{
    if (new_state == xct_aborting || new_state == xct_committing)  {
        finish();
    }
}

void
sort_stream_i::init(const key_info_t& k, const sort_parm_t& s, uint est_rec_sz)
{
    _file_sort = sorted = eof = false;
    ki = k;
    sp = s;

    sp.run_size = (sp.run_size<3) ? 3 : sp.run_size;

    if (!est_rec_sz) {
	// no hint, just get a rough estimate
	est_rec_sz = 20;
    }

    sd->max_rec_cnt = (uint) (file_p::data_sz / 
	(align(sizeof(rectag_t)+est_rec_sz)+sizeof(page_s::slot_t))
	* sp.run_size);
    
    if (sd->keys)  {
	for (register uint i=0; i < sd->rec_count; i++)
	    delete ((sort_key_t*) sd->keys[i]);
        delete [] sd->keys;
        sd->keys = 0;
    }

    if (sd->fkeys)  {
        for (register uint i=0; i<sd->rec_count; i++)
	    delete ((file_sort_key_t*) sd->fkeys[i]);
	delete [] sd->fkeys;
        sd->fkeys = 0;
    }

    sd->rec_count = 0;

    // If the comparison function is specified, create the structure 
    // that will be used as the data for the inverse function
    if ((ki.type == key_info_t::t_custom) && (!sp.ascending)) {
      struct rcustom_info *newdata = new struct rcustom_info();
      newdata->comp_func = ki.cmp;
      newdata->cdata = ki.comp_data;
      ki.comp_data = newdata;
    }
    sd->cmp = get_cmp_func(ki, sp.ascending);
    sd->cdata = ki.comp_data;

    if (sc) { delete [] sc; sc = 0; }

    if (sd->tmp_fid!=stid_t::null) {
	W_COERCE( SSM->_destroy_file(sd->tmp_fid) );
	sd->tmp_fid = stid_t::null;
    }

    _file_sort = false;
}

rc_t
sort_stream_i::put(const cvec_t& key, const cvec_t& elem)
{
    FUNC(sort_stream_i::put);
    w_assert1(!_file_sort);

    if (sd->rec_count >= sd->max_rec_cnt) {
	// flush current run
	W_DO ( flush_run() );
    }

    if (!sd->keys) {
	sd->keys = new char* [sd->max_rec_cnt];
        w_assert1(sd->keys);
	memset(sd->keys, 0, sd->max_rec_cnt*sizeof(char*));
    }

    sort_key_t* k = (sort_key_t*) sd->keys[sd->rec_count];
    if (!k) {
	k = new sort_key_t;
        sd->keys[sd->rec_count] = (char*) k;
    } else {
	if (k->val) delete [] k->val;
	if (k->rec) delete [] k->rec; 
    }

    // copy key
    k->val = new char[key.size()];
    key.copy_to(k->val, key.size());
    k->klen = key.size();

    // copy elem
    k->rlen = elem.size();
    k->rec = new char[k->rlen];
    elem.copy_to(k->rec, k->rlen);

    sd->rec_count++;

    if (empty) empty = false;

    return RCOK;
}

rc_t
sort_stream_i::file_put(const cvec_t& key, const void* rec, uint rlen,
			uint hlen, const rectag_t* tag)
{
    FUNC(sort_stream_i::put);
    w_assert1(_file_sort);

    if (sd->rec_count >= sd->max_rec_cnt) {
	// ran out of array space
	if (_once) {
	  if (sd->fkeys) {
	    char** new_keys = new char* [sd->max_rec_cnt<<1];
	    memcpy(new_keys, sd->fkeys, sd->max_rec_cnt*sizeof(char*));
	    memset(&new_keys[sd->max_rec_cnt], 0, sd->max_rec_cnt*sizeof(char*));
	    // this is needed for shallow deletion of fkeys
	    memset(sd->fkeys, 0, sd->max_rec_cnt*sizeof(char*));
	    delete [] sd->fkeys;
	    sd->fkeys = new_keys;
	  }
	sd->max_rec_cnt <<= 1;
      } else {
	// flush current run
	W_DO ( flush_run() );
      }
    }

    if (!sd->fkeys) {
	sd->fkeys = new char* [sd->max_rec_cnt];
	w_assert1(sd->fkeys);
	memset(sd->fkeys, 0, sd->max_rec_cnt*sizeof(char*));
    }

    file_sort_key_t* k = (file_sort_key_t*) sd->fkeys[sd->rec_count];
    if (!k) {
	k = new file_sort_key_t;
	sd->fkeys[sd->rec_count] = (char*) k;
    } else {
        if (k->hdr) { delete [] k->hdr; }
    }

    // copy key and hdr
    k->hdr = new char[key.size()];
    key.copy_to((void*)k->hdr, key.size());
    k->val = k->hdr;
    k->klen = k->hlen = key.size();

    if (!ki.derived) {
	// for non-derived keys, the key will have rec hdr at end
	k->klen -= hlen;
    }

    // set up body
    k->rec = rec;
    k->rlen = rlen;
    if (tag && !(tag->flags & t_small))
	k->blen = tag->body_len;
    else 
	k->blen = rlen;

    sd->rec_count++;
    if (empty) empty = false;

    return RCOK;
}

rc_t
sort_stream_i::get_next(vec_t& key, vec_t& elem, bool& end)
{
    FUNC(sort_stream_i::get_next)
    fill4 filler;
    W_DO( file_get_next(key, elem, filler.u4, end) );

    return RCOK;
}

rc_t
sort_stream_i::file_get_next(vec_t& key, vec_t& elem, uint4& blen, bool& end)
{
    FUNC(sort_stream_i::file_get_next);
    end = eof;
    if (eof) {
	finish();
	return RCOK;
    }

    if (!sorted) {
	// flush current run
	W_DO ( flush_run() );

        // free the allocated buffer space for sorting phase
	sd->free_space();

	// sort and merge, leave the final merge to the end
	W_DO ( merge(true) );

	sorted = true;

	if (num_runs==0) {
	    end = eof = true;
	    finish();
	    return RCOK;
	}

	// initialize for the final merge: ???
	if (sc) delete [] sc;
	sc = new run_scan_t[num_runs];

	register uint4 k;
	for (k = 0; k<num_runs; k++) {
	    W_DO( sc[k].init(sd->run_list[k], ki, sp.unique) );
	}

	for (k = num_runs-1, heap_size = 1; k > 0; heap_size <<= 1, k >>= 1);
	if (heap) delete [] heap;
	heap = new int2[heap_size];
        
	if (num_runs == 1)  {
	    r = heap[0] = 0;
	} else {
	    create_heap(heap, heap_size, num_runs, sc);
	    r = heap[0];
            r = heap_top(heap, heap_size, r, sc);
        }

	old_rec = 0;
    }
    
    // get current record
    const record_t *rec;
    bool part_eof;
    
    W_DO ( sc[r].current(rec) );
    DBG(<<"r " << r 
	<< " rec: "
	<< rec->body()[0]
	<< rec->body()[1]
	<< rec->body()[2]
	);

    if (sp.unique) {
	if (old_rec) {
	    bool same = duplicate_rec(old_rec, rec);
	    while (same && num_runs>1) {
		old_rec = rec;
		r = heap_top(heap, heap_size, r, sc);
		W_DO ( sc[r].next(part_eof) );
		if (part_eof) {
		    // reach end of current run
		    r = heap_top(heap, heap_size, r, sc);
		    num_runs--;
		} else { 
		    W_DO ( sc[r].current(rec) );
		    same = duplicate_rec(old_rec, rec);
		}
	    }
	    if (same) {
		while (1) {
		    old_rec = rec;
		    W_DO ( sc[r].current(rec) );
		    if (!duplicate_rec(old_rec, rec))
			break;
		    W_DO ( sc[r].next(part_eof) );
		    if (part_eof) {
			// end, search in vain, still write the last one out
			end = eof = true; 
			finish();
			return RCOK;
		    }
		}
	    }
	}
	old_rec = rec;
    }

    // set up the key for output
    key.put(rec->hdr(), rec->hdr_size());
    elem.put(rec->body(), rec_size(rec));

    DBG(<<"copying  rec " << rec->body()[0] << rec->body()[1] << rec->body()[2]);

    if (_file_sort) { blen = rec->body_size(); }

    // prepare for next get
    W_DO ( sc[r].next(part_eof) );

    if (num_runs>1)
        r = heap_top(heap, heap_size, r, sc);

    if (part_eof)  {
	if (--num_runs < 1) {
	    eof = true;
	}
    }
    DBG(<<"returning  rec " << rec->body()[0] << rec->body()[1] << rec->body()[2]);

    return RCOK;
}

//
// copy large object to a contigous chunk of space
//
static rc_t
_copy_out_large_obj(
	const record_t* rec,
	void*		data,
	uint4		start,
	uint4		len,
	const file_p&	hdr_page)
{
    lpid_t data_pid;
    lgdata_p page;
    char* buf_ptr = (char*) data;
    uint4 start_byte, new_start = start, range, left = len, offset;
	
    w_assert1(!rec->is_small());

    while (left>0) {
	data_pid = rec->pid_containing(new_start, start_byte, hdr_page);
	offset = new_start-start_byte;
	range = MIN(lgdata_p::data_sz-offset, rec->body_size()-new_start); 
	W_DO( page.fix(data_pid, LATCH_SH) );
	memcpy(buf_ptr, (char*) page.tuple_addr(0)+offset, (int)range);
	buf_ptr += range; 
	new_start += range;
	left -= range;
    }

    w_assert1(left==0);

    return RCOK;
}
    

rc_t
ss_m::_sort_file(const stid_t& fid, vid_t vid, stid_t& sfid,
                sm_store_property_t property,
                const key_info_t& key_info, int run_size,
                bool unique, bool destructive,
                const serial_t& logical_id,
		const lvid_t& logical_vid)
{
    register int i, j;

    if (run_size < 3) run_size = 3;

    // format sort parameters
    sort_parm_t sp;
    sp.unique = unique;
    sp.vol = vid;
    sp.property = property;
    sp.logical_id = logical_id;
    sp.destructive = destructive;
    sp.keep_lid = (sp.logical_id != serial_t::null) && destructive;
    sp.lvid = logical_vid;

    bool large_obj = false;
    stid_t lg_fid;
    sdesc_t* sdesc;
    rid_t rid;
    if (!sp.destructive) {
	// HACK:
	// create a file to hold potential large object 
	// in a non-destructive sort.
	W_DO ( _create_file(vid, lg_fid, property, logical_id) );
    	W_COERCE( dir->access(lg_fid, sdesc, NL) );
	w_assert1(sdesc);
    }

#ifndef OLD_SORT
    file_p last_lg_page;
#endif

    key_info_t kinfo = key_info;
    bool fast_spatial = (kinfo.type==sortorder::kt_spatial && !sp.unique);
    if (fast_spatial) { 
	// hack to speed up spatial comparison:
	// use hilbert value (turn into an integer sort)
	kinfo.type = sortorder::kt_i4;
	kinfo.len = sizeof(int);
    }

    w_assert1((kinfo.len>0) || 
	      (kinfo.len==0 && kinfo.type==key_info_t::t_string) ||
	      (kinfo.len==0 && kinfo.type==key_info_t::t_custom));

    // determine how many pages are used for the current file to 
    // estimate file size (decide whether all-in-memory is possible)
    lpid_t pid, first_pid;
    W_DO( fi->first_page(fid, pid) );
    first_pid = pid;
    int pcount = 0;
    bool eof = false;

    while (!eof) {
	if (pcount++ > run_size) break;
	W_DO(fi->next_page(pid, eof) );
    }
    
    bool once = (pcount<=run_size);
    if (once) run_size = pcount;

    // setup a sort stream
    sp.run_size = run_size;
    sort_stream_i sort_stream(kinfo, sp);

    if (once) {
        sort_stream.set_file_sort_once(property, logical_id);
    } else {
        sort_stream.set_file_sort();
    }

    // iterate through records, put into sort stream
    // the algorithm fix a block of pages in buffer to
    // save the extra cost of copy each tuple from buffer pool
    // to to memory for sorting
    pid = first_pid;
    {
      file_p* fp = new file_p[pcount];
      w_auto_delete_array_t<file_p> auto_del_fp(fp);

      for (eof = false; ! eof; ) {
	 for (i=0; i<run_size && !eof; i++) {
	    W_DO( fp[i].fix(pid, LATCH_SH) );
	    for (j = fp[i].next_slot(0); j; j = fp[i].next_slot(j)) {
		//
		// extract the key, and compress hdr with body
		//
	        const record_t* r;
		W_COERCE( fp[i].get_rec(j, r) );

		const void *kval, *hdr, *rec;
		uint rlen, hlen;

		hdr = r->hdr();
		hlen = r->hdr_size();

		if (!r->is_small()) {
		    // encounter large record in sort file:
		    //	has to be destructive sort
		    large_obj = true;
		    if (sp.destructive) {
			rec = r->body();
			rlen = rec_size(r);
		    } else {
			// needs to copy the whole large object
			rlen = (uint) r->body_size();

			// copy the large object into another file
			char* buf = new char[rlen];
			w_auto_delete_array_t<char> auto_del_buf(buf);
			W_DO( _copy_out_large_obj(r, buf, 0, rlen, fp[i]) );
			vec_t b_hdr, b_rec(buf, rlen);
			
			W_COERCE( dir->access(lg_fid, sdesc, NL) );
			w_assert1(sdesc);
#ifdef OLD_SORT
			W_DO ( fi->create_rec(lg_fid, b_hdr, rlen, b_rec, 
					serial_t::null, *sdesc, rid) );
#else
			W_DO ( fi->create_rec_at_end(lg_fid, b_hdr, rlen,
					b_rec, serial_t::null, *sdesc,
					last_lg_page, rid) );
#endif
			// get its new index info
			file_p tmp;
			W_DO( fi->locate_page(rid, tmp, LATCH_SH) );
			const record_t* nr;
			W_COERCE( tmp.get_rec(rid.slot, nr) );

			rlen = rec_size(nr);
			void* nbuf = sort_stream.buf_space.alloc(rlen);
			memcpy(nbuf, nr->body(), rlen);
			rec = nbuf;
		    }

		    if (key_info.where==key_info_t::t_hdr) {
			kval = (char*)hdr + key_info.offset;
		    } else {
			// has to copy the region of bytes for key
			void* buf = sort_stream.buf_space.alloc(key_info.len);
			W_DO( _copy_out_large_obj(r, buf, key_info.offset,
						 key_info.len, fp[i]) );
			kval = buf;
		    }
		} else {
		    rec = r->body();
		    rlen = rec_size(r);
		    kval = (char*) ((key_info.where==key_info_t::t_hdr)
				 ? hdr : rec) + key_info.offset;
		}

		// construct sort key, append hdr at the end	
		cvec_t key;
		int hvalue;
		if (fast_spatial) {
		    // hack for spatial keys (nbox_t type)
		    // use the hilbert value as the key
		    _box_.bytes2box((const char*)kval, (int)key_info.len);
		    hvalue = _box_.hvalue(key_info.universe);
		    key.put(&hvalue, sizeof(int));
		} else {
		    if ((int)key_info.len == 0)
		      key.put(kval, ((key_info.where==key_info_t::t_hdr)
				     ? hlen : rlen));
		    else
		      key.put(kval, (int)key_info.len);
		}

		// for destructive sort that preserve the oid for
		// recs: we need to record the serial number as well
		if (sp.keep_lid) {
		    // remove the association from lid index
		    W_DO(lid->remove(sp.lvid, r->tag.serial_no));
		    key.put((char*)&r->tag.serial_no, sizeof(serial_t));
		}
		    
		// another hack: since the sort stream stores keys at
		// the header of intermediate rec, we have to append 
		// original header at end of key.
		if (hlen>0 && !key_info.derived) {
		    key.put(hdr, hlen);
		}

		W_DO ( sort_stream.file_put(key, rec, rlen, hlen, &r->tag) );
            }

            W_DO( fi->next_page(pid, eof) );
	 }

         // flush each run
         W_DO ( sort_stream.flush_run() );
      }
    }

#ifndef OLD_SORT
    last_lg_page.unfix();
#endif


    if (once) {
	// get the sfid
	sfid = sort_stream.sd->tmp_fid;
	sort_stream.sd->tmp_fid = stid_t::null;

	if (sfid == stid_t::null) {
	    // empty file, creating an empty file
	    W_DO ( _create_file(vid, sfid, property, logical_id) );
	}
    } else {
        // get sorted stream, write to the final file
        W_DO ( _create_file(vid, sfid, property, logical_id) );

#ifndef OLD_SORT
	file_p last_page;
#endif

	if (!sort_stream.is_empty()) { 
    	    char* tmp_buf = new char[1000];
    	    w_assert1(tmp_buf);
    	    w_auto_delete_array_t<char> auto_del_buf(tmp_buf);

            bool eof;
	    uint4 blen;
	    vec_t key, hdr, rec;
	    W_DO ( sort_stream.file_get_next(key, rec, blen, eof) );

	    uint offset = sp.keep_lid ? 
				kinfo.len+sizeof(serial_t) : kinfo.len;
	    serial_t serial_no = serial_t::null;

            W_COERCE( dir->access(sfid, sdesc, NL) );
            w_assert1(sdesc);

    	    while (!eof) {
		uint hlen;
		if ((hlen=(uint)(key.size()-offset))>0 || sp.keep_lid) {
	    	    key.copy_to(tmp_buf, key.size());
		}
		if (hlen>0) {
	    	    hdr.put(tmp_buf+offset, hlen);
		}
		if (sp.keep_lid) {
		    memcpy((char*)&serial_no, tmp_buf+kinfo.len,
				sizeof(serial_t));
		} 
#ifdef OLD_SORT
		W_DO ( fi->create_rec(sfid, hdr, rec.size(), rec, 
			serial_no, *sdesc, rid) );
#else
		W_DO ( fi->create_rec_at_end(sfid, hdr, rec.size(), rec, 
			serial_no, *sdesc, last_page, rid) );
#endif

		if (serial_no != serial_t::null) {
		    W_DO(lid->associate(sp.lvid, serial_no, rid));
		}

		if (blen>rec.size()) {
	    	    // HACK:
	    	    // for large object, we need to reset the tag
	    	    // because we only did a shallow copy of the body
	    	    W_DO( fi->update_rectag(
				rid, blen, lgrec_type(rec.size())) );
		}
		key.reset();
		hdr.reset();
		rec.reset();
		W_DO ( sort_stream.file_get_next(key, rec, blen, eof) );
    	    }
  	}
    }

    // destroy input file if necessary
    if (sp.destructive) {
	if (large_obj) {
	    W_DO ( _destroy_n_swap_file(fid, sfid) );
	} else {
	    W_DO ( _destroy_file(fid) );
	}
    } else {
	if (large_obj) {
	    W_DO ( _destroy_n_swap_file(lg_fid, sfid) );
	} else {
	    W_DO ( _destroy_file(lg_fid) );
	}
    }

    return RCOK;
}

rc_t
ss_m::sort_file(const stid_t& fid, // I - input file id
        vid_t vid,                 // I - volume id for output file
        stid_t& sfid,              // O - output sorted file id
	sm_store_property_t property,
        const key_info_t& key_info,// I - info about sort key
                                   //     (offset, len, type...)
        int run_size,              // I - # pages each run
        bool unique,		   // I - eliminate duplicates?
        bool destructive,	   // I - destroy the input file?
        const serial_t& serial)    // I - serial number for logical id
{
    SM_PROLOGUE_RC(ss_m::sort_file, in_xct, 0);
    W_DO(_sort_file(fid, vid, sfid, property, key_info, run_size,
			unique, destructive, serial, lvid_t::null));
    return RCOK;
}

int
operator>(run_scan_t& s1, run_scan_t& s2)
{
    // get length and keys to be compared
    w_assert1(s1.cur_rec && s2.cur_rec);

    int len1, len2;
    if (((s1.kinfo.len == 0) && 
	 (s1.kinfo.type == key_info_t::t_string)) ||
	(s1.kinfo.type == key_info_t::t_custom)) {
      // variable size, use hdr size
      len1 = s1.cur_rec->hdr_size(), len2 = s2.cur_rec->hdr_size();
    } else {
      len1 = len2 = (int)s1.kinfo.len;
    }

    return (s1.cmp(len1, s1.cur_rec->hdr(), len2, s2.cur_rec->hdr(), 
		   s1.cdata) > 0);
}

