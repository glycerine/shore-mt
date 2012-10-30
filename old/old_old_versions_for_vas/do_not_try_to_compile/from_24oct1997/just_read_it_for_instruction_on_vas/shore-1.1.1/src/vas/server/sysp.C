/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include "sysp.h"
#if (__GNUC_MINOR__ < 7)
#   include <memory.h>
#else
#   include <std/cstring.h>
#endif /* __GNUC_MINOR__ < 7 */

/*************  SYSP CACHE *******************/

swapped_hdrinfo swapped_hdrinfo::null(0);
const	w_ref_t<swapped_hdrinfo> swapped_hdrinfo::none;

#include "SyspCache_op.i"

const char *SyspCache::stat_names[] = {
#include "SyspCache_msg.i"
};

int 
SyspCache::_find_sysp(const lrid_t &lrid)
{
	dassert(cache_size > 0);
#define FIND_NOT_FOUND -1
	int i,j=FIND_NOT_FOUND;

	if(lrid.serial.is_null()) goto failure;

	if(entries == 0) goto failure;

	/// make i run from last one allocated down to 0
	for(i = next-1; i>=0; i--, inspects++) {
		if( _cache[i].valid &&
			(_cache[i].lrid == lrid)) {
				j=i;
				goto bye_ok;
		}
	}
	/// then from end of table down to next one to be allocated
	for(i = cache_size-1; i >= next; i--, inspects++) {
		if( _cache[i].valid &&
			(_cache[i].lrid == lrid)) {
			j= i;
			goto bye_ok;
		}
	}
failure:
	// not found
	dassert(ncached(lrid)==0);
	return FIND_NOT_FOUND;

bye_ok:
	dassert(j>=0);
	dassert(ncached(lrid)==1);
	dassert(entries > 0);
	return j;
}

void	
SyspCache::uncache(const lrid_t &lrid)
{
	if(cache_size == 0) return;
	int 	i = _find_sysp(lrid);

	if(i>=0) {
		_cache[i].swapped = swapped_hdrinfo::none; // decrements ref count
		_cache[i].valid = false;
		entries--;
		uncaches_found++;
	} else {
		uncaches_notfound++;
	}
	dassert(ncached(lrid)==0);
}

void 
SyspCache::_dump_sysp() const
{
	for(int i = cache_size; i>=0; i--) {
		// this is dump_sysp, -DDEBUG ONLY
		if(_cache[i].valid) {
			/*dump*/ cerr << 
			"i=" << i << " sysp@0x" << ::hex(_cache[i].swapped.printable())
			<< " lrid=" << _cache[i].lrid
			<< " fid=" << _cache[i].fid
			<< " type=" << ::hex(
				(unsigned int)(_cache[i].swapped->sysprops().common.type._low))
			<< endl;
		} else {
			/*dump*/ cerr << 
			"i=" << i << " invalid " << endl;
		}
	}
}

const   swapped_hdrinfo_ptr &
SyspCache:: find(const lrid_t &lrid, OUT(serial_t) fid) 
{
	if(cache_size == 0) return swapped_hdrinfo::none;
	int 		i = _find_sysp(lrid);

	if(i>=0) {
		if(fid) *fid = _cache[i].fid;
		dassert((unsigned int)
			(_cache[i].swapped->sysprops().common.type._low) & 0x1);
		hits++;
		 return _cache[i].swapped;
	} else {
		misses++;
	}
	return swapped_hdrinfo::none;
}

#ifdef DEBUG
int
SyspCache:: ncached(const lrid_t &lrid)
{
	int j=0;
	if(lrid.serial.is_null()) return 0;

	for(int i = 0; i< cache_size; i++) {
		if(_cache[i].valid && (_cache[i].lrid == lrid)) j++;
	}
	if(j>1) {
		cerr <<  // this is -DDEBUG stuff
			"ncached is " << j << endl;
		_dump_sysp();
	}
	return j;
}
#endif

void	
SyspCache::cache(const lrid_t &lrid, const swapped_hdrinfo_ptr &s)
{
	int i;

	if(cache_size==0) return;

#if defined(DEBUG)
	{
		dassert(next >=0 && next <cache_size);
		dassert((unsigned int)(s->sysprops().common.type._low) & 0x1);
		dassert(ncached(lrid)<=1);
	}
#endif

	i = _find_sysp(lrid);
	if(i<0) {
		i = next;
		next = (++next % cache_size);
		replaced_other++;
	} else {
		replaced_same++;
		dassert(i >= 0 && i < cache_size );
	}
	_cache[i].lrid = lrid;
	_cache[i].fid = serial_t::null;
	_cache[i].swapped = s; // counted-ref copy
	_cache[i].valid = true;

#ifdef DEBUG
	{
		bool	has_text, has_indexes;
		assert(ncached(lrid)==1);
	}
#endif

	caches1++;
	entries++;

}
void	
SyspCache::cache(const lrid_t &lrid, const serial_t &id) 
{
	if(cache_size==0) return;

	// this just *adds* the id to the cache; it assumes the
	// sysprops are already there.
	int i = _find_sysp(lrid);
	if(i>=0) {
		dassert(_cache[i].valid);
		dassert(_cache[i].lrid == lrid);
		_cache[i].fid = id;
		caches2++;
	}
	// else the item isn't cached, which is
	// legit- perhaps we are getting a file id from an
	// index or pool object but haven't cached the sysprops for
	// any reason yet. 
}
void 	
SyspCache::invalidate() {
	// delete every item in it
	for(int i=0; i<cache_size; i++) {
		_cache[i].swapped=0;
		_cache[i].valid = false;
	}
	next = 0;
	entries = 0;
	invalidates++;
}

void 
SyspCache::compute()
{
	retrieves = hits + misses;
	uncaches = uncaches_found + uncaches_notfound;
	total = caches1 + caches2 + 
				hits + misses + 
				uncaches_found + uncaches_notfound;

	if(total>0) {
		ratio1 = (float)((float)inspects /(float)total);
		ratio2 = (float)((float)hits /(float)total);
	} else {
		ratio1 = ratio2 = 0.0;
	}
}

void
SyspCache::cstats()
{
	inspects = 
	caches1 = 
	caches2 = 
	uncaches_found = 
	uncaches_notfound = 
	hits = 
	misses = 
	replaced_other = 
	replaced_same = 0;
	invalidates = 0;
	retr_swapped = 0;
}

SyspCache::~SyspCache() { 
	invalidate();
	delete [] _cache;
}

swapped_hdrinfo::swapped_hdrinfo(
	int n// =0
) 
{
#ifdef PURIFY
	if(purify_is_running()) {
		//  to suppress UMC reports:
		memset(&__sysprops, '\0', sizeof(__sysprops));
	}
#endif
	_manual_indexes = 0;
	_nspaces = n;
	if(_nspaces > 0) {
		_manual_indexes =  new serial_t[_nspaces];
	}
}
