
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// MemMgr.h
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/MemMgr.h,v 1.19 1995/09/22 22:26:16 schuh Exp $ */

#ifndef _MEMMGR_H_
#define _MEMMGR_H_

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma interface
#endif

#include "OCTypes.h"
#include "CacheBHdr.h"
#if (__GNUC_MINOR__ < 7)
#   include <memory.h>
#else
#   include <std/cstring.h>
#endif /* __GNUC_MINOR__ < 7 */


class MemMgr
{
  //private:
  public:
	// internal state
	ObjCache *myoc;
	long total_bytes; // total bytes available
	long num_blocks; // current number of blocks.
	long alloc_blocks; // blocks allocated
	long alloc_bytes; // bytes allocated.
	long free_bytes;  // do some double entry bookkeeping.
	long free_blocks; // ditto.
	long ex_bytes;	  // externally allocated space
	long ex_blocks;   // externally allocated blocks.
	// some more statistics
	long num_allocs;
	long num_uncached; // in make_space
	long sum_bytes; // total bytes ever allocated.
	caddr_t space; // memory pool
	CacheBHdr * curpt; // current block to look at.
	CacheBHdr * last_alloced; // last block allocated.
	CacheBHdr * next_cachept; // next block to look at when uncaching
	caddr_t get_free_space(int nbytes); // get nbytes if free
	caddr_t old_get_free_space(int nbytes); // get nbytes if free
	caddr_t make_space(int nbytes); // make nbytes available
	void clear_space(int nbytes); // try to clear space at next_cachept
	CacheBHdr *get_next (CacheBHdr *cpt) // get the next block after
	// the cpt one.
	{
		caddr_t next_pt = caddr_t(cpt) + cpt->osize;
		if (next_pt  >= space + total_bytes)
		// we've gone off the end...
			return (CacheBHdr *)space;
		else
			return (CacheBHdr *)next_pt;
	}
	void reuse(CacheBHdr * p1);
	void reclaim(CacheBHdr *p1);
	void coalesce(CacheBHdr * p1, CacheBHdr *p2);
	void set_osize(CacheBHdr *p1, int blen);
	caddr_t allocate(CacheBHdr *p1, int blen);
	void deallocate(CacheBHdr *p1);
  public:

	void init(ObjCache *);
	void set_mem_limit(int);
	void uncache(CacheBHdr * cpt, bool force);
	void old_uncache(CacheBHdr * cpt);
	void audit();

	// allocate nbytes bytes
	w_rc_t alloc(int nbytes, caddr_t &addr);

	// free previously allocated space starting at p
	void free(caddr_t p);

	// Clear out the entire contents of the object cache (currently
	// unimplemented)
	void reset(void);
	MemMgr();  // initialize a few things; most things done from init.
	~MemMgr(); // reclaim storage.
};

#endif	/* _MEMMGR_H_ */
