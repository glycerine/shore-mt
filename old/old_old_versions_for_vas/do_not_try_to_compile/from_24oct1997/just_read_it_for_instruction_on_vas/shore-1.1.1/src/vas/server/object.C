/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/object.C,v 1.69 1997/01/24 16:48:08 nhall Exp $
 */
#include <copyright.h>

#include "Anonymous.h"
#include "sm_app.h"


#include <auditpg.h>
void
audit_pg(
	const shore_file_page_t	*pg
)
{
	FUNC(audit_pg);
	w_rc_t		smerrorrc;
	int			_slot = pg->nslots;
	int         nslots=0;

	assert(sizeof(page_s) >  pg->nslots);

	while ( --_slot >= 0 ) {
			// offset should be -1 (not in use)
			// or 8-byte aligned (in use)

			assert((pg->slot[-_slot].offset == -1) ||
				((pg->slot[-_slot].offset & 0x7)==0));
			assert(pg->slot[-_slot].length <= page_s::data_sz);
			if((pg->slot[-_slot].offset & 0x7)==0) {
				nslots ++;
			}
	}
	assert(nslots>0);

	rec_i		iter(pg);
	record_t	*rec;
	int			aslots=0;
	int			sslots=0;
	int			lslots=0;
	lvid_t		p;
	serial_t	s;
	lrid_t		res;

	while (rec = iter.next()) {
		aslots++;
		if(rec->is_large())  {
			lslots++;
			continue;
		}
		sslots++;
		s = rec->tag.serial_no;
		// only because we put it there
		p = *((lvid_t *)(&pg->lsn2));
		DBG(<< "store.page= " 
			<<  pg->pid.store() << "." << pg->pid.page );
		DBG(<<"small record: " << s);
		assert(iter.slot()>=aslots);

		if SMCALL(convert_to_local_id(p, s, res.lvid, res.serial)) {
			assert(0);
		}
		assert(s == res.serial);
		assert(p == res.lvid);
	}
}
#ifdef DEBUG
void
svas_server::audit_pg(
    const 		void 	*vpg
)
{
#ifdef NOTDEF
    const 		page_s 	*pg = (page_s *)vpg;
    int         _slot = pg->nslots;
	int			nslots=0;

	DBG(<< "audit_page " << ::hex((u_long)pg) 
		<< " offset " << ::hex((u_long) ((char *)pg - (char *)shm.base()) )
		<< " from shm base " << ::hex((u_long)shm.base() )
	);

    assert(sizeof(page_s) >  pg->nslots);

    while ( --_slot >= 0 ) {
            // offset should be -1 (not in use)
            // or 8-byte aligned (in use)

            assert((pg->slot[-_slot].offset == -1) ||
                ((pg->slot[-_slot].offset & 0x7)==0));
            assert(pg->slot[-_slot].length <= page_s::data_sz);
			if((pg->slot[-_slot].offset & 0x7)==0) {
				nslots ++;
			}

    }
	assert(nslots>0);
	::audit_pg((const shore_file_page_t *)vpg);
#endif
}
#endif

VASResult		
svas_server::readObj(
	IN(lrid_t) 			obj,	
	ObjectOffset		offset,	
	ObjectSize			requested,	
	LockMode			lock,	
	IN(vec_t)			buf,
	OUT(ObjectSize)		buflen,	
	OUT(ObjectSize)		more,	
	OUT(lrid_t)			snapped, // =NULL snapped ref to obj
	OUT(bool)			pagecached // =NULL 
) 	
{
	VFPROLOGUE(svas_server::readObj);

	errlog->log(log_info, "READ %d.%d.%d (%d/%d) %d", 
		obj.lvid.high, obj.lvid.low, 
		obj.serial.data._low,
		offset, requested, lock
		);

	TX_REQUIRED;
FSTART
	_DO_(_readObj(obj,offset,requested,
		lock,buf,buflen,more,snapped,pagecached));
FOK:
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_readObj(
	IN(lrid_t) 			obj,	
	ObjectOffset		offset,	
	ObjectSize			requested,	
	LockMode			lock,	
	IN(vec_t)			buf,
	OUT(ObjectSize)		buflen,	
	OUT(ObjectSize)		more,	
	OUT(lrid_t)			snapped, // =NULL snapped ref to obj
	OUT(bool)			pagecached // =NULL 
) 	
{
	VFPROLOGUE(svas_server::_readObj);

	errlog->log(log_info, "_READ %d.%d.%d (%d/%d) %d", 
		obj.lvid.high, obj.lvid.low, 
		obj.serial.data._low,
		offset, requested, lock
		);

	CLI_TX_ACTIVE;
FSTART
	const char		*_b = 0;
	smsize_t 	pinned; // # bytes of our 
		// desired range that were pinned
	smsize_t 	first_wanted;
		// offset into pinned range of the first
		// byte of interest
	{
		Object 			object(this, obj.lvid, obj.serial);
		smsize_t		cursize; 
		bool			isAnonymous;
		smsize_t		amt_to_copy;

		{
			// check arguments
			if(buflen==NULL) {
				VERR(OS_BadAddress);
				RETURN SVAS_FAILURE;
			}
			if(more==NULL) {
				VERR(OS_BadAddress);
				RETURN SVAS_FAILURE;
			}
			// OK for snapped to be null

			DBG(<<"bytes requested: " << requested
				<< " buf.size=" << buf.size()
				<< " offset" << (int)offset
			);
		}
		checkflags(false); // not yet xfer time
		{
			// ok-- do the grunge
			// requested could be WholeObject
			// bufsize could be less than size of object
			// and less or more than requested

			_DO_(object.legitAccess(obj_read, lock, \
					&cursize, offset, requested, &isAnonymous) );
			if( requested == WholeObject ) 
				requested = cursize - offset;

			amt_to_copy = lesser(requested, buf.size());

			// offset is index (origin 0)
			// amt_to_copy indicates how many bytes to read

			clrflags(vf_no_xfer);

			if(amt_to_copy > 0) {
				pinned = object.getBody(offset, amt_to_copy, &_b);
				if(pinned == 0) {
					assert(0); // legitAccess should have caught any such errors
					FAIL;
				}
				dassert(_b!=0);
			} else {
				pinned = 0;
				// should be left pinned
				// the following assert also asserts that it's pinned
				// even though the object's size is 0
				// pin it because later we assume it's pinned
				if(!object.is_pinned()) {
					// should never happen because we checked
					// legitAccess
					dassert(0);
					// TODO-- remove this section 
					//
					if(object.getHdr() != SVAS_OK) {
						FAIL;
					}
				}
				dassert(object.is_small());
			}
				
			// pinned is # bytes of the region (that we
			// requested) that are now pinned (possibly more)
			amt_to_copy = lesser(pinned, amt_to_copy);

			if(pinned > 0) {
				// legitAccess should have caught the error
				// if we requested a bad range
				dassert((offset == 0 && object.body_size() == 0)
					|| object.offset_pinned(offset));
				dassert(object.loc_pinned(_b));

				// let first_wanted be the 
				// offset relative to the pinned portion:
				first_wanted = offset - object.start_byte();

				dassert(object.pinned_offset(_b) == first_wanted);
				DBG( <<"offset relative to pinned portion is " << (int)first_wanted);
			} else {
				first_wanted = 0;
			}

			dassert(requested != WholeObject);// should now be object size
					// if it was originally WholeObject

			if(amt_to_copy == requested) {
				// what's pinned covers the requested region
				*more = 0;

				DBG(<<"more= " << *more);
				//
				// Let's see if we want the entire page
				// If it's an anonymous object, do so, unless this
				// is a pseudo-client 
				// 
				if(isAnonymous && 
					object.is_small() && 
					!pseudo_client()) {

					// First... lock the whole page
					if( object.lockpage(::SH)!=SVAS_OK ) {
						VERR(SVAS_SmFailure);
						FAIL;
					}

					lvid_t		  *pretend;

					*more = 0; // let client side figure out if there's more

					setflags(vf_sm_page);

					DBG(<<"copying whole page: "<< ::hex((u_long)page_buf()));
					vec_t wholepage(page_buf(), page_size());

					wholepage.copy_from(object.page(), page_size()); 
						// starting offset 0

					// write the logical volume 
					// id into the lsn2 of the page

					// get warning about alignment requirements:
					struct page_s *p = (struct page_s *)page_buf();
					pretend = (lvid_t *)&(p->lsn2);
					*pretend = obj.lvid;

#ifdef DEBUG
					audit_pg(p);
#endif
					dassert(requested <= pinned);
					dassert(first_wanted + requested <= object.pinned_length());

					*buflen = requested;
				}
			}
			if(!sm_pg_copied()) {
				// one of these reasons:
				//  large object
				//  not anonymous
				//  pseudo-client
				//  pinned < requested
				dassert( 
					object.is_large() || 
					(!isAnonymous) || 
					pseudo_client() || 
					(pinned<requested) );

				// remember, first_wanted is the offset from start_byte
				// here are umpteen ways to assert that the requested
				// region is pinned...
#ifdef DEBUG
	if(first_wanted > 0) {
				dassert(object.offset_pinned(object.start_byte()+first_wanted));
				dassert(object.offset_pinned(object.start_byte()+first_wanted+pinned-1));
	} 
#endif
				dassert(first_wanted <= object.pinned_length());

				// pinned is the # bytes starting with the lower
				// bound of the requested range
				// (not necessarily all of what's really pinned)
				dassert(pinned <= object.pinned_length());

				// first_wanted + requested could be > obj.length()
				if(requested > 0) {
					dassert(object.pinned_offset(_b) == first_wanted);
				}

				setflags(vf_obj_follows);

				// copy contents to given buf from object 
				// copy no more than pinned, and 
				//    no more than the size of the vector

				if(amt_to_copy > 0) {
					buf.copy_from(_b, (smsize_t) amt_to_copy);
				}
				*more = requested - amt_to_copy;
				*buflen = amt_to_copy;
#ifndef notdef
				if (object.is_large()) {
					dassert(buf.len(0) == buf.size());
					// offset is offset from start of object;
					// reqoffset is offset from start requested portion
					ObjectOffset reqoffset = 0;;
					ObjectSize bufroomleft=0;
					vec_t b2;

					while(amt_to_copy>0 && *more>0) {
						reqoffset += amt_to_copy;
						bufroomleft = buf.size() - reqoffset; 

						DBG(<<"newbuf at offset " << offset << " for " 
							<< bufroomleft << " bytes" );

						{ 
							// any room left to copy any more
							b2.reset();

							if(bufroomleft>0) {
								b2.put(buf.ptr(0)+reqoffset, bufroomleft);
							} else {
								amt_to_copy = 0;
								break;
							}
						}

						DBG(<<"pinning at offset " << offset 
							<< " from object; " << reqoffset
							<< " from beginning of request, for "
							<< *more << " bytes" );

						pinned = object.getBody(offset+reqoffset, *more, &_b);
						// make a new vec_t for the new area of buf
						// For now , this ASSUMESS that the
						// lg data area is a contiguous buffer

						amt_to_copy = lesser(pinned, bufroomleft);
						if(amt_to_copy > 0) {
							DBG(<<"copying at reqoffset " << reqoffset 
							<< " for " << amt_to_copy << " bytes" );

							b2.copy_from(_b, (smsize_t) amt_to_copy);
							*more -= amt_to_copy;
							*buflen += amt_to_copy;
						}
					}
				}
#endif
			}
			if(snapped) *snapped = object.snap();

		}
		checkflags(true); // now it's xfer time

		if(pseudo_client()) {
			clrflags(vf_obj_follows);
			setflags(vf_no_xfer);
		}
	}

	DBG( <<"Returning, more = " << (int)(more?(*more):-1)
		<< " #bytes requested= " << (int) requested
		<< " #bytes copied= " << *buflen
	);
FOK:
	if(pagecached) (*pagecached) = false; // TODO- caching pages on server side?
	RETURN SVAS_OK;

FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_lockObj(
	IN(lrid_t) 			obj,	
	LockMode			lock,	
	RequestMode			ok2block  // TODO: use
)
{
	VFPROLOGUE(svas_server::_lockObj);

	CLI_TX_ACTIVE;
FSTART
	{
		smsize_t		cursize;
		Object 			object(this, obj.lvid, obj.serial);

		// TODO: blocking & non-blocking lock requests

		// just like read but we don't get any body
		_DO_(object.legitAccess(obj_lock, lock, &cursize, 0, 0) );
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::lockObj(
	IN(lrid_t) 			obj,	
	LockMode			lock,	
	RequestMode			ok2block  // TODO: use
)
{
	VFPROLOGUE(svas_server::lockObj);
	long t = timeout();

	errlog->log(log_info, "LOCK %d.%d.%d %d", 
		obj.lvid.high, obj.lvid.low, 
		obj.serial.data._low, lock);

	TX_REQUIRED;
FSTART
	if(ok2block == NonBlocking) {
	   set_timeout(WAIT_IMMEDIATE);
	}
	_DO_(_lockObj(obj,lock,ok2block));
FOK:
        set_timeout(t);
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
        set_timeout(t);
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::writeObj(
	IN(lrid_t) 			obj, 
	ObjectOffset		offset,
	IN(vec_t)			newdata
) 	// for user-defined types only
	// get X lock
{
	VFPROLOGUE(svas_server::writeObj);

	errlog->log(log_info, "WRITE %d.%d.%d (%d/%d)", 
		obj.lvid.high, obj.lvid.low, 
		obj.serial.data._low, offset, newdata.size());

	TX_REQUIRED; 
FSTART
	if(newdata.size()>0) {
		Object 		object(this, obj.lvid, obj.serial, ::UD);

		_DO_(object.write(offset, newdata));
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::truncObj(
	IN(lrid_t) 			obj,
	ObjectSize			newlen,	
	ObjectOffset		newtstart,
	bool				zeroed // = true
) 
{
	VFPROLOGUE(svas_server::truncObj);

	errlog->log(log_info, "TRUNC %d.%d.%d (%d/%d) %d", 
		obj.lvid.high, obj.lvid.low, 
		obj.serial.data._low,
		newlen, newtstart);

	TX_REQUIRED; 
FSTART
	{
		Object 		object(this, obj.lvid, obj.serial, ::UD);
		_DO_( object.truncate(newlen, assign, newtstart, zeroed));
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::truncObj(
	IN(lrid_t) 			obj,
	ObjectSize			newlen
) 	
{
	VFPROLOGUE(svas_server::truncObj);

	errlog->log(log_info, "TRUNC %d.%d.%d (%d)", 
		obj.lvid.high, obj.lvid.low, 
		obj.serial.data._low,
		newlen);

	TX_REQUIRED; 
FSTART
	{
		Object 		object(this, obj.lvid, obj.serial, ::UD);
		_DO_( object.truncate(newlen, nochange, 0 )) ;
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_appendObj(
	IN(lrid_t) 			obj,
	IN(vec_t)			data
) 	
{
	VFPROLOGUE(svas_server::_appendObj);

	CLI_TX_ACTIVE; 
	SET_CLI_SAVEPOINT;
FSTART
	if(data.size()>0) {
		Object object(this, obj.lvid, obj.serial, ::UD);
		_DO_( object.append(data, nochange, 0));
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	VABORT;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::appendObj(
	IN(lrid_t) 			obj,
	IN(vec_t)			data
) 	
{
	VFPROLOGUE(svas_server::appendObj);

	errlog->log(log_info, "APPEND %d.%d.%d (%d)", 
		obj.lvid.high, obj.lvid.low, 
		obj.serial.data._low,
		data.size());

	TX_REQUIRED; 
FSTART
	_DO_(_appendObj(obj,data));
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	// _appendObj did abort to savepoint
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_appendObj(
	IN(lrid_t) 			obj,
	IN(vec_t)			data,
	ObjectOffset		newtstart 
	//					set size to "end-of-heap"
) 	
{
	VFPROLOGUE(svas_server::_appendObj);
	CLI_TX_ACTIVE; 
	SET_CLI_SAVEPOINT;
FSTART
	{
		Object object(this, obj.lvid, obj.serial, ::UD);
		// do even if data.size() == 0 because might only
		// change newtstart
		_DO_(object.append(data, assign, newtstart));
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	VABORT;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::appendObj(
	IN(lrid_t) 			obj,
	IN(vec_t)			data,
	ObjectOffset		newtstart 
	//					set size to "end-of-heap"
) 	
{
	VFPROLOGUE(svas_server::appendObj);

	errlog->log(log_info, "APPEND %d.%d.%d (%d/%d)", 
		obj.lvid.high, obj.lvid.low, 
		obj.serial.data._low,
		data.size(), newtstart);

	TX_REQUIRED; 
FSTART
	_DO_(_appendObj(obj,data,newtstart));
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	// _appendObj did abort to savepoint
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::updateObj(
	IN(lrid_t) 			obj,	// -- reg or anon
	ObjectOffset		offset,	// first byte of object
								// to be updated
	IN(vec_t)			wdata,	// data to write
	ObjectSize			newlen, // truncate
	ObjectOffset		newtstart
) 	
{
	VFPROLOGUE(svas_server::updateObj);

	errlog->log(log_info, "UPDATE %d.%d.%d (%d/%d/t:%d/%d)", 
		obj.lvid.high, obj.lvid.low, obj.serial.data._low, 
		offset, wdata.size(), newlen, newtstart);

	TX_REQUIRED; 
FSTART
	{
		Object 		object(this, obj.lvid, obj.serial, ::UD);

		// DO TRUNCATE FIRST BECAUSE THE wdata might be bigger
		// than the current size of the object, and the truncate
		// might be one that increases the object's size
		_DO_( object.truncate(newlen, assign, newtstart));
		if(wdata.size()>0) {
			_DO_(object.write(offset, wdata));
		}
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::updateObj(
	IN(lrid_t) 			obj,	// -- reg or anon
	ObjectOffset		offset,	// first byte of object
								// to be updated
	IN(vec_t)			wdata,	// data to write
	ObjectOffset		aoffset,	// not used on server size
	IN(vec_t)			adata,	
	ObjectOffset		newtstart
) 	
{
	VFPROLOGUE(svas_server::updateObj);

	errlog->log(log_info, "UPDATE %d.%d.%d (%d/%d/a:%d/%d)", 
		obj.lvid.high, obj.lvid.low, obj.serial.data._low, 
		offset, wdata.size(), adata.size(), newtstart);

	TX_REQUIRED; 
FSTART
	{
		Object 		object(this, obj.lvid, obj.serial, ::UD);

		// do even if data.size() == 0 because might only
		// change newtstart
		_DO_( object.append(adata, assign, newtstart));

		// DO APPEND FIRST BECAUSE IT MAKES THE OBJECT BIGGER,
		// and, although it would be a stupid thing to do, a user 
		// should be able to hand us a wdata vec that assumes the
		// final size

		if(wdata.size()>0) {
			_DO_(object.write(offset, wdata));
		}
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult       
svas_server::_offVolRef(
	IN(lvid_t)      fromvol,    // - volume
	IN(lrid_t)      toobj,      // - object
	OUT(lrid_t)   	result
)
{
	VFPROLOGUE(svas_server::_offVolRef);

	CLI_TX_ACTIVE;
FSTART
	serial_t		ref;
	lrid_t			temp;

	if(toobj.serial.is_remote()) {
		// temp = the snapped (far) oid
		_DO_(_snapRef(toobj, &temp));
	} else {
		temp = toobj;
	}
	if(temp.lvid == fromvol) {
		// the real ref is on the same volume as the "from volume",
		// so don't need to make an off-volume reference
		*result = temp;
	} else {
		result->lvid  = fromvol;
		DBG(<<"calling link_to_remote_id("
			<< fromvol << "," 
			<< ref << ","
			<< temp.lvid << ","
			<< temp.serial << ")");
		if SMCALL(link_to_remote_id(fromvol, 
				ref, // this is the result parameter
				temp.lvid, temp.serial)) {

			VERR(SVAS_SmFailure);
			FAIL;
		}
		result->serial  = ref;
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	dassert(status.vasreason != 0);
	RETURN SVAS_FAILURE;
}

VASResult       
svas_server::offVolRef(
	IN(lvid_t)      fromvol,    // - volume
	IN(lrid_t)      toobj,      // - object
	OUT(lrid_t)   	result
)
{
	VFPROLOGUE(svas_server::offVolRef);

	errlog->log(log_info, "OFFVREF %d.%d -> %d.%d.%d",
		fromvol.high, fromvol.low, toobj.lvid.high, toobj.lvid.low, 
		toobj.serial.data._low);

	TX_REQUIRED; 
FSTART
	if(!result) {
		VERR(OS_BadAddress);
		FAIL;
	}
	_DO_(_offVolRef(fromvol, toobj, result));
FOK:
	errlog->log(log_info, "OFFVREF returned %d.%d.%d -> %d.%d.%d",
		result->lvid.high, result->lvid.low, result->serial.data._low,
		toobj.lvid.high, toobj.lvid.low, toobj.serial.data._low);
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult       
svas_server::mkVolRef(
	IN(lvid_t)    onvolume,  
	OUT(lrid_t)   result,
	int			  number // default=1
)
{
	VFPROLOGUE(svas_server::mkVolRef);

	errlog->log(log_info, 
		"VREF %d.%d qty %d", onvolume.high, onvolume.low, number);
	TX_REQUIRED; 
	serial_t		ref=serial_t::null;
FSTART

	if(!result) {
		VERR(OS_BadAddress);
		FAIL;
	}
	if SMCALL(create_id(onvolume, number, ref)) {
		VERR(SVAS_SmFailure);
		FAIL;
	}
FOK:
	result->lvid = onvolume;
	result->serial = ref;

	errlog->log(log_info, "VREF returned %d.%d.%d",
		result->lvid.high, result->lvid.low, result->serial.data._low);

	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult       
svas_server::_snapRef(
	IN(lrid_t)    off, 
	OUT(lrid_t)   result 
)
{
	VFPROLOGUE(svas_server::_snapRef);

	CLI_TX_ACTIVE;
FSTART
	dassert(result != 0);
	DBG( << "snapRef : off=" << off )
	if(off.serial.is_remote()) {

		// result = the snapped (far) oid
		if SMCALL(convert_to_local_id(off.lvid,
				off.serial, result->lvid, result->serial)) {
			VERR(SVAS_SmFailure);
			FAIL;
		}
	} else {
		*result = off;
	}
FOK:
	DBG( << "returning " << *result )
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult       
svas_server::snapRef(
	IN(lrid_t)    off, 
	OUT(lrid_t)   result 
)
{
	VFPROLOGUE(svas_server::snapRef);

	errlog->log(log_info, "SNAP %d.%d.%d", 
		off.lvid.high, off.lvid.low, 
		off.serial.data._low);

	TX_REQUIRED;
FSTART

	if(!result) {
		VERR(OS_BadAddress);
		FAIL;
	}
	_DO_(_snapRef(off, result));

FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_sysprops(
	IN(lrid_t)	target, // for any kind of object
	OUT(SysProps) sys,
	bool		copypage, // in default = false
	LockMode	lock,	// in default = SH
	OUT(bool)	is_unix_file,	// default = NULL (don't care)
	OUT(int)	size_of_sysp,	// default = NULL (don't care)
	OUT(bool)	pagecached	// default = NULL (don't care)
)
{
	VFPROLOGUE(svas_server::_sysprops); 

	CLI_TX_ACTIVE;
FSTART
	SysProps Dummy, *s;

	if(!(s=sys)) {
		s = &Dummy; // grot
			// just in case someone asks for size of sysprops
			// but doesn't want the whole thing
			// VERR(OS_BadAddress);
			// FAIL;
	}

	if(is_unix_file) *is_unix_file = false;

	DBG(<<"sysprops for obj " << target);
	{
		// obj and d have to persist until end of this method,
		// so that the sysprops pointers that they contain
		// remain valid until we are done using them.
		Directory d(this,ReservedOid::_RootDir,ShoreVasLayer.RootObj);

		// This object has to have the same scope as h
		// because in some cases, h points into the object's
		// swapped hdr.
		Object	obj(this, target.lvid, target.serial);
		union _sysprops	*h;

		checkflags(false); // not yet xfer time

		if(target.serial == ReservedSerial::_RootDir) {
			lrid_t temp;
			DBG(<<"sysprops for root " << target);

			_DO_(d.get_sysprops(&h));
			_DO_(getRootDir(&temp) ); // map it

			s->volume 	= temp.lvid;
			s->ref  	= temp.serial.data; // GAK
			DBG(<<"sysprops for root " << d.lrid());

		} else {

			DBG(<<"sysprops for target " << target);

			_DO_(obj.get_sysprops(&h));

			// obj has to be pinned for the snap to work
			{ 	// 
				// snap this ref
				// this is the cheap way, since the object
				// is pinned
				//
				lrid_t temp = obj.snap();

				s->volume 	= temp.lvid;
				s->ref  	= temp.serial.data; // GAK
			}

			dassert(h->common.csize + h->common.hsize == obj.body_size());

			checkflags(false); 
			if(copypage) {
				smsize_t	cursize;
				bool		isAnonymous ;

				checkflags(false); // not yet xfer time

				_DO_(obj.legitAccess(obj_stat, lock, \
						&cursize, 0, WholeObject, &isAnonymous) );

				if(isAnonymous && obj.is_small() && !pseudo_client()) {
					// what's pinned covers the end of the requested region
					assert(obj.start_byte() == 0);

					if( obj.lockpage(::SH)!=SVAS_OK ) {
						VERR(SVAS_SmFailure);
						FAIL;
					}

					lvid_t		  *pretend;
					clrflags(vf_no_xfer);
					setflags(vf_sm_page);

					DBG(<<"copying whole page: ");
					vec_t wholepage(page_buf(), page_size());

					wholepage.copy_from(obj.page(), page_size()); 

					// write the logical volume 
					// id into the lsn2 of the page
					struct page_s *p = (struct page_s *)page_buf();
					pretend = (lvid_t *)&(p->lsn2);
					*pretend = target.lvid;
#ifdef DEBUG
					audit_pg(p);
#endif
					checkflags(true); 
				} else {
					checkflags(false); 
				}
			} else{
				checkflags(false); 
			}
		}

		s->type 	= h->common.type;
		s->csize 	= h->common.csize;
		s->hsize 	= h->common.hsize;

		AnonProps			*ap; 
		RegProps			*rp;

		s->tag = sysp_split(*h, &ap, &rp,  
			&s->tstart, &s->nindex, size_of_sysp);

		if(is_unix_file && (s->tstart != NoText)) {
			*is_unix_file = true;
		}
		switch(s->tag) {
			case KindRegistered:
				s->reg_nlink 	= rp->nlink;
				s->reg_mode		= rp->mode;
				s->reg_uid		= rp->uid;
				s->reg_gid		= rp->gid;
				s->reg_atime 	= rp->atime;
				s->reg_mtime 	= rp->mtime;
				s->reg_ctime 	= rp->ctime;
				break;

			case KindAnonymous:
				s->anon_pool 	= ap->pool;
				break;
			
			default: 
				break;
		}
		dassert((unsigned int)(s->type._low) & 0x1);
	}

FOK:
	if(pagecached) (*pagecached) = false; // TODO page caching on server
	RETURN SVAS_OK;

FFAILURE:
	RETURN SVAS_FAILURE;
}
VASResult		
svas_server::sysprops(
	IN(lrid_t)	target, // for any kind of object
	OUT(SysProps) sys,
	bool		copypage, // in default = false
	LockMode	lock,	// in default = SH
	OUT(bool)	is_unix_file,	// default = NULL (don't care)
	OUT(int)	size_of_sysp,	// default = NULL (don't care)
	OUT(bool)	pagecached	// default = NULL (don't care)
)
{
	VFPROLOGUE(svas_server::sysprops); 

	errlog->log(log_info, "STAT %d.%d.%d", 
		target.lvid.high, target.lvid.low, 
		target.serial.data._low);

	TX_REQUIRED;
FSTART
	_DO_(_sysprops(target, sys, copypage, lock,  \
		is_unix_file, size_of_sysp, pagecached));
FOK:
	if(pagecached) (*pagecached) = false; // TODO page caching on server
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

// this is for command du on an object DU DF
VASResult       
svas_server::disk_usage(
	IN(lrid_t) 		oid,		 	// shore object!!
	bool			try_root,		// could be rootdir
    OUT(struct sm_du_stats_t) du
)
{
	VFPROLOGUE(svas_server::disk_usage);

	TX_REQUIRED;

FSTART
	if(du==NULL) {
		VERR(OS_BadAddress);
		FAIL;
	}
	{
		Object obj(this, oid.lvid, oid.serial);
		bool	disallowed, isAnonymous, isDirectory;
		union _sysprops	*sys;

		if(obj.typeInfo(obj_read, 
			&disallowed, &isAnonymous,&isDirectory, &sys)!=SVAS_OK) {
			FAIL;
		}
		bool found;
		lrid_t root;
		serial_t fids;
		// doesn't matter if it's anonymous or registered; it's
		// the type that counts
		if(isDirectory) {
			if(try_root) {
				// see if this is the root of a mounted volume
				_DO_( _lookup2(oid, "..", Permissions::op_read,\
					Permissions::op_read, &found, &root,\
					&fids, true, false) );

				if(
					// current way to find root:
					(root == oid) 
				||
					// volume linked in hierarchy on different volume
					(root.lvid != oid.lvid) 
				) {
					if SMCALL( get_du_statistics(oid.lvid, fids, *du)) {
						VERR(SVAS_SmFailure);
						FAIL;
					}
				}
			}
			goto done;
		} else {
			if(sys->common.type == ReservedSerial::_Pool) {
				lrid_t file; 
				_DO_(fileOf(oid, &file));
				if SMCALL( get_du_statistics(file.lvid, file.serial, *du)) {
					VERR(SVAS_SmFailure);
					FAIL;
				}
			} else if( ! ReservedSerial::is_reserved_fs(sys->common.type) ){
				// INDEXES!
				int ni;
				(void) sysp_split(*sys, 0, 0, 0, &ni);

				if(ni > 0) {
					manual_index_i m(obj);
					if(m.err()) {
						FAIL;
					}
					while(m.next(&fids, manual_index_i::i_nonnull)) {
						if SMCALL( get_du_statistics(oid.lvid, fids, *du)) {
								VERR(SVAS_SmFailure);
								FAIL;
						}
					}/*while*/
				} /*has indexes*/
			}/*user-defined type*/
		}/*not a directory*/
	}
FOK:
done:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

