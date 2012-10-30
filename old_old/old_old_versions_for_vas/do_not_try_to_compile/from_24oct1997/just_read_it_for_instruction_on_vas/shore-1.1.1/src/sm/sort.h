/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef _SORT_H_
#define _SORT_H_

typedef int  (*PFC) (uint4 kLen1, const void* kval1, uint4 kLen2, const void*
kval22, const void* cdata);

#ifndef LEXIFY_H
#include <lexify.h>
#endif
#ifndef SORT_S_H
#include <sort_s.h>
#endif

struct sort_desc_t;
class run_scan_t;

#ifdef __GNUG__
#pragma interface
#endif

//
// chunk list for large object buffer
//
struct s_chunk  {

    char* data;
    s_chunk* next;

    NORET s_chunk() { data = 0; next = 0; };
    NORET s_chunk(uint4 size, s_chunk* tail) { 
		      data = new char[size];
		      next = tail;
		};
    NORET ~s_chunk() { delete [] data; };
};

class chunk_mgr_t {

private:
    s_chunk* head;

    void _free_all() { 
	      s_chunk* curr;
	      while (head) {
		curr = head;
		head = head->next;
		delete curr;
	      }
	  };

public:

    NORET chunk_mgr_t() { head = 0; };
    NORET ~chunk_mgr_t() { _free_all(); };

    void  reset() { _free_all(); head = 0; };

    void* alloc(uint4 size) {
	      s_chunk* curr = new s_chunk(size, head);
	      head = curr;
 	      return (void*) curr->data;
	  };
};

class sort_stream_i : public smlevel_top, public xct_dependent_t {

  friend class ss_m;

  public:

    NORET	sort_stream_i();
    NORET	sort_stream_i(const key_info_t& k,
			const sort_parm_t& s, uint est_rec_sz=0);
    NORET	~sort_stream_i();

    void	init(const key_info_t& k, const sort_parm_t& s,
			uint est_rec_sz=0);
    void	finish();

    rc_t	put(const cvec_t& key, const cvec_t& elem);

    rc_t	get_next(vec_t& key, vec_t& elem, bool& eof);

//    bool	is_eof() { return eof; }
    bool	is_empty() { return empty; }
    bool	is_sorted() { return sorted; }

  private:
    void	set_file_sort() { _file_sort = true; _once = false; }
    void	set_file_sort_once(
			sm_store_property_t prop,
			serial_t lid) {
				_file_sort = true; _once = true; 
				_property = prop; _logical_id = lid; }
    rc_t	file_put(const cvec_t& key, const void* rec, uint rlen,
				uint hlen, const rectag_t* tag);
    rc_t	file_get_next(vec_t& key, vec_t& elem, uint4& blen, bool& eof);

    rc_t        flush_run();		// sort and flush one run

    rc_t	flush_one_rec(const record_t *rec, rid_t& rid,
				const stid_t& out_fid, file_p& last_page);

    rc_t	remove_duplicates();	// remove duplicates for unique sort
    rc_t	merge(bool skip_last_pass);

    void	xct_state_changed(
			xct_state_t old_state, 
			xct_state_t new_state);

    key_info_t		ki;		// key info
    sort_parm_t		sp;		// sort parameters
    sort_desc_t*	sd;		// sort descriptor

    bool 		sorted;		// sorted flag
    bool 		eof;		// eof flag
    bool 		empty;		// empty flag
    const record_t*	old_rec;	// used for sort unique
  
    bool		_file_sort;	// true if sorting a file

    int2*		heap;	   	// heap array
    int			heap_size; 	// heap size
    run_scan_t* 	sc;	   	// scan descriptor array	
    uint4 		num_runs;  	// # of runs for each merge
    int			r;	   	// run index

    chunk_mgr_t		buf_space;	// in-memory storage

    // below vars used for speeding up sort if whole file fits in memory
    bool		_once;		// one pass write to result file
    sm_store_property_t _property;	// property for the result file
    serial_t		_logical_id;	// logical id
};


class file_p;

//
// run scans
//
class run_scan_t {
    lpid_t pid;         // current page id
    file_p* fp;         // page buffer (at most fix two pages for unique sort)
    int2   i;           // toggle between two pages
    int2   slot;        // slot for current record
    record_t* cur_rec;  // pointer to current record
    bool eof;         // end of run
    key_info_t kinfo;   // key description (location, len, type)
    int2   toggle_base; // default = 1, unique sort = 2
    bool   single;	// only one page
    bool   _unique;	// unique sort
    PFC cmp;            // The comparison function
    const void *cdata;  // A handle on any extra data the cmp function needs
public:
    NORET run_scan_t();
    NORET ~run_scan_t();

    rc_t init(rid_t& begin, const key_info_t& k, bool unique);
    rc_t current(const record_t*& rec);
    rc_t next(bool& eof);

    is_eof()   { return eof; }

    friend operator>(run_scan_t& s1, run_scan_t& s2);
    friend operator<(run_scan_t& s1, run_scan_t& s2);
};

#endif // _SORT_H_
