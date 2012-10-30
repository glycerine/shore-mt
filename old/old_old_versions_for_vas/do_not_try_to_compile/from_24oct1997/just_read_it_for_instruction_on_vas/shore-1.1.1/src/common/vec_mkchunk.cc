/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/vec_mkchunk.cc,v 1.5 1997/06/15 02:36:11 solomon Exp $
 */

#ifdef __GNUC__
     #pragma implementation
#endif

#define VEC_T_C
#include <stdlib.h>
#include <iostream.h>
#include "w_base.h"
#include "w_minmax.h"
#include "basics.h"
#include "dual_assert.h"
#include "vec_t.h"
#include "umemcmp.h"
#include "debug.h"


/////////////////////////////////////////////////////
// "result" is a vector with size() no larger than 
// maxsize, whose contents are taken from the part
// of *this that's left after you skip the first
// "offset" bytes of *this.
// CALLER provides result, which is reset() right
// away, and re-used each time this is called.
//
// The general idea is that this allows you to take
// a vector of arbitrary configuration (in the context
// of writes, say) and break it up into vectors of
// "maxsize" sizes so that you can limit the sizes of
// writes.
/////////////////////////////////////////////////////
void
vec_t::mkchunk(
	int				maxsize,
	int				offset, // # skipped
	vec_t			&result // provided by the caller
) const
{
	int i;

    dual_assert1( _base[0].ptr != zero_location );

	DBG(<<"offset " << offset << " in vector :");
	result.reset();

	// return a vector representing the next
	// maxsize bytes starting at the given offset
	// from the data represented by the input vector
	int		first_chunk=0, first_chunk_offset=0, first_chunk_len=0;
	{
		// find first_chunk
		int skipped=0, skipping;

		for(i=0; i<this->count(); i++) {
			skipping = this->len(i);
			if(skipped + skipping > offset) {
				// found
				first_chunk = i;
				first_chunk_offset = offset - skipped;
				first_chunk_len = skipping - first_chunk_offset;
				if(first_chunk_len > maxsize) {
					first_chunk_len = maxsize;
				}

		DBG(<<"put " << ::dec((unsigned int)this->ptr(i)) << 
			"+" << first_chunk_offset << ", " << first_chunk_len);

				result.put(this->ptr(i)+first_chunk_offset,first_chunk_len);
				break;
			}
			skipped += skipping;
		}
		if(first_chunk_len == 0) return;
	}

	if(first_chunk_len < maxsize) {
		// find next chunks up to the last
		int used, is_using ;

		used = first_chunk_len;
		for(i=first_chunk+1; i<this->count(); i++) {
			is_using = this->len(i);
			if(used + is_using <= maxsize) {
				// use the whole thing
				used += is_using;

				DBG(<<"put " << ::dec((unsigned int)this->ptr(i)) << ", " << is_using);
				result.put(this->ptr(i),is_using);
			} else {
				// gotta use part
				result.put(this->ptr(i),maxsize-used);
				used = maxsize;
				break;
			}
		}
	}
}
