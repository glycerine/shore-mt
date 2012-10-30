/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/index.C,v 1.46 1997/01/24 16:47:58 nhall Exp $
 */
#include <copyright.h>

#include "Anonymous.h"


ss_m::ndx_t
IndexKind2ndx_t	(IndexKind indexKind) {	
	ss_m::ndx_t ik;
	switch(indexKind) {
		case BTree:
			ik = ss_m::t_btree;
			break;
		case RDTree:
			ik = ss_m::t_rdtree;
			break;
		case RTree:
			ik = ss_m::t_rtree;
			break;
		case UniqueBTree:
			ik = ss_m::t_uni_btree;
			break;
		case LHash:
			ik = ss_m::t_lhash;
			break;
		default:
			ik = ss_m::t_bad_ndx_t;
			break;
	}
	return ik;
}

enum boundtype {lower, upper};

// local function
// returns true if it passes muster 
bool
boundsCheck( 
	INOUT(CompareOp) 	c, 
	IN(vec_t)			bound,
	OUT(vec_t *)		res,
	boundtype 			which // lower or upper
) 
{
	FUNC(boundsCheck);
	bool	bad = false;

	DBG(<<"check " << (char *)((which==lower)?"lower":"upper")
		<<" bound "  << *c );

	*res = 0;
	switch(*c) {
		case gtPosInf:
		case gePosInf:
			bad=true;
			break;

		case ltPosInf:
		case lePosInf:
		case eqPosInf:
			if(which==lower) {
				bad = true;
			} else {
				DBG(<<" returning pos inf for bound");
				*res = &vec_t::pos_inf;
				(*c) = (CompareOp) (((int)(*c))-(int)PosInf);
			}
			break;

		case ltNegInf:
		case leNegInf:
			bad = true;
			break;

		case gtNegInf:
		case geNegInf:
		case eqNegInf:
			if(which==upper) {
				bad = true;
			} else {
				DBG(<<" returning neg inf for bound");
				*res = &vec_t::neg_inf;
				(*c) = (CompareOp) (((int)(*c))-(int)NegInf);
			}
			break;

		case gtOp:
		case geOp:
			if(which==upper) {
				bad = true;
			}
			break;
			
		case leOp:
		case ltOp:
			if(which==lower) {
				bad = true;
			}
			break;

		case eqOp:
			break;

		default:
			bad = true;
			break;
	} 
	DBG(<<"check returns " << bad << " c=" << *c 
		<<" vec size=" << bound.size()
		<< " is_neg_inf= " << bound.is_neg_inf()
		<< " is_pos_inf= " << bound.is_pos_inf()
	);
#ifdef DEBUG
	if(bound.size()>0 && !bound.is_neg_inf() && !bound.is_pos_inf()) {
		DBG(<< form("|%s|\n", bound.ptr(0)) );
	}
#endif

	return !bad;
}

//
// Version for manual indexes
//
VASResult		
svas_server::openIndexScan(
	IN(__IID__) 		idx, 	
	CompareOp			lc,		
	IN(vec_t)			lbound,
	CompareOp			uc,	
	IN(vec_t)			ubound,
	OUT(Cookie)			cookie	// OUT
)
{
	VFPROLOGUE(svas_server::openIndexScan);

	errlog->log(log_info, "{SCAN(I) %d.%d.%d", /*}*/
		idx.obj.lvid.high, idx.obj.lvid.low, idx.obj.serial.data._low);

	TX_REQUIRED;
FSTART
	if(!cookie) {
		VERR(SVAS_BadCookie);
		FAIL;
	}
	{
		lvid_t 		idxlvid;
		serial_t	idxserial;

		idxlvid = idx.obj.lvid;
		Object 	    object(this, idx.obj.lvid, idx.obj.serial);
		// errlog->log(log_info, "{SCAN(I) %d.%d", /*}*/
		// 	idx.obj.lvid.vid, idx.obj.serial.data._low);

		_DO_(object.indexAccess(obj_scan, idx.i, &idxserial));

		scan_index_i::cmp_t		b1;
		scan_index_i::cmp_t		b2;
		vec_t 					*lb, *ub;

		if(this->iscan != NULL) {
			VERR(SVAS_IndexScanIsOpen); // only one at a time for the moment
			FAIL;
		}

		// check the arguments here because the ssm
		// just chokes if they're not of the following
		// form: lbound has to exist and be >, >= or =
		// form: ubound has to exist and be <, <= or =
		if( ! boundsCheck(&lc, lbound, &lb, lower) ) {
			VERR(SVAS_BadParam2); 
		}
		if( !boundsCheck(&uc, ubound, &ub, upper) )  {
			VERR(SVAS_BadParam4);
		}

		b1 = (scan_index_i::cmp_t)lc;
		b2 = (scan_index_i::cmp_t)uc;
		assert(b1 >= scan_index_i::bad_cmp_t && b1 <= scan_index_i::le); 
		assert(b2 >= scan_index_i::bad_cmp_t && b2 <= scan_index_i::le); 

		DBG(<< "opening scan with b1=" 
			<< b1 << " b2= " << b2);

		scan_index_i *scandesc  = new
			scan_index_i(
				idxlvid, idxserial, 
				b1, lb?*lb:lbound, b2, ub?*ub:ubound);

		if(scandesc == NULL)  {
			VERR(SVAS_SmFailure);
			FAIL;
		}
		*cookie = (Cookie) scandesc;
	}
FOK:
	errlog->log(log_info, "COOK(P) 0x%x", *cookie);
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::nextIndexScan(
	INOUT(Cookie)		cookie,	// INOUT
	IN(vec_t)			key, // INOUT
	OUT(ObjectSize)		keylen,
	IN(vec_t)			value, // INOUT
	OUT(ObjectSize)		valuelen,
	INOUT(bool)		eof	// true if no result
						// if false, result is legit
)
{
	VFPROLOGUE(svas_server::nextIndexScan);
	errlog->log(log_info, "NEXT(I) 0x%x", cookie);

	TX_REQUIRED;
	bool  freethem=false;
	char	*kbuf=0;
	char	*vbuf=0;

FSTART
	if(!cookie) {
		VERR(SVAS_BadCookie);
		FAIL;
	}
	if(!keylen) {
		VERR(OS_BadAddress);
		FAIL;
	}
	if(!valuelen) {
		VERR(OS_BadAddress);
		FAIL;
	}

	bool 	_eof;

	{
		smsize_t	klen = key.size();
		smsize_t	vlen = value.size();
		kbuf = new char[klen];
		vbuf = new char[vlen];
		vec_t		kvec;
		kvec.put(kbuf, klen);
		vec_t		vvec;
		vvec.put(vbuf, vlen);
		freethem = true;

		scan_index_i *scandesc;
		if(!(scandesc = check_index_cookie(*cookie))) {
			VERR(SVAS_BadCookie);
			FAIL;
		}

		if CALL(scandesc->next(_eof) ) {
			VERR(SVAS_SmFailure);
			FAIL;
		}
		if(eof) *eof=_eof;
		if( !_eof ) {
			if CALL(scandesc->curr(&kvec, klen, &vvec, vlen) ) {
				VERR(SVAS_SmFailure);
				FAIL;
			}
			key.copy_from(kbuf, klen);
			value.copy_from(vbuf, vlen);
			*keylen = (ObjectSize) klen;
			*valuelen = (ObjectSize) vlen;
		}
		else // this may fix an intermittent rpc crash?
		{
			*keylen =0;
			*valuelen = 0;
		}
		delete [] kbuf;
		delete [] vbuf;
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	if(freethem) {
		delete [] kbuf;
		delete [] vbuf;
	}
		LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::closeIndexScan(
	IN(Cookie)			cookie	// IN
)
{
	VFPROLOGUE(svas_server::closeIndexScan);
	/*{*/
	errlog->log(log_info, "}SCAN(I) 0x%x", cookie);

	TX_REQUIRED;

	scan_index_i *scandesc;
	if(!(scandesc = check_index_cookie(cookie))) {
		VERR(SVAS_BadCookie);
		FAIL;
	}
	delete  scandesc;

	this->iscan = NULL;
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}


VASResult		
svas_server::insertIndexElem(
	IN(__IID__)			idx, // obj owning index + int
	IN(vec_t)			key,		// IN
	IN(vec_t)			value	  	// IN
)
{
	VFPROLOGUE(svas_server::insertIndexElem);

	errlog->log(log_info, "INSERT(I) %d.%d.%d,%d", 
		idx.obj.lvid.high,
		idx.obj.lvid.low,
		idx.obj.serial.data._low,
		idx.i);

	TX_REQUIRED; 

FSTART
	lvid_t 		idxlvid;
	serial_t	idxserial;
	{
		idxlvid = idx.obj.lvid;
		Object 	    object(this, idx.obj.lvid, idx.obj.serial);
		// errlog->log(log_info, "{SCAN(I) %d.%d", /*}*/
		// 	idx.obj.lvid.vid, idx.obj.serial.data._low);

		_DO_(object.indexAccess(obj_insert, idx.i, &idxserial));

		if SMCALL(create_assoc(idxlvid, idxserial, key, value) ) {
			VERR(SVAS_SmFailure);
			FAIL;
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
svas_server::removeIndexElem(
	IN(__IID__)			idx, // obj representing idx
	IN(vec_t)			key,
	OUT(int)			numremoved	  // # elements removed
)
{
	VFPROLOGUE(svas_server::removeIndexElem);
	errlog->log(log_info, "REMOVE(I) %d.%d.%d,%d", 
		idx.obj.lvid.high,
		idx.obj.lvid.low,
		idx.obj.serial.data._low,
		idx.i);

	TX_REQUIRED; 
	bool		freethem = false;
	char		*kbuf=0;
	char		*vbuf=0;
FSTART
	if(!numremoved) {
		VERR(OS_BadAddress);
		FAIL;
	}
#define DESTROY_ALL
#ifdef DESTROY_ALL
	{
		lvid_t 		idxlvid;
		serial_t	idxserial;

		idxlvid = idx.obj.lvid;
		Object 	    object(this, idx.obj.lvid, idx.obj.serial);
		_DO_(object.indexAccess(obj_remove, idx.i, &idxserial));

		*numremoved = 0;
		if SMCALL(destroy_all_assoc(idxlvid, idxserial, key, *numremoved)) {
			VERR(SVAS_SmFailure);
			FAIL;
		}
	}
#else
	{
		smsize_t		klen = key.size();
		smsize_t		vlen = ss_m::page_sz;
		kbuf = new char[klen];
		vbuf = new char[vlen];
		freethem = true;

		{
			lvid_t 		idxlvid;
			serial_t	idxserial;
			bool		_eof;

			idxlvid = idx.obj.lvid;
			Object 	    object(this, idx.obj.lvid, idx.obj.serial);
			// errlog->log(log_info, "{SCAN(I) %d.%d", /*}*/
			// 	idx.obj.lvid.vid, idx.obj.serial.data._low);

			_DO_(object.indexAccess(obj_remove, idx.i, &idxserial));

			scan_index_i::cmp_t		b1 = scan_index_i::eq;
			vec_t				kvec;
			kvec.put(kbuf, klen);
			vec_t				vvec;
			vvec.put(vbuf, vlen);

			scan_index_i	scandesc(idxlvid, idxserial, b1, key, b1, key);

			*numremoved = 0;
			if CALL(scandesc.next(_eof) ) {
				VERR(SVAS_SmFailure);
				FAIL;
			}
			while(!_eof) {

				vvec.set(vbuf,ss_m::page_sz); // old vlen
				if CALL(scandesc.curr(&kvec, klen, &vvec, vlen) ) {
					VERR(SVAS_SmFailure);
					FAIL;
				}
				// make vvec reflect the new vlen
				vvec.set(vbuf,vlen); // old vlen

				if SMCALL(destroy_assoc(idxlvid, idxserial, kvec, vvec) ) {
					VERR(SVAS_SmFailure);
					FAIL;
				}
				(*numremoved)++;
				if CALL(scandesc.next(_eof)) {
					VERR(SVAS_SmFailure);
					FAIL;
				}
			}
		} 
		delete [] kbuf;
		delete [] vbuf;
	}
#endif
FOK:
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	if(freethem) {
		delete [] kbuf;
		delete [] vbuf;
	}
	LEAVE;
	RETURN SVAS_FAILURE;
}


VASResult		
svas_server::removeIndexElem(
	IN(__IID__)			idx, // obj representing idx
	IN(vec_t)			key,
	IN(vec_t)			value	  // INOUT
)
{
	VFPROLOGUE(svas_server::removeIndexElem);

	errlog->log(log_info, "REMOVE(I) %d.%d.%d,%d", 
		idx.obj.lvid.high,
		idx.obj.lvid.low,
		idx.obj.serial.data._low,
		idx.i);

	TX_REQUIRED; 

FSTART
	lvid_t		idxlvid;
	serial_t	idxserial;
	{
		idxlvid = idx.obj.lvid;
		Object 	    object(this, idx.obj.lvid, idx.obj.serial);
		_DO_(object.indexAccess(obj_remove, idx.i, &idxserial));
	}
	if SMCALL(destroy_assoc(idxlvid, idxserial, key, value) ) {
		VERR(SVAS_SmFailure);
		FAIL;
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}


VASResult		
svas_server::findIndexElem(
	IN(__IID__)			idx, // obj representing idx
	IN(vec_t)			key,
	IN(vec_t)			value,	  // INOUT
	OUT(ObjectSize)		value_len, // OUT
	OUT(bool)			found
)
{
	VFPROLOGUE(svas_server::findIndexElem);
	errlog->log(log_info, "FIND(I) %d.%d.%d,%d", 
		idx.obj.lvid.high,
		idx.obj.lvid.low,
		idx.obj.serial.data._low,
		idx.i);

	bool		freethem = false;
	char		*vbuf=0;

	lfid_t		index_file;
	smsize_t	vlen = ss_m::page_sz;

	TX_REQUIRED;
FSTART
	if(!value_len) {
		VERR(OS_BadAddress);
		FAIL;
	}
	if(!found) {
		VERR(OS_BadAddress);
		FAIL;
	}
	smsize_t 	vlen = ss_m::page_sz;
	lvid_t		idxlvid;
	serial_t	idxserial;
	{
		idxlvid = idx.obj.lvid;
		Object 	    object(this, idx.obj.lvid, idx.obj.serial);
		_DO_(object.indexAccess(obj_search, idx.i, &idxserial));
	}
	{
		vbuf = new char[vlen];
		if(!vbuf) {
			VERR(SVAS_MallocFailure);
			FAIL;
		}
		freethem =  true;

		DBG(<<"find assoc " << idxlvid << "." << idxserial
			<< " key=" << key.ptr(0) );

		if SMCALL(find_assoc(idxlvid, idxserial,
			key, vbuf, vlen, *found) ) {
			VERR(SVAS_SmFailure);
			FAIL;
		}
		// return value is hosed if not found, so leave vector zeroed.
		if (*found)
		{
			value.copy_from(vbuf, vlen);
			*value_len = (ObjectSize)vlen;
		}
	} 
FOK:
	if(freethem) delete [] vbuf;
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	if(freethem) delete [] vbuf;
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::addIndex(
	IN(IndexId)				idx,
	IndexKind			indexKind	//  btree, lhash, etc
)
{
	LOGVFPROLOGUE(svas_server::addIndex);

	errlog->log(log_info, "ADD INDEX %d.%d.%d,%d",
		idx.obj.lvid.high, idx.obj.lvid.low, 
		idx.obj.serial.data._low, idx.i);

	TX_REQUIRED; 
FSTART
	{
		serial_t	idxserial;

		Object 	    object(this, idx.obj.lvid, idx.obj.serial);
		_DO_(object.indexAccess(obj_add, idx.i));

		//create an index
		// create an index on this volume, complete its iid
		if SMCALL(create_index(idx.obj.lvid, 
			IndexKind2ndx_t(indexKind), 
			NotTempFile, 
			"b*1000", // TODO: pass along this argument to the user
			10, // large
			idxserial) ) {
			VERR(SVAS_SmFailure);
			FAIL;
		}

		DBG(<<"created index " << idx.obj.lvid << "." << idxserial
			<< ", which is the " << idx.i << "th index in "
			<< idx.obj );

		// stuff its id in idx.obj's list, in the idx.i'th place
		_DO_(object.addIndex(idx.i, idxserial));
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::dropIndex(
	IN(IndexId)				idx
)
{
	VFPROLOGUE(svas_server::dropIndex);
	errlog->log(log_info, "DROP INDEX %d.%d.%d,%d",
		idx.obj.lvid.high, idx.obj.lvid.low, 
		idx.obj.serial.data._low, idx.i);

	TX_REQUIRED; 
FSTART
	{
		serial_t  idxserial;

		Object 	    object(this, idx.obj.lvid, idx.obj.serial);
		_DO_(object.indexAccess(obj_destroy, idx.i, &idxserial));

		//blow away the index
		if SMCALL(destroy_index(idx.obj.lvid, idxserial) ) {
			VERR(SVAS_SmFailure);
			RETURN SVAS_FAILURE;
		}
		// stuff null iid in the idx.i'th place
		// of idx.obj's list
		_DO_(object.addIndex(idx.i, serial_t::null));
		// 
		// TODO: if anonymous object, destroy the metadata
		// in the pool.
	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::_indexCount(
	IN(lrid_t)			idx,
	OUT(int)			entries
) 
{
	VFPROLOGUE(svas_server::_indexCount);

	dassert(entries!=0);
FSTART
	int		count=0;
	bool	_eof;
	scan_index_i	scandesc(idx.lvid, idx.serial, 
		scan_index_i::ge, vec_t::neg_inf, 
		scan_index_i::le, vec_t::pos_inf);

	if CALL(scandesc.next(_eof) ) {
		VERR(SVAS_SmFailure);
		FAIL;
	}
	while(!_eof) {
		count++;
		if CALL(scandesc.next(_eof)) {
			VERR(SVAS_SmFailure);
			FAIL;
		}
	}
	*entries = count;
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

// TODO - implement du or remove statIndex
VASResult		
svas_server::statIndex(
	IN(IndexId)			idx,
	OUT(indexstatinfo)		j
) 
{
	VFPROLOGUE(svas_server::statIndex);
	errlog->log(log_info, "STAT INDEX %d.%d.%d,%d",
		idx.obj.lvid.high, idx.obj.lvid.low, 
		idx.obj.serial.data._low, idx.i);

	TX_REQUIRED; 
FSTART
	if(j==NULL) {
		VERR(OS_BadAddress);
		FAIL;
	}
	memset(j, '\0', sizeof(j)); // clear stats struct

	{
		serial_t	idxserial;
		sm_du_stats_t		du;
		Object 	    object(this, idx.obj.lvid, idx.obj.serial);

		_DO_(object.indexAccess(obj_scan, idx.i, &idxserial));

		if SMCALL( get_du_statistics(idx.obj.lvid, idxserial, du)) {
			VERR(SVAS_SmFailure);
			FAIL;
		}

		{
			int		count=0;
			lrid_t	iid(idx.obj.lvid, idxserial);
			_DO_(_indexCount(iid,  &count));
			j->nentries = count;
		}
		j->fid.lvid = idx.obj.lvid;
		j->fid.serial = idxserial;

		// TODO: get index-specific stats
		j->npages = 0; 
		j->nbytes = 0; 

	}
FOK:
	LEAVE;
	RETURN SVAS_OK;
FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}


VASResult		
svas_server::_fileOf(
	IN(__IID__)			idx,
	OUT(lrid_t)			result // really an lfid but oh well
) 
{
	VFPROLOGUE(svas_server::_fileOf);

	CLI_TX_ACTIVE;
FSTART
	dassert(result!=0);
	result->lvid = idx.obj.lvid;

	{
		serial_t	serial;
		Object	obj(this, idx.obj.lvid, idx.obj.serial);

		_DO_(obj.indexAccess(obj_read, idx.i, &serial));

		result->serial= serial;
	}
FOK:
	RETURN SVAS_OK;
FFAILURE:
	RETURN SVAS_FAILURE;
}

VASResult		
svas_server::fileOf(
	IN(__IID__)			idx,
	OUT(lrid_t)			result // really an lfid but oh well
) 
{
	VFPROLOGUE(svas_server::fileOf);

	errlog->log(log_info, "FILEOF(I) %d.%d.%d,%d", 
		idx.obj.lvid.high,
		idx.obj.lvid.low,
		idx.obj.serial.data._low,
		idx.i);

	TX_REQUIRED;
FSTART
	if(!result) {
		VERR(OS_BadAddress);
		FAIL;
	}
	_DO_(_fileOf(idx,result));
FOK:
	LEAVE;
	RETURN SVAS_OK;

FFAILURE:
	LEAVE;
	RETURN SVAS_FAILURE;
}
