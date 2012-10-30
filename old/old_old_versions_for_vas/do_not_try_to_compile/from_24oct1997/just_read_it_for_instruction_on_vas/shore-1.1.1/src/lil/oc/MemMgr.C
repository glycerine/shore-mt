/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// MemMgr.C
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/MemMgr.C,v 1.28 1995/10/12 19:56:22 nhall Exp $ */

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma implementation "MemMgr.h"
#pragma implementation "CacheBHdr.h"
#pragma implementation "OTEntry.h"
#endif

#include "MemMgr.h"
#include "SH_error.h"
#include "OTEntry.h"
#include "debug.h"
#include "ObjCache.h"

// a hard core assert
#define massert(val) ((val)?0:abort())
// Implementation of CacheBHdr methods.
void 
CacheBHdr::add_to_pagelist(CacheBHdr * linkpt)
// "this" is the head of cir. list; add in linkpt.
// note that if "this" == linkpt, linkpt list may be hosed.
{
	linkpt->page_link = this->page_link;
	this->page_link = linkpt;
}

void 
CacheBHdr::clear_pagelist() // clear all pagelink pointers on cache
// objs. linked to "this" node.
{
	CacheBHdr *cpt= this;
	while (cpt) {
		CacheBHdr *npt = cpt->page_link;
		cpt->page_link = 0;
		cpt = npt;
	}
}

int 
CacheBHdr::can_free_page()
// walk through the page list and see if
// delete this object from the page link list.
// can
{
	CacheBHdr *first_cpt= this;
	CacheBHdr *cpt = this;
	while ( cpt)
	{
		// this is the same predicate as can_free for unlinked pages.
		// but we can't call can_free directly or we get recursive loop.
		if ((cpt->pin_count<=0) 
			&& (cpt->used_bit==0 || cpt->ref_bit==0))
		{
			if (cpt->page_link == this) // have gone around list
			// we've check all the elements, so return true
				return 1;
		}
		else
			return 0; // can't free cpt, and hence page containing "this".
		cpt = cpt->page_link;
		massert(cpt != 0);
	}
	// if we got here, we found something we can't free.
	// but we can't acutally get here...
	return 0;
}

void
CacheBHdr::delete_from_pagelist()
// if this block is on a pagelist, unlink it.
{
	// since the list is singly linked, we need to find the node which
	// points to "this"
	if (this->page_link == 0)
		return; // nothing to do.
	CacheBHdr *cpt = this;
	while (cpt->page_link != this)
	{
		massert(cpt!=0);
		cpt = cpt->page_link;
	}
	// unlink this by making its prececessor point to the next node.
	cpt->page_link = this->page_link;
	this->page_link = 0;
}

// reuse will dettach a memory block that is freed but still attached
// to the oc loid table for possible reclamation so that it may no
// longer be reclaimed.
void
MemMgr::reuse(CacheBHdr *p1)
{
	if (p1->alloced_bit) {
		if (p1->page_link)
			p1->delete_from_pagelist();
		if (p1->otentry && p1->otentry->obj && get_brec(p1->otentry->obj) == p1)
		{
			OTEntry *ote = p1->otentry;
			if (ote->type)
			// this frees any malloc'd space attached to the obj.
				ote->type->__apply(DeAllocate,ote->obj);
			ote->obj = 0;
		}
	}
	p1->alloced_bit = 0;
}

// reclaim will reallocate a freed but not reused block back to the ote
// that it was last used for.
void
MemMgr::reclaim(CacheBHdr *p1)
{
	if ((char*)p1 >=space && (char*)p1 < space+total_bytes)
	{
		p1->mark_used();
		alloc_bytes += p1->osize ;
		++alloc_blocks;
		free_bytes -= p1->osize;
		--free_blocks;
		massert(alloc_bytes + free_bytes == total_bytes);
		massert(alloc_blocks + free_blocks == num_blocks);
		++num_allocs; // for debugging purposes
		sum_bytes += p1->osize;
	}
}

void 
MemMgr::coalesce(CacheBHdr * p1, CacheBHdr *p2)
// p1 and p2 must point to adjacent space; merge the 2 blocks
// into 1
{
	p1->osize += p2->osize;
	--num_blocks;
	--free_blocks;
	massert(p1->is_free()&& p2->is_free());
	massert(alloc_bytes + free_bytes == total_bytes);
	massert(alloc_blocks + free_blocks == num_blocks);
	reuse(p2); // if pref. alloced
	if (last_alloced==p2)
		last_alloced = p1;
	if (next_cachept==p2)
		next_cachept= get_next(p1);
}

void 
MemMgr::set_osize(CacheBHdr *p1, int blen)
// check if we should split.  If an allocation of size blen would leave
// space for at least a full cache header, split the block , making the
// first block blen long and let the 2nd block hold any remaining
// space.
{
	massert(p1->is_free() && p1->osize >= blen);
	free_bytes -= p1->osize;
	--free_blocks;
	if (p1->osize >= blen + sizeof(CacheBHdr))
	// then, sufficient space is available to make a 2nd block.
	{
		CacheBHdr * p2 = (CacheBHdr *)(caddr_t(p1)+blen);
		p2->osize =  p1->osize - blen;
		p2->mark_free();
		p2->alloced_bit = 0;
		p2->pin_count= 0; // may have been garbage
		p2->otentry = 0;
		p2->page_link = 0;
		free_bytes += p2->osize;
		++free_blocks;
		p1->osize = blen;
		++num_blocks;
	}
	alloc_bytes += p1->osize ;
	++alloc_blocks;
	massert(alloc_bytes + free_bytes == total_bytes);
	massert(alloc_blocks + free_blocks == num_blocks);
}	

caddr_t 
MemMgr::allocate(CacheBHdr *p1, int blen)
// block is about to be allocated; mark and trim it appropriately.
{
	// if allocation was reclaimable, clear the entry now.
	reuse(p1);
	set_osize(p1,blen);
	p1->mark_used();
	p1->pin_count=0;
	p1->ref_bit = 0;
	p1->ref2_bit = 0;
	sum_bytes += p1->osize;
	++num_allocs;
	return caddr_t(p1);
}

// mark free so block is available for reuse; may be reclaimed untill
// it is actually reallocated to another otentry. (or its connection
// to an ote is broken by reuse.
void
MemMgr::deallocate(CacheBHdr *p1)
{
	p1->mark_free();
	alloc_bytes -= p1->osize ;
	--alloc_blocks;
	free_bytes += p1->osize;
	++free_blocks;
	massert(alloc_bytes + free_bytes == total_bytes);
	massert(alloc_blocks + free_blocks == num_blocks);
}
	


caddr_t
MemMgr::old_get_free_space(int nbytes)
{
	// iterate through all cache blocks looking for a free block
	// of size >= nbytes.
	// always need space for header.
	if (nbytes<sizeof(CacheBHdr))
		nbytes = sizeof(CacheBHdr);

	// first, always round up to sizeof(double)
	nbytes = roundup(nbytes,sizeof(double));

	// first, go from curpt to end of block.
	CacheBHdr * checkpt = get_next(last_alloced);
	int check_limit = num_blocks;
	for(int check_blocks=0;
		check_blocks < check_limit; 
		checkpt= get_next(checkpt),check_blocks++)
	{
		if (checkpt==next_cachept) // push next_cachept forward
			clear_space(total_bytes/4); 
			
		if (checkpt->is_free() )
		{
			// first, scan ahead and see if we can coalesce anything
			CacheBHdr * nextpt;
			while((nextpt = get_next(checkpt) )
				&& nextpt > checkpt  // next block is contiguous
				&& nextpt->is_free()) // block is available.
			{
				if (nextpt==next_cachept) // push next_cachept forward
					clear_space(total_bytes/4); 
				if ( nextpt > checkpt  // next block is contiguous
					&& nextpt->is_free()) // block is available.
				{
					++check_blocks;
					coalesce(checkpt,nextpt);
				}
				else
					break; // can't continue.
			}
			if (checkpt->osize>=nbytes)
			{
				last_alloced = checkpt;
				return allocate(checkpt,nbytes);
			}
		}
	}
	// we weren't able to allocate anything,
	// so return null
	return 0;
}

caddr_t
MemMgr::get_free_space(int nbytes)
{
	// iterate through all cache blocks looking for a free block
	// of size >= nbytes.
	// always need space for header.
	// on first pass, we look for first fit on 
	if (nbytes<sizeof(CacheBHdr))
		nbytes = sizeof(CacheBHdr);

	// first, always round up to sizeof(double)
	nbytes = roundup(nbytes,sizeof(double));

	// first, go from curpt to end of block.
	CacheBHdr * checkpt = get_next(last_alloced);
	//int check_limit = num_blocks ; 
	// go back to old make_space techniqe.
	int check_limit = num_blocks * 3; 
	int check_blocks = 0;
	// at most look at everything 3 times (zowie); once without
	// clearing ref bits for first fit; once to clear ref bits
	// (perhaps), then a final pass that better succeed or else
	for(;
		check_blocks < check_limit; 
		checkpt= get_next(checkpt),check_blocks++)
	{
		if (checkpt==next_cachept) // push next_cachept forward
			clear_space(total_bytes/4); 
		if (checkpt->is_free())
		{
			// first, scan ahead and see if we can create enough 
			// contiguous space to do nbyte allocation.
			CacheBHdr * nextpt = checkpt;
			int avail_bytes = checkpt->osize;
			int nblocks = 1;
			while( avail_bytes < nbytes
				&& (nextpt = get_next(nextpt) )
				&& nextpt > checkpt  // next block is contiguous
				&& nextpt->is_free()) // block can be made available.
			{
				avail_bytes += nextpt->osize;
				if (nextpt==next_cachept) // push next_cachept forward
					clear_space(total_bytes/4); 
				++nblocks;
			}
			if (avail_bytes>=nbytes)
			// the scan ahead found enough space, so
			// we can allocate starting with checkpt block,
			// first uncache and coallesce as necessary, then allocate.
			{
				CacheBHdr *rel_pt = checkpt;
				int i;
				// now, coalesce the blocks from check_pt through 
				// nextpt together.
				for (i=1,rel_pt = get_next(checkpt); i<nblocks; 
					++i,rel_pt = get_next(rel_pt))
				{
					coalesce(checkpt,rel_pt);
				}
				// finally, allocate from checkpt.
				last_alloced = checkpt;
				return allocate(checkpt,nbytes);
			}
			else 
			// bump checkpt up to the last nextpt we looked at.
			// which is either not able to be freed or non contiguos.
			{
				checkpt = nextpt;
			}
		}
	}
	// we weren't able to allocate anything,
	// so return null
	return 0;
}


caddr_t 
MemMgr::make_space(int nbytes)
// this is called if we weren't able to allocate through the get_free_space
// routine; at this point, start doing the ref count bit.
{
	if (nbytes<sizeof(CacheBHdr))
		nbytes = sizeof(CacheBHdr);
	nbytes = roundup(nbytes,sizeof(double));
	CacheBHdr *checkpt = next_cachept;
	int check_limit = num_blocks * 2;
	// check each block twice at most (once to clear ref bit, once to uncache)
	// the loop termination condition could be made cleaner...
	for(int check_blocks=0;
		check_blocks < check_limit; 
		checkpt= get_next(checkpt),check_blocks++)
	{
		if (checkpt->can_free())
		{
			// uncache this block and see if we can build it into
			// a block of sufficient size
			CacheBHdr * nextpt;
			uncache(checkpt,true); // force uncaching
			// if this block is too small,
			// try to expand the checkpt block by releasing
			// following blocks.
			// next, add
			while((nextpt = get_next(checkpt) )
				&& nextpt > checkpt  // next block is contiguous
				&& checkpt->osize <nbytes
				&& nextpt->can_free()
			)
			{
				if (!nextpt->is_free())
					uncache(nextpt,true);
				++check_blocks;
				coalesce(checkpt,nextpt);
			}
			// at this point, check if the current block is big
			// enough; if it is, return the (perhaps split)
			// block;
			if (checkpt->osize>=nbytes)
			{
				next_cachept = get_next(checkpt);

				return allocate(checkpt,nbytes);
			}
		}
		// we couldn't make a block of sufficient size,
		// so clear the ref bits // and continue to the
		// next block
		checkpt->clear_ref();
	}
	// at this point, we've tried to free everything and failed to
	// make sufficient space, so give up .
	return 0;
}

void
MemMgr::clear_space(int nbytes)
// this is a new  variant of make_space; the new protocol is to
// push the deallocation routine out ahead of the allocation
// routine to decrease the likelyhood of wrapping around unnecessarily
// this routine is called to keep the
// this is called if we weren't able to allocate through the get_free_space
// routine; at this point, start doing the ref count bit.
{
	if (nbytes<sizeof(CacheBHdr))
		nbytes = sizeof(CacheBHdr);
	nbytes = roundup(nbytes,sizeof(double));
	CacheBHdr *checkpt = next_cachept;
	int bytes_checked = 0;
	// the loop termination condition could be made cleaner...
	while (bytes_checked < nbytes)
	{
		if (checkpt->can_free())
		{
			// uncache this block and see if we can build it into
			// a block of sufficient size
			CacheBHdr * nextpt;
			uncache(checkpt, false);  // optional uncache;
			// may remain resisdent due to page considerations.
		}
		checkpt->clear_ref();
		bytes_checked += checkpt->osize;
		next_cachept = checkpt = get_next(checkpt);
		// we couldn't make a block of sufficient size,
		// so clear the ref bits // and continue to the
		// next block
	}
}

int do_old_getspace = 0;

w_rc_t
MemMgr::alloc(int nbytes, caddr_t &addr)
{
	addr = 0;
	if (nbytes < total_bytes/4) // try to allocate from our pool
	{
		//if (((total_bytes-alloc_bytes)>nbytes) &&
		//	(alloc_bytes<.9* total_bytes))
		// try to allocate space without uncaching anything.
		// POLICY HERE: before trying to do the first fit allocation
		// that get_free_space does, clear some ref bits/ do some
		// uncaching if we have high memory usage,
		// HIGH MEMORY use is here defined as 90% allocated;
		// we clear 1/4 of the pool each time we allocate at
		// that usage level.
		if (alloc_bytes > total_bytes * .9) // more that 90 % allocated
			clear_space(total_bytes/4); 
			// clear ref bits or uncache over 25 %  of space.
		if (do_old_getspace)
			addr = old_get_free_space(nbytes); //look for available space
		else
			addr = get_free_space(nbytes); //look for available space
		if (addr==NULL)
		{
			// fprintf(stderr,"calling make_space??\n");
			// we shouldn't really need to do make_space any more...
			addr= make_space(nbytes);// try to create space by uncaching.
		}
	}
	if (addr==NULL)
	{
		addr = new char[nbytes];
		if (addr)
		{
			ex_bytes += nbytes;
			++ ex_blocks ;
		}
	}
	// if batching is on and we had to write anything back to free space,
	// flush the batch.
	if (myoc->batch_active)
	{
		w_rc_t rc;
		if (rc= myoc->end_batch())
			return rc;
	}
	if(addr)
		return RCOK;
	return RC(SH_OutOfMemory);
}

void MemMgr::free(caddr_t p)
{
	CacheBHdr *free_pt = (CacheBHdr *)p;
	if (p >=space && p < space+total_bytes)
	// p is in the range of our memory block
	{
		if (free_pt->page_link)
		// just get rid of the links
			free_pt->clear_pagelist();
		uncache(free_pt,true); // really uncache it.
		// but, really free it..
		reuse(free_pt);
	}
	else // it must have been allocated outside our pool, so put it back
	// to regular free pool
	{
		ex_bytes -= free_pt->osize;
		--ex_blocks;
		delete [] p;
	}
}


void
MemMgr::set_mem_limit(int x)
{
	total_bytes = x;
}

MemMgr::MemMgr():
	myoc(0),
	total_bytes(0),
	num_blocks(0),
	alloc_blocks(0),
	alloc_bytes(0),
	free_bytes(0),
	free_blocks(0),
	ex_bytes(0),
	ex_blocks(0),
	num_allocs(0),
	num_uncached(0),
	sum_bytes(0),
	space(0),
	curpt(0),
	last_alloced(0),
	next_cachept(0)
{
}

void
MemMgr::init(ObjCache * me)
// set the initial state.
{
	myoc = me;
	space = new char[total_bytes]; // memory pool
#ifdef PURIFY
	if(purify_is_running()) {
		memset(space, '\01', total_bytes); // for debugging
	}
#endif
	if (getenv("OLD_MMG"))
		do_old_getspace = 1;
	reset();
}

void
MemMgr::reset(void)
// reset the initial state; don't reallocate the space but zero 
// all the counters.
{
	// first, set one free mem block as in init()
	curpt = (CacheBHdr *)space; // current block to look at.
	curpt->osize = total_bytes;
	curpt->mark_free();
	curpt->alloced_bit = 0; 
	curpt->pin_count = 0; 
	curpt->otentry = 0;
	curpt->page_link = 0;
	num_blocks = 1; // current number of blocks.
	// got to start somewhere with these.
	last_alloced = curpt; // last block allocated.
	next_cachept = curpt;
	alloc_blocks = 0; // blocks allocated
	alloc_bytes = 0; // bytes allocated.
	free_blocks = 1;
	free_bytes = total_bytes;
	ex_bytes = 0;
	ex_blocks = 0;
};

void
MemMgr::audit()
{
// check that the data structures are consistent.
	long tbytes = 0; // check totat_bytes
	long nblocks = 0; // check number of blocks
	long ablocks = 0; // check number of blocks allocated
	long abytes  = 0; // check # bytes allocated.
	int allocpt_found = 0;
	int nextc_found = 0;
	CacheBHdr * bpt;
	bpt = (CacheBHdr *)space;
	do 
	{
		tbytes += bpt->osize;
		++nblocks;
		// if (!bpt->is_free()) // block in used
		// oops, is_free also looks at pin_count, which isn't part
		// of alloc accounting.
		if (bpt->used_bit)
		{
			++ablocks;
			abytes += bpt->osize;
#ifdef oldcode
			if (bpt->otentry ) {
				if (bpt->otentry->obj != caddr_t(bpt + 1))
					fprintf(stderr,"memmgr error: otentry %x:obj %x, expected %x\n",bpt->otentry,bpt->otentry->obj,bpt+1);
			}
			else
				fprintf(stderr,"cache block %x has no otentry\n",bpt);
#endif
		}
		if (bpt==last_alloced) allocpt_found=1;
		if (bpt==next_cachept) nextc_found = 1;
		bpt = get_next(bpt);
	} while (bpt != (CacheBHdr *)space);
	if ((total_bytes != tbytes) || (num_blocks != nblocks)
		|| (alloc_blocks != ablocks)
		|| (alloc_bytes != abytes)
		|| !allocpt_found 
		|| !nextc_found
		)
	{
		fprintf(stderr,"memory manager audit error\n");
		fprintf(stderr,"expected bytes %8d blocks %4d allocated bytes %8d blocks %4d\n",
			total_bytes, num_blocks, alloc_bytes, alloc_blocks);

		fprintf(stderr,"found    bytes %8d blocks %4d allocated bytes %8d blocks %4d\n",
			tbytes, nblocks, abytes, ablocks);
		if (!allocpt_found)
			fprintf(stderr,"last_alloced %x not found\n",last_alloced);
		if (!nextc_found)
			fprintf(stderr,"next_cachept %x not found\n",next_cachept);

		return;
		bpt = (CacheBHdr *)space;
		for (int i=0; i<nblocks; i++)
		{
			printf("allocation %4d addr %x offset %6d osize %d\n",
				i,bpt,int(bpt)-int(space),bpt->osize);
			bpt = get_next(bpt);
		}
		// abort();

	}
}

void
print_mmgr(MemMgr *p)
{
	fprintf(stderr,"expected bytes %8d blocks %4d allocated bytes %8d blocks %4d\n",
		p->total_bytes, p->num_blocks, p->alloc_bytes, p->alloc_blocks);

	CacheBHdr * bpt;
	bpt = (CacheBHdr *)p->space;
	for (int i=0; i<p->num_blocks; i++)
	{
		printf("allocation %4d addr %x offset %6d osize %4d r %d%d u %d pinc %d\n",
			i,bpt,int(bpt)-int(p->space),bpt->osize,
			bpt->ref_bit, bpt->ref2_bit,bpt->used_bit,bpt->pin_count);
		bpt = p->get_next(bpt);
	}

}


MemMgr::~MemMgr()
{
	if (space!=0)
		delete [] space;
}
// going out of scope; reclaim any new'd space.
