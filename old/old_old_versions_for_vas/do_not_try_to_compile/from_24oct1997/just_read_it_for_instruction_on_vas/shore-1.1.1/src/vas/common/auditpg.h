/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef _AUDITPG_H_
#define _AUDITPG_H_
/*
 * $Header: /p/shore/shore_cvs/src/vas/common/auditpg.h,v 1.2 1995/04/24 19:43:49 zwilling Exp $
 */
#include <copyright.h>
#define FILE_C
#include <sm_app.h>

void audit_pg( const shore_file_page_t *pg);

// ************ rec_i *****************************
class rec_i {
private:
    const shore_file_page_t     *pg;
    int             _slot;

public:
	rec_i(const shore_file_page_t *p) : pg(p) {
		_slot = pg->slot_count();
		dassert(pg->slot_count() == pg->nslots);
		dassert(pg->nslots< sizeof(shore_file_page_t));
	}
	int	slot()	{ return pg->slot_count() - _slot; }

	record_t *next() {
		FUNC(next);
		record_t *rec;
		while(--_slot > 0) {
			// offset should be -1 (not in use)
			// or 8-byte aligned (in use)
			DBG(<<"page " << pg->pid.page << ", slot " << _slot );

			assert((pg->slot[- _slot].offset == -1) ||
				((pg->slot[- _slot].offset & 0x7)==0));
			assert(pg->slot[- _slot].length <= page_s::data_sz);

			if(rec = pg->rec_addr(_slot)) 
				return rec;
		}
		DBG(<<"**end scan of records** for page " << pg->pid.page );
		return (record_t *)0;
	}
	record_t *next_small() {
		FUNC(next_small);
		record_t *rec;
		// slot 0 is reserved by SM -- don't look at that
		while(--_slot > 0) {
			rec = pg->rec_addr(_slot);
			if( rec && rec->is_small()) {
				DBG(<< " RETURNED  page " << pg->pid.page << ", slot " << _slot 
				<< " serial=" << rec->tag.serial_no );
				return rec;
#ifdef DEBUG
			} else {
				// rec might not be there
				DBG(<< " SKIPPED page " << pg->pid.page <<" slot "  << _slot);
				if(rec) {
					DBG(<< " serial=" << rec->tag.serial_no );
				}
				if(rec) {
					if(rec->is_large()) {
						DBG( << " : large object");
					} else {
						DBG( << " : reason unknown");
					}
				} else  {
					DBG( << " : not in use");
				}
#endif
			}
		}
		DBG(<<"**end small scan of records** for page " << pg->pid.page );
		return (record_t *)0;
	}
};
#endif
