/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __SYSP_H__
#define __SYSP_H__

#include "Object.h"
#include <w_statistics.h>

class SyspCache {
private:
		int  	_find_sysp(const lrid_t &lrid);
		int	next;
		typedef struct {
			lrid_t 			lrid;
			swapped_hdrinfo_ptr		swapped;
			serial_t		fid; // for pools
			bool			valid;
		}	spcache;
		spcache *_cache;

#		ifdef DEBUG
		int ncached(const lrid_t &);
#		endif

#include "SyspCache_struct.i"
public:

		void	compute();
		void	uncache(const lrid_t &lrid);
		void 	invalidate();
		void	cache(const lrid_t &lrid, const swapped_hdrinfo_ptr &swapped);
		void	cache(const lrid_t &lrid, const serial_t &id);
		void  	_dump_sysp() const;
		void  	pstats(ostream &) const;
		void  	cstats();
		void 	bypassed() { retr_swapped++; }
		const 	swapped_hdrinfo_ptr	&find(const lrid_t &lrid,
										serial_t *fid_out=0);

		SyspCache(int s=1): cache_size(s) { 
			cstats();
			_cache = new spcache[s];
			invalidate();
		}
		~SyspCache();
};
#endif /* __SYSP_H__ */
