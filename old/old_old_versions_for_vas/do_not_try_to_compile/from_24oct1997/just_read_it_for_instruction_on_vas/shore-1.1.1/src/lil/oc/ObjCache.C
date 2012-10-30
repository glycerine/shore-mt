/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// ObjCache.C
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/ObjCache.C,v 1.168 1997/06/13 21:51:08 solomon Exp $ */

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif


#ifdef __GNUG__
#pragma implementation "ObjCache.h"
#pragma implementation "OCInternal.h"
#endif

#include <string.h>
#include <debug.h>
#include <fc_error.h>
#include <w_statistics.h>

#include "OCInternal.h"
#include "ObjCache.h"
//#ifdef SERVER_ONLY
// #include "sm_vas.h"
//#else
#include "shore_vas.h"
//#endif
#include "reserved_oids.h"
#include "svas_error_def.h"
#include "os_error_def.h"
#include "SH_error.h"
//#include "sdl_string.h"
#include "sdl_UnixFile.h" 

//  yikes.
OCstats stats;
// bletch
// oc calls now are not static; they vector through a real class
// instance.  For now, we punt on the shore calls and define
// a global variable; this is used to access the (currently valid) oc.
// we will need to fix this for more general contexts (e.g. multicache
// server.
ObjCache *cur_oc = 0;

// trace hack
FILE *tf = 0;
FILE *cf = 0;
// For want of a better place to put this...  NB: default constructor
// constructs a null loid.
//const LOID LOID::null;

#ifdef BITS64
const LOID LOID::null(0, 0, 0, 1);
#else
const LOID LOID::null(0, 0, 1);
#endif

////////////////////////////////////////////////////////////////
//
//		     Initialization and Cleanup
//
////////////////////////////////////////////////////////////////

// these are former static vars, now in the oc class.
// note: all except next_xact_num are correctly initializes as 0,
// so for them moment just init that in ::init.
#ifdef oldstatics
bool			 ObjCache::initialized = false;

int			 ObjCache::curr_xact_num = 0;
int			 ObjCache::next_xact_num = 1;

VolumeTable		 ObjCache::vt;
MemMgr			 ObjCache::mmgr;
LOIDTable		 ObjCache::lt;
svas_base		*ObjCache::vas;
OTChunk			*ObjCache::first;
OTChunk			*ObjCache::current;
int			 ObjCache::next_free;
OTEntry			*ObjCache::pool_list;
int			 ObjCache::serial_array_size;
int			 ObjCache::auditlevel = 0;
SerialSet		*ObjCache::serials;
XactAllocListNode	*ObjCache::xact_alloc_list = 0;
unsigned int	ObjCache::_last_error_num=0;
bool			ObjCache::aborting=false;
bool			ObjCache::do_refcount = true;
#endif
// this one is still static, regardless.
bool			 ObjCache::e_initialized = false;
void
ObjCache::init_svars()
{
	// since we don't have statics anymore, this routine initializes
	// the former static variables as they would be if the #ifdefd
	// decls above were still valid.
	curr_xact_num = 0;
	next_xact_num = 1;
	xact_alloc_list = 0;
	auditlevel = 0;
	_last_error_num=0;
	aborting=false;
	initialized = false;
	
	curr_xact_num = 0;
	next_xact_num = 1;
	
	vas = 0;
	first = 0;
	current = 0;
	next_free = 0;
	pool_list = 0;
	serial_array_size = 0;
	auditlevel = 0;
	serials = 0;
	xact_alloc_list = 0;
	_last_error_num=0;
	aborting=false;
	batch_active = false;
}
	

// error strings for error codes are found here
#include "SH_einfo.i"

w_rc_t
ObjCache::init_error_codes()
{
	// initialize error stuff
	if(!e_initialized){
		if(!w_error_t::insert("Shore Object Cache",
							SH_error_info,
							SHERRMAX - SHERRMIN + 1)){
			return RC(SH_Internal);
		}
	}
	e_initialized = true;
	return RCOK;
}

int do_old_prefetch = 0;
int do_logops;
int do_logcr;

const int MAX_LOGOPS = 100000;
struct log_ent { OTEntry *ote; char *op; };
log_ent *log_recs;
int num_logents = 0;
void
log_opr(OTEntry *ote, char * desc)
{
	//if (num_logents >= MAX_LOGOPS) return;
	//log_recs[num_logents].ote = ote;
	//log_recs[num_logents].op = desc;
	//++num_logents;
	int serial = 0;
	char *addr =0;
	if (!tf) tf = fopen("op_log","w"  );
	if (ote) {serial = ote->volref.guts._low; addr = ote->obj;}

	fprintf(tf,"ote 0x%x s# %d addr 0x%x op %s\n",ote,addr,serial,desc);
}

void
print_opr_log()
{
	int i;
	for (i=0;i<num_logents; i++)
		printf("%x ote, op %s\n",log_recs[i].ote,log_recs[i].op);
}


// export mmgr ptr
//MemMgr * mptr;
w_rc_t
ObjCache::init()
{
	svas_base *tmp;
	if (this==0) // hm.
	{
		cur_oc = new ObjCache;
		return return_error(cur_oc->init_error_codes());
	}
	if(initialized)
	{
		if (inside_server)
			return return_error(RCOK);
		else
			return return_error(RC(SH_AlreadyInitialized));
	}
	// memset(this,0,sizeof(*this));
	// yuck.
	cur_oc = this;
	init_svars();

	dassert(e_initialized);
	

	vt.init();
	mmgr.init(this);
	// mptr = &mmgr;
	lt.init(this);
	vas = 0;
	first = 0;
	current = 0;
	next_free = 0;
	pool_list = 0;
	serial_array_size = 0;
	serials = 0;

	clear_stats();

	// initialize a new connection with the vas
	// use default values (options)
	// special case of error handling - we'd
	// ok, for client/server compatibility, add a new routine, get_my_svas.
	// this replaces new_svas; it calls new_svas in the client
	// version and just gets the vas from the current thread
	// in the server version.  Each version sets inside_server
	// appropriately.

	w_rc_t rc = get_my_svas(&tmp,inside_server);
	if(rc) {
		return return_error(RC_AUGMENT(rc));
	}
	vas = tmp;
	if (inside_server) 
	// set the vas's idea of the Oc correctly.

	// now the cache is initialized;
	initialized = true;
	sdl_init();
	if (getenv("DO_PAGELIST"))
		do_pagecluster = 1;
	if (getenv("OLD_PREFETCH"))
		do_old_prefetch = 1;
	if (getenv("LOG_IO"))
	{
		do_logops = 1;
		tf = fopen("op_log","w"  );
	}
	if (getenv("LOG_CR"))
	// log create anonymous only.
	{
		do_logcr = 1;
		cf = fopen("mkanon_log","w");
	}
	if (inside_server)
		do_batch = false;

	return return_error(RCOK);
}

w_rc_t
ObjCache::exit()
{
	OTChunk *otc, *next;

	// reset the object cache (invalidate structures, flush cache)
	reset(true, true, true);

	for(otc = first; otc != 0; otc = next){
		next = otc->next;
		delete otc;
	}
	first = 0;

	if(serials)
		delete serials;
	serials = 0;

	// terminate the connction with the vas
	if(!inside_server && vas)
		delete vas;

	// set this flag so we can call init() later
	initialized = false;
	if (stats.total_pins != stats.total_unpins)
		cerr << " pin mismatch at exit : pins " << stats.total_pins
				<< " unpins " << stats.total_unpins << endl;

	delete this; // dubious but neccessary for server-resident app.
	return return_error(RCOK);
}

////////////////////////////////////////////////////////////////
//
//		       Transaction Methods
//
////////////////////////////////////////////////////////////////

w_rc_t
ObjCache::begin_transaction(int degree)
{
	// make sure the object cache has been initialized
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }

	// make sure there isn't already an active transaction
	if(vas->status.txstate != NoTx && vas->status.txstate != Stale)
		return return_error(RC(SH_TxNotAllowed));

	aborting = false;

	// start a transaction
	VAS_DO(vas->beginTrans(degree));

	// set the current transaction number
	curr_xact_num = next_xact_num++;

	return return_error(RCOK);
}

w_rc_t
ObjCache::chain_transaction()
{
    // make sure the object cache has been initialized
    if(vas==0) { return return_error(RC(SH_NotInitialized)); }

    // make sure there is an active transaction
    if(vas->status.txstate == NoTx)
	    return return_error(RC(SH_TxRequired));
    if(vas->status.txstate == Aborting)
	    return return_error(RC(SH_TxAborted));

    // write everything back to the vas
    W_DO(writeback_all());

    // update the timestamps for the pools on the pool list
    W_DO(update_pool_timestamps(0));

    W_DO(automatic_stats());
    VAS_DO(vas->commitTrans(true));
	
    return return_error(RCOK);
}
w_rc_t
ObjCache::commit_transaction(bool invalidate)
{
    // make sure the object cache has been initialized
    if(vas==0) { return return_error(RC(SH_NotInitialized)); }

    // make sure there is an active transaction
    if(vas->status.txstate == NoTx)
	    return return_error(RC(SH_TxRequired));
    if(vas->status.txstate == Aborting)
	    return return_error(RC(SH_TxAborted));

    // write everything back to the vas
    W_DO(flush(true));

    // update the timestamps for the pools on the pool list
    W_DO(update_pool_timestamps(0));

    // commit the transaction;  NB: If the transaction is
    // aborted, then the application will have to call
    // abort_transaction, so we don't do any cleanups here.

    W_DO(automatic_stats());
    VAS_DO(vas->commitTrans());
	
    // reset the cache
	// we already flushed so no need to do so again
    reset(invalidate, false, false);

    return return_error(RCOK);
}

w_rc_t
ObjCache::abort_transaction(bool invalidate)
{
    VASResult vr;

    // make sure the object cache has been initialized
    if(vas==0) { return return_error(RC(SH_NotInitialized)); }

    // make sure there is a transaction
    if(vas->status.txstate == NoTx)
	    return return_error(RC(SH_TxRequired));

    aborting = true;

    W_DO(automatic_stats());

    // abort the transaction; NB: even if this fails, we clean up
    // before returning
    vr = vas->abortTrans();

    // clear the pool list
    clear_pool_list();


    // reset the cache whether we got errors from the vas or not
    // the 2nd argument is false because we cannot write-back
    // to the vas w/o a transaction active!
    reset(invalidate, true, true);

    // VAS_DO(vr);		// kinda cheap, but correct

    return return_error(RCOK);
}

w_rc_t
ObjCache::automatic_stats(bool clear) // = true
{
    if(do_pstats) {
	// prints statistics, then clear them 

	static w_statistics_t rstats;
	static w_statistics_t __stats;
	static bool		gathered=false;

	w_rc_t e;

	if(!gathered) {
		e = gather_stats(__stats,false); 
		__stats << stats;
		gathered = true;
	}
	if(e) {
		cout << "Could not gather stats: " << e << endl;
	} else {
		cout << __stats;
	}
	if(do_premstats) {
	    e = gather_stats(rstats,true); 
	    if(e) {
		cout << "Could not gather remote stats: " << e << endl;
	    } else {
		cout << rstats;
	    }
	}
	if(clear) {
	    vas->cstats();
	    clear_stats();
	}
    }
    return RCOK;
}

TxStatus ObjCache::get_txstatus()
{
	if(vas == 0)
		return NoTx;

	else
		return vas->status.txstate;
}

caddr_t ObjCache::xact_alloc(int nbytes)
{
	caddr_t p;
	int ndoubles;
	XactAllocListNode *n;

	ndoubles = ((nbytes - 1) / sizeof(double)) + 1;

	p = (char *)(new double[ndoubles]);

	for(n = xact_alloc_list; n != 0 && n->in_use; n = n->next);
	if(n == 0){
		n = new XactAllocListNode;
		n->next = xact_alloc_list;
		xact_alloc_list = n;
	}

	n->p = p;
	n->in_use = true;

	return p;
}

w_rc_t ObjCache::xact_free(caddr_t p)
{
	XactAllocListNode *n;

	for(n = xact_alloc_list; n != 0 && n->p != p; n = n->next);
	if(n == 0)
		return return_error(RC(SH_Internal));

	delete [] n->p;
	n->in_use = false;

	return return_error(RCOK);
}

////////////////////////////////////////////////////////////////
//
//		      Statistics gathering
//
////////////////////////////////////////////////////////////////

void
OCstats::compute(ObjCache * CUR_OC)
// compute stat values from (mostly) things in ObjCache::mmgr
{
	stats.mem_used = CUR_OC->mmgr.alloc_bytes;
	stats.obj_used = CUR_OC->mmgr.alloc_blocks;
	stats.extern_mem = CUR_OC->mmgr.ex_bytes;
	stats.extern_obj = CUR_OC->mmgr.ex_blocks;
	
}
shrc 
ObjCache::gather_stats(w_statistics_t &s, bool remote) 
{
    if(remote) {
	if(vas==0) { return return_error(
		RC(SH_NotInitialized)); }
	// remote server stats only:
	vas->gatherRemoteStats(s);

    } else {
	// object cache stats:
	stats.compute(this); // fills in current stats.
	s << stats;

	// add the SVAS stats
	s << *vas;
    }
    return return_error(RCOK);
}

void
ObjCache::clear_stats()
{
    memset(&stats, 0, sizeof(stats));
}

////////////////////////////////////////////////////////////////
//
//			Filesystem methods
//
////////////////////////////////////////////////////////////////

w_rc_t
ObjCache::chdir(const char *path)
{
	// make sure the object cache has been initialized
	if(vas == 0) return return_error(RC(SH_NotInitialized));

	// ask the vas to change the current directory
	VAS_DO(vas->chDir(path));

	return return_error(RCOK);
}

w_rc_t
ObjCache::lookup(const char *path,
				OTEntry *&ote, 	// OUT ote
				rType  *ref_type)	// IN type expected (default null)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	LOID loid;
	int vindex;
	bool found;
	ote = 0; // return null if not found.

	if (ref_type) // use alternate form to get type right away.
	{
		LOID type_loid;
		W_DO(lookup(path,type_loid,ote));
		rType * real_type = get_type_object(type_loid);
		ote->type = real_type; // fill it in if ever needed?
		if (real_type->cast(0,ref_type)) // type is compatable
			return RCOK;
		else // type incompatible
			return RC(SH_BadType);
	}

	// ask the vas to resolve the pathname
	VAS_DO(vas->lookup(path, &loid.id, &found));
	if(!found) {
		return return_error( RC(SH_NotFound) );
	}

	// get the object's local volid
	vindex = vt.lookup(loid.volid(), true);
	dassert(vindex != VolumeTable::NoEntry);
	
	// see if we already have an OT entry for this object; create one
	// if not
	W_DO(lt.lookup_add(vindex, loid.volref(), ote));

	return return_error(RCOK);
}

w_rc_t
ObjCache::lookup(const char *path,
				LOID &type_loid,				// OUT type_loid
				OTEntry *&ote)						// OUT ote
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }

	int vindex;
	VolRef volref, type_volref;
	SysProps props;

	// get the objects system properties
	VAS_DO(vas->sysprops(path, &props));

	// get the object's loid
	vindex = vt.lookup(props.volume, true);
	dassert(vindex != VolumeTable::NoEntry);

	volref.set(props.ref);

	// get the object's type loid
	{
		VolRef type_volref;

		type_volref.set(props.type);

		// If it's a reserved serial, then the volid should be null.
		// Otherwise, use the object's volid.
		if(ReservedSerial::is_reserved(type_volref))
			type_loid.set(VolId::null, type_volref);
		else
			type_loid.set(props.volume, type_volref);
	}

	// see if we already have an OT entry for this object; create one
	// if not
	W_DO(lt.lookup_add(vindex, volref, ote));

	return return_error(RCOK);
}

////////////////////////////////////////////////////////////////
//
//			Index methods
//
////////////////////////////////////////////////////////////////

w_rc_t
ObjCache::addIndex(const IndexId &iid, IndexKind k)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }

	// ask the vas to add the index
	VAS_DO(vas->addIndex(iid,k));

	return return_error(RCOK);
}

w_rc_t
ObjCache::insertIndexElem(const IndexId &iid, const vec_t & k, const vec_t &v)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }

	// ask the vas to insert an element
	VAS_DO(vas->insertIndexElem(iid,k,v));

	return return_error(RCOK);
}

w_rc_t
ObjCache::removeIndexElem(const IndexId &iid, const vec_t & k, int *nrm)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }

	// ask the vas to remove an element
	VAS_DO(vas->removeIndexElem(iid,k,nrm));

	return return_error(RCOK);
}

w_rc_t
ObjCache::removeIndexElem(const IndexId &iid, const vec_t & k, const vec_t &v)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }

	// ask the vas to remove an element
	VAS_DO(vas->removeIndexElem(iid,k,v));

	return return_error(RCOK);
}

w_rc_t
ObjCache::findIndexElem(const IndexId &iid, const vec_t &k,
						const vec_t &value,
						ObjectSize *vlen, bool *found)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }

	// ask the vas to find an element
	VAS_DO(vas->findIndexElem(iid,k,value, vlen, found));

	return return_error(RCOK);
}

shrc 
ObjCache::openIndexScan(const IndexId &iid,
				CompareOp o1, const vec_t & k1,
				CompareOp o2, const vec_t &k2,
				Cookie & ck)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }

	VAS_DO(vas->openIndexScan(iid,o1,k1,o2,k2,&ck));
	return return_error(RCOK);
}

shrc 
ObjCache::nextIndexScan(Cookie & ck,
						const vec_t &k, ObjectSize & kl,
						const vec_t &v, ObjectSize& vl,
						bool &eof)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	VAS_DO(vas->nextIndexScan(&ck,k,&kl,v,&vl,&eof));
	return return_error(RCOK);
}

shrc 
ObjCache::closeIndexScan(Cookie &ck) 
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	VAS_DO(vas->closeIndexScan(ck));
	return return_error(RCOK);
}

////////////////////////////////////////////////////////////////
//
//	     Object Creation and Destruction Methods
//
////////////////////////////////////////////////////////////////

w_rc_t
ObjCache::alloc_ote(OTEntry *&ote)		// OUT ote
{
	// if there are no chunks in this object table yet...
	if(current == 0){

		// create a new chunk
		first = new OTChunk;

		if(first == 0)
			return return_error(RC(SH_OutOfMemory));

		first->next = 0;
		current = first;
		next_free = 0;
	}

	// if the current chunk of the object table is full...
	if(next_free >= OT_CHUNK_SIZE){

		// ...and we don't already have another...
		if(current->next == 0){

			// allocate a new one
			current->next = new OTChunk;
			current->next->next = 0;
		}

		current = current->next;
		next_free = 0;
	}

	// return the next unused ot entry in the current chunk
	ote = current->entries + next_free;
	++next_free;

	// initialize the circular linked-list of aliases
	ote->pool = 0;
	ote->lockmode = NO_LOCK_MODE;
	ote->flags = (ObjFlags)0;
	ote->obj = 0;
	ote->alias = ote;
	ote->lt_next = 0;
	ote->volref = VolRef::null;

	// update statistics
	++stats.otentries;

	return return_error(RCOK);
}

w_rc_t
ObjCache::new_ote(const rType *type,
				int vindex,
				const VolRef &volref,
				ObjFlags flags,
				OTEntry *pool,
				LockMode lm,
				OTEntry *&ote)				// OUT ote
{
	// allocate and initialize a new OT entry
	W_DO(alloc_ote(ote));
	ote->type = (rType *)type;
	dassert(vindex != VolumeTable::NoEntry);
	ote->volume = vindex;
	ote->flags = flags;
	ote->pool = pool;
	ote->lockmode = lm;
	ote->volref = volref;

	// set OF_Secondary if this is a remote serial
	if(!volref.is_null() && 
		(volref.is_remote() || ReservedSerial::is_reserved(volref)))
		ote->flags |= OF_Secondary;

	return return_error(RCOK);
}



w_rc_t
ObjCache::alloc_mem(OTEntry *ote)
{
	caddr_t p;
	BehindRec *brec;

	// allocate space for the new object in the object cache
	// special case: need to prepend space for string header.

	int mlen = ote->type->size + sizeof(BehindRec);
	// special case: need to prepend space for string header.
	if (ote->type->loid.id.serial== ReservedSerial::_UnixFile)
		mlen += sizeof(SdlUnixFile);

	// finally we know how much memory we need; at this point,
	// check if cache replacement is necessary

	if (ote->flags & OF_Transient) // don't use memory allocator, for now..
	{
		p = new char[mlen];
	}
	else
	{
		W_DO(mmgr.alloc(mlen, p));
	}
	// mmgr.audit();
	brec = (BehindRec *)p;
	brec->pin_count=0;
	brec->ref_bit =0;
	ote->obj = p + sizeof(BehindRec);

	// zero-out the new object
	memset(ote->obj, 0, ote->type->size);

	// intialize the behind rec
	brec->otentry = ote;

	// usage statistics
	// stats.mem_used += brec->osize;
	// stats.obj_used ++;
	// get this out of mmgr in OCStats::compute.

	return return_error(RCOK);
}

w_rc_t
ObjCache::create_anonymous(OTEntry *pool,
							rType *type,
							OTEntry *&ote)		// OUT ote
{
	// make sure the object cache has been initialized
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }

	// validate parameters
	if(pool == 0)
		return return_error(RC(SH_BadPool));
	if(type == 0)
		return return_error(RC(SH_BadType));

	// make sure this is the pool's primary ote
	if(pool->is_secondary()){
		OTEntry *tmp;
		W_DO(snap(pool, tmp));
		pool = tmp;
	}

	// if we haven't created objects in this pool before, then we need
	// to ask the vas if we have permission to do so, and to place a
	// lock on the pool to make sure it stays around until we're done
	// with it
	if(pool->lockmode < POOL_LOCK_MODE){
		W_DO(lock_obj(pool, POOL_LOCK_MODE));
	}

	// allocate and initialize a new ote for the object
	W_DO(new_ote(type,
				 pool->volume,
				 VolRef::null,
				 (ObjFlags)(OF_New | OF_Dirty),
				 pool,
				 NEW_OBJ_LOCK_MODE,
				 ote));

	// allocate memory for the object
	W_DO(alloc_mem(ote));

	// if the object contains indices or collections, write it back
	// right away
	if(type->nindices > 0){
		vt.inc_ncreates(pool->volume);
		if(!ote->has_serial())
			W_DO(get_volref(ote));
		// set it to real null
		//		ote->volref = serial_t::null;
		W_DO(writeback(ote));
	}

	// otherwise, increment the number of "uncreated" objects on this
	// volume
	else{
		vt.inc_ncreates(pool->volume);
	}

	// if the object's pool wasn't on the list of pools in which
	// objects have been created, then add it
	if(!pool->on_pool_list()){
		pool->pool = pool_list;
		pool_list = pool;
		pool->flags |= OF_OnPoolList;
	}

	// increment type refcount
	type->inc_refcount(1);

	// initialize the object's vptr; used to be done by Ref<T> member.
	type->setup_vtable(ote->obj);

	// update statistics
	++stats.anoncreates;

	return return_error(RCOK);
}

// create a trancient object. We probably can't be totally orthogonal
// about this.
w_rc_t
ObjCache::create_transient( rType *type,
							OTEntry *&ote)		// OUT ote
{
	// validate parameters
	if(type == 0)
		return return_error(RC(SH_BadType));

	// allocate and initialize a new ote for the object
	W_DO(new_ote(type,
				 0,
				 VolRef::null,
				 (ObjFlags)(OF_New | OF_Dirty | OF_Transient),
				 0,
				 NEW_OBJ_LOCK_MODE,
				 ote));

	// allocate memory for the object
	W_DO(alloc_mem(ote));

	// if the object contains indices or collections, write it back
	// right away
	if(type->nindices > 0){
		return return_error(RC(SH_BadType));
	}

	// if the object's pool wasn't on the list of pools in which
	// objects have been created, then add it


	// initialize the object's vptr; used to be done by Ref<T> member.
	type->setup_vtable(ote->obj);


	return return_error(RCOK);
}

w_rc_t
ObjCache::create_registered(const char *path,
							mode_t mode,
							rType *type,
							OTEntry *&ote)		// OUT ote
{
	int vindex;
	LOID loid, type_loid;

	// make sure the object cache has been initialized
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }

	// For backwards compatibility, we check to see if the volid is
	// null.  If it is, then this isn't a real type object, and we
	// must fill in the volid to make the type id look like a real
	// loid.  We will use the id of the volume on which the object
	// will be created, which is determined by stating the directory
	// in which it will be created.
	if(type->loid.id.lvid == VolId::null){

		char dirpath[256], *s;
		SysProps props;

		// get the pathname of the directory by removing the trailing
		// path component
		strcpy(dirpath, path);
		if((s = strrchr(dirpath, '/')) == 0)
			strcpy(dirpath, ".");
		else if(s == dirpath)
			dirpath[1] = '\0';
		else
			*s = '\0';

		// stat the directory object
		VAS_DO(vas->sysprops(dirpath, &props));

		// record the volid in the transient type object
		type_loid.set(props.volume, type->loid.id.serial);
	}
	else // get a volume relative oid for the type.in order to check if 
	{
		// this bites. if we really need to get the vol, we have
		// to do anothre call.We ne
		// VAS_DO(vas->sysprops(dirpath, &props));
		// W_DO(get_type_loid(ote,type,type_loid));
		type_loid = type->loid;
	}

	// ask the vas to create the new object
	// first, an unfortunate special case for unix file.
	if (type_loid.id.serial== ReservedSerial::_UnixFile)
	{
		VAS_DO(vas->mkRegistered(path,
							(mode_t)mode,
							type_loid.id,
							0,0,0,0,
							&loid.id));

	}
	else
	{
		VAS_DO(vas->mkRegistered(path,
							(mode_t)mode,
							type_loid.id,
							type->size,
							0,
							type->size,
							type->nindices,
							&loid.id));
	}

	// get the vindex of the created object
	vindex = vt.lookup(loid.volid(), true);
	dassert(vindex != VolumeTable::NoEntry);

	// allocate and initialize a new ote for the object. Note: we
	// don't set OF_Dirty here.  On the off-chance that the object
	// isn't written (initialized), we won't bother to write anything
	// back to the server.
	// really bad idea, that.  Always mark it dirty.
	W_DO(new_ote(type,
				vindex,
				loid.volref(),
				(ObjFlags)0,
				0,
				NEW_OBJ_LOCK_MODE,
				ote));

	// allocate memory for the object
	W_DO(alloc_mem(ote));

	// add the new object to the loid table
	W_DO(lt.add(ote));

	// for objects containing indicies, we make a fake call to
	// prepare_for_disk here to assure that the IndexId's in the
	// object get set properly.  Also mark the object dirty to
	// force an eventual write.
	if (type->nindices >0)
	{
		vec_t heap;
		smsize_t tlen = 0;
		smsize_t hlen = 0;
		W_DO(prepare_for_disk(ote, &heap, hlen, tlen));
		// but, since it is still cached, put pack into mem. formo.
		StringRec srec;
		srec.string = 0;
		srec.length = 0;
		// pass in 0 srec, since there can't be any heap at this point??
		W_DO(prepare_for_application(ote->type->loid, ote, srec, tlen ));

		// mark the object dirty
		ote->flags |= OF_Dirty;
	} 
	// always mark it dirty; else, it can come back as
	// garbage the next time around.
	ote->flags |= OF_Dirty; // hmm seems to be necessary.  
	// increment type refcount
	type->inc_refcount(1);

	// initialize the object's vptr; used to be done by Ref<T> member.
	type->setup_vtable(ote->obj);

	// update statistics
	++stats.regcreates;

	return return_error(RCOK);
}

w_rc_t
ObjCache::create_pool(const char *path,
					mode_t mode,
					OTEntry *&ote)				// OUT ote
{
	// make sure the object cache has been initialized
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	LOID loid;

	// ask the vas to create the pool
	VAS_DO(vas->mkPool(path, mode, &loid.id));

	// allocate and initialize an OT entry for the pool
	W_DO(alloc_ote(ote));
	ote->volume = vt.lookup(loid.volid(), true);
	dassert(ote->volume != VolumeTable::NoEntry);

	ote->volref = loid.volref();
	ote->obj = 0;
	ote->lockmode = NO_LOCK_MODE;

	// add the ote to the loid table
	W_DO(lt.add(ote));

	return return_error(RCOK);
}

w_rc_t ObjCache::create_xref(const char *path, mode_t mode, OTEntry *ote)
{
	// make sure the object cache has been initialized
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	LOID loid, result;

	// get the object's full LOID
	W_DO(get_loid(ote, loid));

	// if the object is new, we must first create it on the server
	// side or else mkXref will fail
	if(ote->is_new()){
		W_DO(writeback(ote));
	}

	// ask the vas to make the xref
	VAS_DO(vas->mkXref(path, mode, loid.id, &result.id));

	return return_error(RCOK);
}

w_rc_t
ObjCache::free_mem(OTEntry *ote)
{
	BehindRec *brec;
	BehindRec *p;

	// prepare_to_free(ote);
	// prepare_to_free is more consistent with other usage, but
	// we cheat here and call a type member directly.
	if (ote->type != NULL && ote->obj != NULL)
		ote->type->__apply(DeAllocate,ote->obj);
	brec=get_brec(ote->obj);
	if (brec->pin_count)
	// then, pin counts haven't been properly maintained
	{
		OCRef  err_ref;
		err_ref.init(ote->obj);
		cerr << "non-zero pin count" << brec->pin_count ;
		cerr << " for object from OCRef " << err_ref << endl;
	}

	// usage statistics
	// now maintianed in mmgr.
	//stats.mem_used -= brec->osize;
	//--stats.obj_used ;

	p = get_brec(ote->obj);
	// ignore ref & pin at this point... pin is dubious.
	p->pin_count=0;
	p->ref_bit =0;

	if (ote->flags & OF_Transient) // don't use memory allocator, for now..
		delete caddr_t(p);
	else
		mmgr.free(caddr_t(p));

	ote->obj = 0;

	return return_error(RCOK);
}

w_rc_t
ObjCache::remove_destroyed(OTEntry *ote)
{
	dassert(!ote->is_secondary());

	// Clean out everything we know about the object, except whether
	// it was new or not.  Clear all flags but OF_New.

	ote->pool = 0;
	// ote->lockmode = DESTROY_LOCK_MODE;
	// pretend to give up lock; this will make any access
	// try to go through the sm and get an exception.
	ote->lockmode = NO_LOCK_MODE;
	ote->flags &= OF_New;
	if(ote->obj != 0){
		if (ote->obj >=mmgr.space && ote->obj < mmgr.space+mmgr.total_bytes)
		// it is in the cache area.
		{
			CacheBHdr *cpt = get_brec(ote->obj);
			mmgr.deallocate(cpt); // really detach it.
			mmgr.reuse(cpt);
		}
		else // use more general routine
			W_DO(free_mem(ote));
	}
	ote->type = 0;

	// update statistics
	++stats.ndestroys;

	return return_error(RCOK);
}

w_rc_t
ObjCache::destroy_object(OTEntry *ote)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	OTEntry *primary;
	LOID loid;

	if(ote == 0)
		return return_error(RC(SH_BadObject));

	// make sure we're dealing with the object's primary ote
	if(ote->is_secondary()){
		W_DO(snap(ote, primary));
	}
	else
		primary = ote;

	// fix up bi-directional relationships, indexed attributes, etc.
	W_DO(prepare_to_destroy(ote));

	// Destroy the object.  Note: we use the ote that was passed to
	// us, even if it is not the primary ote.  This gives the server
	// an opportunity to retire its non-local alias for the destroyed
	// object.
	if(!ote->is_new()){
		W_DO(get_loid(ote, loid));
		VAS_DO(vas->rmAnonymous(loid.id));
	}

	// remove the object from the cache and reset the ote
	W_DO(remove_destroyed(primary));

	return return_error(RCOK);
}

w_rc_t
ObjCache::unlink(const char *path)
{
	// make sure the object cache has been initialized
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }

	LOID 	loid;
	bool found;

	VAS_DO(vas->lookup(path, &loid.id, &found) );

	if(!found) {
		return return_error(RC(SH_NotFound));
	} else {
		bool remove;

		// first stage of object removal
		VAS_DO(vas->rmLink1(path, &loid.id, &remove));

		//
		// Server tells us if the link count hit 0 by
		// setting remove to true.
		// We must destroy the object.  
		//
		if(remove){

			OTEntry *ote;

			// NB: If an error occurs before we call rmLink2, then we may
			// not be able to commit the transaction - the vas will not
			// let us commit unless both rmLink1 and rmLink2 are called on
			// the object.  For now, we don't try to do anything fancy
			// here.

			// get the object's primary ote (ths should be simple, because
			// the loid returned from rmLink1 should always be primary)
			W_DO(get_primary_ote(loid, ote));

			// deal with bi-directional relationships, indexed attributes,
			// etc.
			W_DO(prepare_to_destroy(ote));

			// do the second stage of object removal
			VAS_DO(vas->rmLink2(loid.id));

			// remove the object from the cache and reset its ote
			W_DO(remove_destroyed(ote));
		}
	}

	return return_error(RCOK);
}

		
////////////////////////////////////////////////////////////////
//
//		    Object Metadata Methods
//
////////////////////////////////////////////////////////////////

// BUG: Neither of the two forms of stat takes into account changes to
// hsize in the current transaction.  For new objects, hsize is
// reported as 0.  For pre-existing objects, the last committed size
// is given.  We hope to address this bug in the beta release.

w_rc_t
ObjCache::stat(const char *path,
				OStat *osp)						// OUT osp
{
	SysProps props;
	VAS_DO(vas->sysprops(path, &props));
	stat(&props, osp);
	return return_error(RCOK);
}

w_rc_t
ObjCache::stat(OTEntry *ote,
				OStat *osp)						// OUT osp
{
	SysProps props;

	// make sure we;re dealing with the object's primary ote
	if(ote->is_secondary()){
		OTEntry *primary;
		W_DO(snap(ote, primary));
		ote = primary;
	}

	// make sure it's not a destroyed object
	if(ote->is_new() && ote->obj == 0)
		return return_error(RC(SH_BadObject));

	// assign the object a serial
	if(!ote->has_serial())
		W_DO(get_volref(ote));

	// if it's a new object, fill in the info from the ote.  BUG: we
	// don't know the hsize, so the size we report is just the csize.
	if(ote->is_new()){

		W_DO(get_loid(ote, osp->loid));
		osp->type_loid.id = ote->type->loid.id;
		osp->csize = ote->type->size;
		osp->hsize = compute_heapsize(ote,0); // caculate current heap.
		osp->nindices = ote->type->nindices;
		osp->kind = KindAnonymous;
		W_DO(get_loid(ote->pool, osp->astat.pool));
	}

	// if it's not new, get the sysprops from the vas
	else{
		W_DO(sysprops(ote, &props));
		stat(&props, osp);
	}

	return return_error(RCOK);
}

void
ObjCache::stat(SysProps *props, OStat *osp)
{
	VolRef type_volref;

	// object's loid
	osp->loid.set(props->volume, props->ref);

	// loid of type object
	type_volref.set(props->type);
	if(ReservedSerial::is_reserved(type_volref))
		osp->type_loid.set(VolId::null, type_volref);
	else
		osp->type_loid.set(props->volume, type_volref);

	// size, nindices, kind
	osp->csize = props->csize;
	osp->hsize = props->hsize;
	osp->nindices = props->nindex;
	osp->kind = props->tag;

	// registered of anonymous properties
	switch(props->tag){
	case KindAnonymous:
		osp->astat.pool.set(props->volume, props->anon_pool);
		break;

	case KindRegistered:
		osp->rstat.nlink = props->reg_nlink;
		osp->rstat.mode = props->reg_mode;
		osp->rstat.uid = props->reg_uid;
		osp->rstat.gid = props->reg_gid;
		osp->rstat.atime = props->reg_atime;
		osp->rstat.mtime = props->reg_mtime;
		osp->rstat.ctime = props->reg_ctime;
		break;

	// to placate compilers (we should never see any of these)
	case KindTransient:
		break;
	}
	// see if the object is cached; if so,compute its heap size...
	// this could perhaps be streamlined...
	OTEntry *ote;
	LOID cloid = osp->loid;

	if (get_ote(cloid, ote)) return;
	if(ote->is_secondary()){
		OTEntry *primary;
		if (snap(ote, primary)) return;
		ote = primary;
	}
	if (ote->obj && ote->is_dirty()) 
	// object is cached and modified...
	{	
		osp->hsize = compute_heapsize(ote,0);
		// should add tsize at some point...
	}

}

w_rc_t
ObjCache::sysprops(OTEntry *ote, SysProps *props,
					bool cache_obj, LockMode lm, int * prefetch_count)
{
	LOID loid, type_loid;
	SysProps sp;

	if(ote == 0)
		return return_error(RC(SH_BadObject));

	if(props == 0)
		props = &sp;

	bool	cached_something=false;
	OTEntry * orig_ote=0;

	// get the sysprops
	W_DO(get_loid(ote, loid));
	// reserved oid hack.
	if (ote->is_secondary() && ReservedSerial::is_reserved(ote->volref))
	{
		OTEntry * primary;
		W_DO(snap(ote, primary));
		orig_ote = ote;
		ote = primary;
		W_DO(get_loid(ote,loid));
	}
	if (do_prefetch && cache_obj)
	{
		VAS_DO(vas->sysprops(loid.id, props, cache_obj, lm,0,0,&cached_something));
		// try to do prefetch here??
		if (cached_something)
				prefetch(0);
	}
	else
	{
		VAS_DO(vas->sysprops(loid.id, props, false, lm,0,0,&cached_something));
		
	}

	// if this is a secondary ref, create an alias now that we know
	// the primary ref
	if(ote->is_secondary()){

		int vindex;
		VolRef volref;
		OTEntry *prim_ote;

		// props->volume is the "snapped" volumeid,
		// i.e, the volume on which the object resides
		// it might be one we've not see yet
		vindex = vt.lookup(props->volume, true);
		dassert(vindex != VolumeTable::NoEntry);

		volref.set(props->ref);
		W_DO(add_alias(ote, vindex, volref, prim_ote));

		ote = prim_ote;
	}

	// update lock mode
	if(ote->lockmode < lm)
		ote->lockmode = lm;

	// fill in the type
	type_loid.set(props->volume, props->type);
	// we need to be shure to get the primary ote...
	if (type_loid.is_remote())
	{
		OTEntry *prim_tote;
		W_DO(get_primary_ote(type_loid, prim_tote));
		W_DO(get_primary_loid(prim_tote,type_loid));
	}
	// now set the local type ptr.
	ote->type = get_type_object(type_loid);
	if (orig_ote)
		orig_ote->type = ote->type;

	// fill in the pool
	if(props->tag == KindAnonymous){
		VolRef pool_volref;
		pool_volref.set(props->anon_pool);
		W_DO(get_ote(ote->volume, pool_volref, ote->pool));
	}

	return return_error(RCOK);
}

w_rc_t
ObjCache::get_type(OTEntry *ote, rType *&type)				// OUT type
{
    FUNC(ObjCache::get_type);
    if(ote == 0)
	return return_error(RC(SH_BadObject));

    // sysprops fills in ote->type
    if(ote->type == 0)
	W_DO(sysprops(ote));

    type = ote->type;
    DBG(<<"get_type returns type " << type);

    return return_error(RCOK);
}

w_rc_t
ObjCache::get_type(OTEntry *ote, LOID &type_loid) // OUT type_loid
{
    FUNC(ObjCache::get_type);
    if(ote == 0)
	return return_error(RC(SH_BadObject));

    if(ote->type == 0){

	LOID loid;
	SysProps props;

	W_DO(get_loid(ote, loid));
	VAS_DO(vas->sysprops(loid.id, &props));
	DBG(<<"sysprops done, about to set("
		<< props.volume << "," << props.type <<")");
	type_loid.set(props.volume, props.type);
    }
    else
	type_loid.set(ote->type->loid);

    DBG(<<"get_type returns type_loid " << type_loid);

    return return_error(RCOK);
}

w_rc_t
ObjCache::get_pool(OTEntry *ote,
					OTEntry *&pool)				// OUT pool
{
	if(ote == 0)
		return return_error(RC(SH_BadObject));

	// sysprops fills in ote->pool
	if(ote->pool == 0)
		W_DO(sysprops(ote));

	pool = ote->pool;

	return return_error(RCOK);
}

w_rc_t
ObjCache::get_kind(OTEntry *ote,
					ObjectKind &kind)				// OUT kind
{
	SysProps props;

	if(ote == 0)
		return return_error(RC(SH_BadObject));

	// get the sysprops
	W_DO(sysprops(ote, &props));

	kind = props.tag;

	return return_error(RCOK);
}

w_rc_t
ObjCache::get_lockmode(OTEntry *ote,
						LockMode &lm)				// OUT lm
{
	if(ote == 0)
		return return_error(RC(SH_BadObject));

	lm = (LockMode)ote->lockmode;
	return return_error(RCOK);
}

w_rc_t
ObjCache::get_primary_loid(OTEntry *ote,
							LOID &loid)				// OUT loid
{
	OTEntry *primary;

	if(ote == 0)
		return return_error(RC(SH_BadObject));

	// make sure the object has a serial number
	if(!ote->has_serial()){
		W_DO(get_volref(ote));
	}

	// if it already has a serial, make sure it's a primary serial
	else if(ote->is_secondary()){
		W_DO(snap(ote, primary));
		ote = primary;
	}

	// initalize the loid
	loid.set(vt[ote->volume], ote->volref);

	return return_error(RCOK);
}

w_rc_t
ObjCache::get_loid(OTEntry *ote,
					LOID &loid)						// OUT loid
{
	if(ote == 0)
		return return_error(RC(SH_BadObject));

	// if the object has no serial number...
	if(!ote->has_serial()){
		W_DO(get_volref(ote));
	}

	// initalize the loid
	loid.set(vt[ote->volume], ote->volref);

	return return_error(RCOK);
}

w_rc_t
ObjCache::simple_get_loid(OTEntry *ote,
						LOID &loid)				// OUT loid
{
	if(ote == 0)
		return return_error(RC(SH_BadObject));

	// if the object has no serial, return the null loid
	if(!ote->has_serial())
		loid.set(LOID::null);
	else
		loid.set(vt[ote->volume], ote->volref);

	return return_error(RCOK);
}

w_rc_t
ObjCache::get_volref(OTEntry *ote)
{
	if(ote == 0)
		return return_error(RC(SH_BadObject));

	// if this object already has a serial then do nothing
	if(ote->has_serial())
		return return_error(RCOK);

	// if the object isn't new and doesn't have a serial, then we've
	// screwed up
	if(!ote->is_new())
		return return_error(RC(SH_Internal));

	// get a new serial from the object's volume
	W_DO(new_serial(ote->volume, ote->volref));

	// enter the ote into the loid table
	W_DO(lt.add(ote));

	return return_error(RCOK);
}

w_rc_t
ObjCache::valid(OTEntry *ote, LockMode lm)
{
	return fix_rc(lock_obj(ote, lm, lm == NO_LOCK_MODE));
	// return fix_rc(lock_obj(ote, lm, block));
}

////////////////////////////////////////////////////////////////
//
//		    Methods for scanning pools
//
////////////////////////////////////////////////////////////////

w_rc_t
ObjCache::poolscan_open(const char *path,
						Cookie *cookie)				// OUT cookie
{
		LOID loid;
		bool		found;
		VAS_DO(vas->lookup(path, &loid.id, &found));
		if(!found) {
			return return_error(RC(SH_NotFound));
	}

	VAS_DO(vas->openPoolScan(path, cookie));
	return return_error(RCOK);
}

w_rc_t
ObjCache::poolscan_open(OTEntry *ote,
						Cookie *cookie)				// OUT cookie
{
	LOID loid;

	if(ote == 0 || !ote->has_serial())
		return return_error(RC(SH_BadPool));

	// get the pool's loid
	W_DO(get_loid(ote, loid));

	// open scan
	VAS_DO(vas->openPoolScan(loid.id, cookie));

	return return_error(RCOK);
}

w_rc_t
ObjCache::poolscan_next(Cookie *cookie,
						bool fetch_obj,
						LockMode lm,
						OTEntry *&ote)				// OUT ote
{
	bool eof = false;
	LOID loid;

	// try to advance the scan
	VAS_DO(vas->nextPoolScan(cookie, &eof, &loid.id));

	// check for end of scan
	if(eof)
		return return_error(RC(SH_EndOfScan));

	// if it's not the end of the scan
	else{

		// get the object's OT entry
		W_DO(get_ote(loid, ote));

		// if we want to get the object...
		if(fetch_obj){

			// see if we can reclaim a freed object...
			if (ote->obj )
			{
				if( get_brec(ote->obj)->is_free())
				// tell mmgr to reclaim it.
					mmgr.reclaim(get_brec(ote->obj));
				if (ote->is_unswizzled())
					W_DO(reclaim_object(ote));
			}
			// if the object isn't cached, get it
			if(ote->obj == 0){
				W_DO(fetch(ote, lm));
			}

			// if it is cached but not at the requested lockmode, get a
			// new lock
			else if(ote->lockmode < lm){
				W_DO(lock_obj(ote, lm));
			}
		}
	}

	return return_error(RCOK);
}

w_rc_t
ObjCache::poolscan_close(Cookie cookie)
{
	VAS_DO(vas->closePoolScan(cookie));
	return return_error(RCOK);
}

////////////////////////////////////////////////////////////////
//
//		 Methods for scanning directories
//
////////////////////////////////////////////////////////////////

w_rc_t
ObjCache::dirscan_open(const char *path, LOID &loid)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	SysProps props;
	int vindex;
	LOID type_loid;
	OTEntry *ote;

	// lookup the object and get its type
	W_DO(lookup(path, type_loid, ote));

	// if it's not a directory, then error
	if(type_loid.id != ReservedOid::_Directory)
		return return_error(RC(SH_NotADir));

	W_DO(get_loid(ote, loid));

	return return_error(RCOK);
}

w_rc_t
ObjCache::dirscan_next(const LOID &loid, Cookie *cookie,
						char *buf, int buflen, int *nentries)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	if(*cookie == TerminalCookie) {
		return return_error(RC(SH_EndOfScan));
	}

	VAS_DO(vas->getDirEntries(loid.id, buf, buflen, nentries,
							cookie));

	if(nentries == 0) {
		if(*cookie == TerminalCookie) { 
		    return return_error(RC(SH_EndOfScan));
		} else {
			// cerr << "Not enough space for a single entry" << endl;
			// TODO fix this -- create a new RC
			return return_error(RC(SH_ScanBufTooSmall));
		}
	}
	return return_error(RCOK);
}

////////////////////////////////////////////////////////////////
//
//	      Methods to read objects into the cache
//		and to prepare objects for writing.
//
////////////////////////////////////////////////////////////////

w_rc_t
ObjCache::is_resident(OTEntry *ote,
					bool &res)				// OUT res
{
	if(ote == 0)
		return return_error(RC(SH_BadObject));

	if(ote->is_secondary()){
		OTEntry *primary;
		W_DO(snap(ote, primary));
		ote = primary;
	}

	res = (ote->obj != 0);
	return return_error(RCOK);
}

w_rc_t
ObjCache::lock_obj(OTEntry *ote, LockMode lm, bool force)
{
	LOID loid;

	if(ote == 0)
		return return_error(RC(SH_BadObject));

	if(lm < MIN_LOCK_MODE || lm > MAX_LOCK_MODE)
		return return_error(RC(SH_BadLockMode));

	// make sure we're dealing with the object's primary ote
	if(ote->is_secondary()){
		OTEntry *primary;
		W_DO(snap(ote, primary));
		ote = primary;
	}

	// check for deleted object
	if(ote->is_new() && ote->obj == 0)
		return return_error(RC(SH_BadObject));

	// see if we already have the requested lock
	// don't force the lock, ever; Force=true
	// was never used. We hijack force as a parameter
	// for Blocking/nonblocking
	//the blocking parameter. i.e. now force true means wait
	// for the lock, force false means don't wait.
	if(/* !force && */ ote->lockmode >= lm)
		return return_error(RCOK);

	// request the lock from the vas
	W_DO(get_loid(ote, loid));
	VAS_DO(vas->lockObj(loid.id, lm,force? Blocking:NonBlocking));

	// update the lock mode
	ote->lockmode = lm;

	return return_error(RCOK);
}

w_rc_t
ObjCache::make_writable(OTEntry *ote, LockMode lm)
{
	OTEntry *primary;

	if(!ote) {
		return  RC(SH_BadObject);
	}
    if(ote->is_dirty()) return RCOK;

	if(lm < MIN_WRITE_LOCK_MODE || lm > MAX_LOCK_MODE)
		return return_error(RC(SH_BadLockMode));

	// make sure we're dealing with the object's primary ote
	if(ote->is_secondary()){
		W_DO(snap(ote, primary));
		ote = primary;
	}

	// if the object is new but not cached, then it must have been
	// destroyed
	if(ote->is_new() && !ote->obj)
		return return_error(RC(SH_BadObject));

	// see if we can reclaim a freed object...
	if (ote->obj && get_brec(ote->obj)->is_free())
	// mark it used so it doesn't get reallocated.
	{
		mmgr.reclaim(get_brec(ote->obj));
	}
	// also, need to reswizzle here if necessary..
	if (ote->obj &&ote->is_unswizzled())
		W_DO(reclaim_object(ote));
	// if the object isn't cached, then go get it
	if(ote->obj == 0){
		W_DO(fetch(ote, lm));
	}

	// if already have the object but it isn't at the requested lock
	// mode...
	else if(ote->lockmode < lm){
		W_DO(lock_obj(ote, lm));
	}

	// mark the object dirty
	ote->flags |= OF_Dirty;

	// update statistics
	++stats.nupdates;

	return return_error(RCOK);
}

// NB: If the given ote contains a secondary loid, then we first go to
// the server to snap it, and then go to the server a second time to
// read the object.  The reason we don't just get the object using the
// secondary loid (we could, after all) is that we have no way of
// knowing whether we already have the object, having gotten it via a
// different loid.  We figure 2 trips to the server for secondary refs
// (uncommon), is better than reading objects unnecessarily.

w_rc_t
ObjCache::fetch(OTEntry *ote,
				LockMode lm)
{
	caddr_t p, obj;
	char *string;
	BehindRec *brec;
	vec_t vec;
	StringRec srec;
	rType *type;
	ObjectSize nbytes, more;
	SysProps props;
	LOID loid, type_loid;
	bool	cached_something=false;
	// flag so we don't try to do prefect recursively.
	// static bool in_prefetch = false;
	int prefetch_count = 0;
	// mmgr.audit();

	if(ote == 0)
		return return_error(RC(SH_BadObject));

	// check the lock mode
	if(lm <= NO_LOCK_MODE || lm > MAX_LOCK_MODE)
		return return_error(RC(SH_BadLockMode));

	// make sure this isn't a secondary OT entry
	if(ote->is_secondary()){
		OTEntry *primary;
		W_DO(snap(ote, primary));
		ote = primary;
	}

	// if the object is new but not cached, then it must have been
	// destroyed
	if(ote->is_new() && !ote->obj)
		return return_error(RC(SH_BadObject));

	// if the object is already cached, then we're done
	if(ote->obj != 0)
		return return_error(RCOK);

	////////////////////////////////////////////////////////////
	//
	//			Step 1: Get the sysprops
	//
	////////////////////////////////////////////////////////////

	// make sure the requested lock mode isn't lower than the object's
	// current lock mode
	if(ote->lockmode > lm)
		lm = (LockMode)(ote->lockmode);

	// get the object's loid
	W_DO(get_loid(ote, loid));

	// get the object's sysprops
	// W_DO(sysprops(ote, &props, do_prefetch?true:false, lm));
	// save the rc, and print a message.
	// W_DO(sysprops(ote, &props, do_prefetch?true:false, lm));
	w_rc_t rc;
	rc= sysprops(ote, &props, do_prefetch?true:false, lm);
	if (rc)
	{
		cerr << "oc::fetch: sysprops failed for object " << loid << endl;
		return rc;
	}
		

	// obj may have been fetched by a prefetch in sysprops...
	if(ote->obj != 0)
		return return_error(RCOK);


	// get the object's type loid
	type_loid.set(vt[ote->volume], props.type);

	// if it's a registered object, it has no pool
	if(props.tag == KindRegistered)
		ote->pool = 0;

	// otherwise, get a pointer to its pool if we don't already have it
	else if(ote->pool == 0){
		VolRef pool_volref;
		pool_volref.set(props.anon_pool);
		W_DO(get_ote(ote->volume, pool_volref, ote->pool));
	}

	////////////////////////////////////////////////////////////
	//
	//		  Step 2: Prepare to read the object
	//
	////////////////////////////////////////////////////////////

	// allocate space for the object in the object cache

	// allocate memory contiguously
	int mlen = 
		sizeof(BehindRec) 
		+ roundup(props.csize, sizeof(double))
		+ props.hsize;
	int tlen = props.csize + props.hsize - props.tstart;

	// round it up for good measure
	if (roundup(mlen,sizeof(double)) != mlen)
		mlen = roundup(mlen,sizeof(double));

	// need another byte to be able to pad text attr with a null
	else if (tlen != 0)
		++mlen;

	// special case: need to prepend space for string header.
	if (type_loid.id.serial== ReservedSerial::_UnixFile)
		mlen += sizeof(SdlUnixFile);

	// finally we know how much memory we need; at this point,
	// check if cache replacement is necessary

	W_DO(mmgr.alloc(mlen, p));
	// mmgr.audit();
	brec = (BehindRec *)p;
	obj = p + sizeof(BehindRec);
#ifdef nocontig
	vec.put(obj, (int)props.csize);
#else
	if (type_loid.id.serial== ReservedSerial::_UnixFile)
		vec.put(obj+sizeof(SdlUnixFile), props.csize + props.hsize);
	else
		vec.put(obj, props.csize + props.hsize);
#endif

	// allocate space for the object's string, if any
	if(props.hsize == 0){
		srec.string = 0;
		srec.length = 0;
	}

	else{
#ifdef nocontig
		srec.string = p + sizeof(BehindRec) +
			roundup(props.csize, sizeof(double));
#else
		// plan p: to make the read contiguous, 
		// start the heap immediately after the core;
		// the prepare routines must be adjusted to
		// round the pointer up & add the extra space to the heap.
		srec.string = p + sizeof(BehindRec) + props.csize;
#endif

		if (type_loid.id.serial== ReservedSerial::_UnixFile)
				srec.string += sizeof(SdlUnixFile);
				
		srec.length = (int)props.hsize;
#ifdef nocontig
		vec.put(srec.string, (int)props.hsize);
#endif
	}

	// read the object from the vas
	// FIX: protcol for reading very large objects
	nbytes = 0;
	more = 0;
	if (props.csize + props.hsize)
		VAS_DO(vas->readObj(loid.id, 0, props.csize + props.hsize,
						lm, vec, &nbytes, &more, &loid.id,
						&cached_something))
	// note: client side should never get more != 0, but server
	// resident cache may; for robustness, we always handle it.

	if (more > 0)
	{
		ObjectSize mbytes = nbytes;
		dassert(nbytes+more <= props.csize + props.hsize);
		while (mbytes < props.csize)
		{
			nbytes = 0; more = 0;
			vec.reset();
			vec.put(obj+mbytes, props.csize - mbytes);
			VAS_DO(vas->readObj(loid.id, mbytes, props.csize + props.hsize- mbytes,
						lm, vec, &nbytes, &more, &loid.id,
						&cached_something))
			mbytes += nbytes;
		}
		// next read heap.
		while (mbytes < props.csize + props.hsize)
		{
			nbytes = 0; more = 0;
			vec.reset();
			vec.put(srec.string +(mbytes- props.csize) , 
				props.hsize + props.csize - mbytes);
			VAS_DO(vas->readObj(loid.id, mbytes, props.csize + props.hsize - mbytes,
						lm, vec, &nbytes, &more, &loid.id,
						&cached_something))
			mbytes += nbytes;
		}
		dassert(more == 0);
	}



	////////////////////////////////////////////////////////////
	//
	//		 Step 3: Get the object ready for consumption
	//
	////////////////////////////////////////////////////////////

	// call up to the language binding to swizzle, byte-swap, etc
	// update ote
	ote->obj = obj;
	dassert(obj != 0);
	ote->lockmode = lm;

	// initialize the behind rec
	brec->otentry = ote;
	brec->pin_count=0;
	brec->ref_bit =0;

	W_DO(prepare_for_application(type_loid, ote, srec, tlen));


	// BEWARE: Do not clear OF_Dirty here.
	// 1. It's not necessary - all OTEs start out with OF_Dirty clear;
	//	it is set in make_writable and cleared in writeback.
	// 2. It's not safe.  We may have marked the OTE dirty in preparation
	//	for writing the object before noticing that the object isn't
	//	cached.  By clearing OF_Dirty here, we may clobber a correctly
	//	set dirty flag.


	// update statistics
	++stats.ncached;
	stats.nbytes += mlen;

	// usage statistics
	stats.mem_used += mlen;
	stats.obj_used ++;
	if (do_logops)
		log_opr(ote,"fetch");

	// finally, do any prefetching
	if(do_prefetch && cached_something)
				prefetch(ote);

	dassert(ote->obj != 0);
	// mmgr.audit();

	return return_error(RCOK);
}


////////////////////////////////////////////////////////////////
//
//      the prefetch routine is called when a new page has
// 	been read by the server, as detected by flags to
//	the sysprops and readobject calls.
//      primary and secondary OT entries and serial numbers
//
////////////////////////////////////////////////////////////////

void
ObjCache::prefetch(OTEntry * ote)
// finally, if we are in this mode, fetch any other objects on the
// page. This function
{
	int		count;
	if (vas->num_cached_oids(&count) || count <= 0) 
		return;
	// gcc/g++ism: dynamic array on count
	// but need a new block...
	// this gcc-ism doesn't work, apparently.
	// lrid_t list[count];
	lrid_t				*list = new lrid_t[count];
	if ( vas->cached_oids(&count, list))
	{ delete list; return;}

	// this procedure is not reentrant; put in a check for this.
	static int in_prefetch = 0;
	// dassert(!in_prefetch);
	// ok it may be reentrant??
	if (in_prefetch)
	{ delete list; return;} // don't do anything
	in_prefetch = 1;

	// for each cached object, translalte the
	// lrid_t to a LOID, get the ote for the LOID,
	// and fetch the object if it's not there already.
	// also, link together via the page_link field.
	CacheBHdr * first_cpt = 0;
	CacheBHdr *cpt = 0;
	if (ote)
		first_cpt = get_brec(ote->obj);
	dassert((ote==0) || (ote->obj != 0));
	if (first_cpt) first_cpt->clear_pagelist();
	if (do_logops)
		log_opr(ote,"prefetch");
	// get ote's for all objs. on the page; fetch if not already 
	// cached.  Link all cached objects on the page together
	// through the add_pagelist method on the header.
	for(int i=0; i<count; i++) {
		LOID ploid;
		ploid.id = list[i];
		w_rc_t rc;
		// get ote for the new obj.
		if( get_ote(ploid,ote)) // don't process ote if there is an error
			continue;
		if (ote->obj==0) { // not cached; fetch now.
			if (fetch(ote)) // don't process if in error.
				continue;
		}
		// clear any prexisting pagelist.
		if (!do_pagecluster)
			continue;
		cpt = get_brec(ote->obj);
		if (cpt->page_link)
			cpt->clear_pagelist();
		if (!first_cpt) first_cpt = cpt;
		first_cpt->add_to_pagelist(cpt);
		// disable uncaching of anything on this list by pinning the obj.
		++cpt->pin_count; 
	}
	// now, decrement pin count to allow objs. to be freed.
	cpt = first_cpt;
	if (do_pagecluster)
		while (cpt) {
			--cpt->pin_count;
			if (cpt->page_link == first_cpt)
				break;
			cpt = cpt->page_link;
		}
	in_prefetch = 0;
	delete list;
}

////////////////////////////////////////////////////////////////
//
//       Methods for dealing with serial numbers and with
//      primary and secondary OT entries and serial numbers
//
////////////////////////////////////////////////////////////////

w_rc_t
ObjCache::allocate_serials(int vindex, int nserials)
{
	int nvolumes, ncreates, i;
	lid_t id;

	dassert(vindex != VolumeTable::NoEntry);

	// get the number of volumes from the volume table
	nvolumes = vt.nvolumes();

	// figure out how many serials to ask for (if any)
	ncreates = vt.get_ncreates(vindex);
	if(ncreates < 1 && nserials < 1)
		return return_error(RCOK);
	if(nserials > ncreates)
		ncreates = nserials;

	// expand the serial array size if needed
	if(serial_array_size <= vindex){

		SerialSet *tmp;

		// allocate a new array
		tmp = new SerialSet[nvolumes];
		if(tmp == 0)
			return return_error(RC(SH_OutOfMemory));

		// copy the old to the new
		for(i = 0; i < serial_array_size; ++i)
			tmp[i] = serials[i];

		// initialize the new entries
		for( ; i < nvolumes; ++i)
			tmp[i].nserials = 0;

		// free up the old
		if(serials != 0)
			delete [] serials;

		// install the new
		serial_array_size = nvolumes;
		serials = tmp;
	}

	// get the serials from the vas
	VAS_DO(vas->mkVolRef(vt[vindex], &id, ncreates));

	serials[vindex].serial.set(id.serial);
	serials[vindex].nserials = ncreates;

	// reset the number of needed serials to 0 for this volume
	vt.reset_ncreates(vindex);

	return return_error(RCOK);
}

w_rc_t
ObjCache::new_serial(int vindex,
					 VolRef &volref)				// OUT volref
{
	dassert(vindex != VolumeTable::NoEntry);

	// if we don't have any preallocated serials from
	// the object's volume, get more
	if(!serials || vindex >= serial_array_size ||
		serials[vindex].nserials == 0)
		W_DO(allocate_serials(vindex));

	// take the next serial
	volref.set(serials[vindex].serial);
	if(serials[vindex].serial.increment(1))
		return return_error(RC(SH_Internal));

	--serials[vindex].nserials;

	return return_error(RCOK);
}

w_rc_t
ObjCache::add_alias(OTEntry *otentry,
					int vindex,
					VolRef &volref,
					OTEntry *&alias)				// OUT alias
{
	OTEntry *ote, *tmp;

	dassert(vindex != VolumeTable::NoEntry);

	// see if we already have an OT entry for this loid; if not, create one
	W_DO(get_ote(vindex, volref, ote));

	// add this alias to the circular linked-list of aliases
	tmp = ote->alias;
	ote->alias = otentry->alias;
	otentry->alias = tmp;

	alias = ote;
	return return_error(RCOK);
}

w_rc_t
ObjCache::add_alias(OTEntry *otentry,
					LOID &loid,
					OTEntry *&alias)				// OUT alias
{
	int vindex;
	VolRef volref;

	vindex = vt.lookup(loid.volid(), true);
	dassert(vindex != VolumeTable::NoEntry);

	volref.set(loid.volref());
	W_DO(add_alias(otentry, vindex, volref, alias));
	return return_error(RCOK);
}

w_rc_t
ObjCache::create_alias(OTEntry *ote,
						int vindex,
						OTEntry *&alias)				// OUT alias
{
	dassert(vindex != VolumeTable::NoEntry);

	const VolId &secondary_volid = vt[vindex];
	VolRef secondary_volref;
	LOID primary_loid, secondary_loid;

	// NB: It is possible that we are being asked to create on-local
	// alias to a destroyed object.  We can't always tell when this is
	// the case, so we have to be careful.  If it is an object that
	// was both created and destroyed in the current transaction, then
	// we can tell.  If it was destroyed in a previous transaction,
	// then we probably can't tell, in which case the alias will be
	// created, but any attempt to follow it will bomb.  (This is the
	// correct semantics, but it is wasteful to create aliases to
	// destroyed objects, as aliases are hard enough to clean up as it
	// is.  Nonetheless, there isn't much we can do about it, and it
	// isn't likely to happen often).  One further case to consider is
	// that the given ote might itself be a non-local alias (to a
	// destroyed object).  If so, then it is possible, but not
	// definite, that the call to snap() would bomb.  In this case, we
	// would know that the target object is destroyed.
	//
	// If we know that the target object is destroyed, we indicate
	// this by setting the ote to 0.  Then, instead of actually
	// creating the non-local alias, we just allocate an unused local
	// serial.  Same semantics, but cheaper.

	// see if the object was both created and destroyed in the current
	// transaction.
	if(ote->is_new() && ote->obj == 0)
		ote = 0;

	// otherwise, get the object's primary ote
	else if(ote->is_secondary()){

		w_rc_t rc;
		OTEntry *primary;

		// get the primary ote
		rc = snap(ote, primary);

		// if that failed...
		if(rc){

			// first see if it failed because ote points to a
			// destroyed object
			rc = fix_rc(rc);
			if(rc.err_num() != SH_BadObject)
				return RC_AUGMENT(rc);

			// if so, set ote to 0
			primary = 0;
		}
		ote = primary;
	}

	// if we were able to determine that the target object is
	// destroyed, just allocate an unused local serial instead of a
	// new non-local serial
	if(ote == 0){
		vt.inc_ncreates(vindex);
		W_DO(new_serial(vindex, secondary_volref));
	}

	// otherwise (normal case)...
	else{

		// get the object's loid (this will be the primary loid, because
		// we are using the primary ote)
		W_DO(get_loid(ote, primary_loid));

		// make an alias for the object
		VAS_DO(vas->offVolRef(secondary_volid,
							  primary_loid.id,
							  &secondary_loid.id));

		secondary_volref.set(secondary_loid.volref());
	}

	// update bookkeeping for the new alias
	W_DO(add_alias(ote, vindex, secondary_volref, alias));
	return return_error(RCOK);
}

w_rc_t
ObjCache::get_matching(OTEntry *ote,
						int vindex,
						OTEntry *&match)				// OUT match
{
	dassert(vindex != VolumeTable::NoEntry);

	// if the given otentry is the matching one...
	if(ote->volume == vindex)
		match = ote;

	// otherwise...
	else{

		OTEntry *tmp;

		// look through the list of aliases for one that matches
		for(tmp = ote->alias;
			tmp != ote && tmp->volume != vindex;
			tmp = tmp->alias);

		// if we found a match, return it
		if(tmp->volume == vindex)
			match = tmp;

		// otherwise create one
		else{
			W_DO(create_alias(ote, vindex, match));
		}
	}

	return return_error(RCOK);
}

// utility routine matching reserved serial #'s to path names for
// sdl type purposes.  This should probably be somewhere else...
char *
get_roid_path(const VolRef & vref)
{
		if (vref==ReservedSerial::_sdlExprNode)		 
				return "/types/primitive_type/sdlExprNode";
		if (vref==ReservedSerial::_sdlDeclaration)		 
				return "/types/primitive_types/sdlDeclaration";
		if (vref==ReservedSerial::_sdlTypeDecl)		 
				return "/types/primitive_types/sdlTypeDecl";
		if (vref==ReservedSerial::_sdlConstDecl)		 
				return "/types/primitive_types/sdlConstDecl";
		if (vref==ReservedSerial::_sdlArmDecl)		 
				return "/types/primitive_types/sdlArmDecl";
		if (vref==ReservedSerial::_sdlAttrDecl)		 
				return "/types/primitive_types/sdlAttrDecl";
		if (vref==ReservedSerial::_sdlRelDecl)		 
				return "/types/primitive_types/sdlRelDecl";
		if (vref==ReservedSerial::_sdlParamDecl)		 
				return "/types/primitive_types/sdlParamDecl";
		if (vref==ReservedSerial::_sdlModDecl)		 
				return "/types/primitive_types/sdlModDecl";
		if (vref==ReservedSerial::_sdlType)		 
				return "/types/primitive_types/sdlType";
		if (vref==ReservedSerial::_sdlNamedType)		 
				return "/types/primitive_types/sdlNamedType";
		if (vref==ReservedSerial::_sdlStructType)		 
				return "/types/primitive_types/sdlStructType";
		if (vref==ReservedSerial::_sdlEnumType)		 
				return "/types/primitive_types/sdlEnumType";
		if (vref==ReservedSerial::_sdlArrayType)		 
				return "/types/primitive_types/sdlArrayType";
		if (vref==ReservedSerial::_sdlCollectionType)		 
				return "/types/primitive_types/sdlCollectionType";
		if (vref==ReservedSerial::_sdlRefType)		 
				return "/types/primitive_types/sdlRefType";
		if (vref==ReservedSerial::_sdlIndexType)		 
				return "/types/primitive_types/sdlIndexType";
		if (vref==ReservedSerial::_sdlOpDecl)		 
				return "/types/primitive_types/sdlOpDecl";
		if (vref==ReservedSerial::_sdlInterfaceType)		 
				return "/types/primitive_types/sdlInterfaceType";
		if (vref==ReservedSerial::_sdlModule)		 
				return "/types/primitive_types/sdlModule";
		if (vref==ReservedSerial::_sdlUnionType)		 
				return "/types/primitive_types/sdlUnionType";
}

w_rc_t
ObjCache::snap(OTEntry *otentry,
				OTEntry *&primary)				// OUT primary
{
	OTEntry *ote;
	LOID primary_loid, secondary_loid;

	if(otentry == 0)
		return return_error(RC(SH_BadObject));

	// if this *is* the primary entry then we're done
	if(otentry->is_new() || otentry->is_primary())
		primary = otentry;

	// otherwise...
	else{

		// look through the circular linked-list of aliases for a
		// primary entry
		for(ote = otentry->alias;
			ote != otentry && ote->is_secondary();
			ote = ote->alias);

		// if we found one then return it
		if(ote != otentry)
		{
			primary = ote;
		}

		// special mapping for reserved oid's
		else if (ReservedSerial::is_reserved(otentry->volref))
		{
			char *pname = get_roid_path(ote->volref);
			if (pname) // look it up and use the name as the primare
			{
				bool found;;
				VAS_DO(vas->lookup(pname, &primary_loid.id, &found));
				if (found) {
					W_DO(get_loid(otentry, secondary_loid));
					W_DO(add_alias(otentry, primary_loid, primary));
				}
			}
		}
		// otherwise...
		else{

			// ask the vas to resolve the off-volume ref
			W_DO(get_loid(otentry, secondary_loid));
			VAS_DO(vas->snapRef(secondary_loid.id, &primary_loid.id));

			// keep track of this new alias
			W_DO(add_alias(otentry, primary_loid, primary));
		}
	}

	return return_error(RCOK);
}

w_rc_t
ObjCache::get_ote(const LOID &loid,
				  OTEntry *&ote)				// OUT ote
{
	int vindex = vt.lookup(loid.volid(), true);
	dassert(vindex != VolumeTable::NoEntry);

	W_DO(get_ote(vindex, loid.volref(), ote));
	return return_error(RCOK);
}

w_rc_t
ObjCache::get_ote(int vindex,
				  const VolRef &volref,
				  OTEntry *&ote)				// OUT ote
{
	W_DO(lt.lookup_add(vindex, volref, ote));
	return return_error(RCOK);
}

w_rc_t
ObjCache::get_primary_ote(const LOID &loid,
						  OTEntry *&primary)		// OUT primary
{
	int vindex = vt.lookup(loid.volid(), true);
	dassert(vindex != VolumeTable::NoEntry);

	W_DO(get_primary_ote(vindex, loid.volref(), primary));
	return return_error(RCOK);
}

w_rc_t
ObjCache::get_primary_ote(int vindex,
						  const VolRef &volref,
						  OTEntry *&primary)		// OUT primary
{
	OTEntry *ote;

	// find the OT entry correspnding to the given LOID, or create one
	W_DO(lt.lookup_add(vindex, volref, ote));

	// if this is the primary entry then we're done
	if(ote->is_primary())
		primary = ote;

	// otherwise...
	else{
		W_DO(snap(ote, primary));
	}

	return return_error(RCOK);
}


w_rc_t 
ObjCache::begin_batch()
{
	// keep track of start/end batch, regardless of whether batch is
	// active or not.
	if (batch_active) // internal error.
		return RC(SH_Internal);
	batch_active = true;
	if(do_batch && !inside_server) {
		if(batch_q_len > 0) {
			VAS_DO(vas->start_batch(batch_q_len));
		} else {
			VAS_DO(vas->start_batch());
		}
	}
	return RCOK;
}

w_rc_t 
ObjCache::end_batch()
{
	// keep track of start/end batch, regardless of whether batch is
	// active or not.  we do this before any vas_do stuff.
	if (!batch_active) // internal error.
		return RC(SH_Internal);
	batch_active = false;
	batched_results_list list;
	if(do_batch && !inside_server) {
		VAS_DO(vas->send_batch(list));
		if( list.attempts > 0 &&
			(list.results < list.attempts
			 || (list.list[list.results-1].status.vasresult != SVAS_OK) ) 
		 ) {
			// status is in list.list[result].status
			// oid is in list.list[result].oid
			// enum BatchedRequest is list.list[result].req
			// return return_error(RC(get_vas_errcode()));

			return return_error(RC(list.list[list.results-1].status.vasreason));
			// if you believe it..
		}
	}
	return RCOK;
}

w_rc_t 
ObjCache::writeback_mmgr(bool reset_locks)
// write back any dirty/new objects in the mmgr cache space.
// This differs from the other flush variants in that it
// clusters writes by page.
{
	int i;
	CacheBHdr *ppt = (CacheBHdr *)mmgr.space;
	for (i=0, ppt = (CacheBHdr *)mmgr.space;
	   i < mmgr.num_blocks; 
	   ++i, ppt= mmgr.get_next(ppt))
	{
		CacheBHdr *cpt;
		for (cpt = ppt; 
			cpt; 
			cpt = cpt->page_link)
		{
			OTEntry *ote = cpt->otentry;
			if(ote)
			{
				if ((ote->is_new() || ote->is_dirty()) && !aborting)
					W_DO(writeback(ote));
				if (reset_locks)
				{
					if (ote->obj != 0)
						ote->type->__apply(DeAllocate,ote->obj);
					ote->obj = 0;
				}

				// could do mmgr.deallocate(cpt);
				// but we are resetting the cache anyway.
			}
			if (cpt->page_link==ppt) break; // we've gone through the list.
		}
	}
	if (reset_locks)
		mmgr.reset();
	return RCOK;
}

// Writes back all objects to the server,
// resets locks according to argument,
// and frees space for all the objects.
w_rc_t
ObjCache::flush(bool reset_locks)
{
	OTChunk *otc;
	int nentries, i;
	caddr_t p;

	if (do_batch && !inside_server)
		VAS_DO(begin_batch());

	mmgr.audit();
	W_DO(writeback_mmgr(reset_locks));


	// even though we've written everything out of the mem. mgr,
	// we need to walk the ote entries anyway to reset locks and
	// catch anything externally allocated.
	for(otc = first; otc != 0; otc = otc->next){

		// figure out how many entries are in the chunk
		if(otc == current)
			nentries = next_free;
		else
			nentries = OT_CHUNK_SIZE;

		// for each object...
		for(i = 0; i < nentries; ++i){

			OTEntry *ote = &otc->entries[i];

			// if the object is new but doesn't have a serial, then
			// assign it one
			if(ote->is_new() && ote->obj && !ote->has_serial())
				W_DO(get_volref(ote));

			// if this is a secondary ote, skip it
			if(ote->is_secondary())
				continue;

			if (ote->obj)
				dassert(get_brec(ote->obj)->otentry==ote);

			// if it's new or dirty, write it back to the vas
			if((ote->is_new() || ote->is_dirty()) && !aborting)
				W_DO(writeback(ote));

			// remove the object from the cache
			if(ote->obj != 0){
				CacheBHdr *cpt = get_brec(ote->obj);
				if (cpt->otentry != ote)
					fprintf(stderr,"ote %x obj %x bad backptr %x",ote,ote->obj,
						cpt->otentry);
				if (cpt->page_link)
					cpt->clear_pagelist();
				W_DO(free_mem(ote));
			}

			// reset the lock state
			if(reset_locks)
				ote->lockmode = NO_LOCK_MODE;
			// finally, call sdl level to updata module refcounts.
			
		}
	}

	if (do_batch && !inside_server) {
		W_DO(end_batch());
	}

	// callback to sdl for module updates...
	if(do_refcount) {
		if (aborting) {
			W_DO(prepare_to_abort());
		} else {
			W_DO(prepare_to_commit(do_refcount));
		}
	}

	return return_error(RCOK);
}

w_rc_t
ObjCache::flush(OTEntry *ote, bool reclaim)
{
	// allocate a serial to the object
	if(!ote->has_serial())
		W_DO(get_volref(ote));

	// write the object back to the server (this catches destroyed
	// objects) o
	W_DO(writeback(ote));

	// remove the object from the cache
	if(reclaim && ote->obj != 0){
		W_DO(free_mem(ote));
	}

	// NB: Don't reset the lock state - we still hold locks on the
	// object.

	return return_error(RCOK);
}

w_rc_t
ObjCache::get_type_loid(OTEntry *ote, const rType * type, LOID &t_oid)
// this routine gets the correct (volume relative) oid for the type
// of the object that ote refers to.  The trick here is that the
// rType structure alway contains a primary oid; if the volume
// the object resides on differs from the volume of the type
// object, we must get a correct secondary loid for the object't
// volume for the type object.
{
	assert(ote->is_primary()); // always called from primary ote.
	OTEntry *prim_ote; // primary ote for the type object.
	OTEntry *rel_ote; // ote for type object relative to volume of ote.

	W_DO(get_ote(type->loid, prim_ote));
	W_DO(get_matching(prim_ote,ote->volume,rel_ote));
	W_DO(get_loid(rel_ote,t_oid));
	return RCOK; // if we got here.
}



// NB: this function calls "prepare for disk," which unswizzles
// pointers and swaps bytes as appropriate.   The object may remain
// cached after that, until the space is reused.  If the object
// is accessed again before the object's cache memory is reused
// for other objects, the space needs to be "reclaimed" and the
// object converted back from going-to-disk format to memory format.
// There is a flag, accessible by the is_unswizzled() method on the
// ote, which will tell if the object is in memory but in disk format.

w_rc_t
ObjCache::writeback(OTEntry *ote)
{
	vec_t core, heap;
	LOID loid;
	if (ote->flags & OF_Transient) // don't do anything.
		return RCOK;

	// mmgr.audit();
	if(aborting) {
		return RC(SH_Internal);
	}

	if(ote == 0)
		return return_error(RC(SH_BadObject));

	// if the object is neither new nor dirty, then we're done
	if(!(ote->is_new() || ote->is_dirty()))
		return return_error(RCOK);
	// if the object is new, but not present, then it was deleted
	// before commit; it is ok to ignore it here.
	if(ote->is_new() && !ote->obj)
		return return_error(RCOK);

	// if the object is marked dirty but not in the cache, then we
	// must have screwed up
	if(ote->is_dirty() && ote->obj == 0 )
		return return_error(RC(SH_Internal));

	// make sure obj has a serial # allocated.
	if(ote->is_new() && ote->obj && !ote->has_serial())
		W_DO(get_volref(ote));

	// get the object's loid if it has one
	W_DO(simple_get_loid(ote, loid));

	// if loid is null, make sure to use null serial_t
	// obsolete. We need a serial # before call.
	//if(ote->volref.is_null())
	//	loid.id.serial.set(serial_t::null);

	// If this is a new object, create it.  NB: we only delay the
	// creation of anonymous objects; this can't be a registered
	// object.
	if(ote->is_new()){

		LOID type_loid;

		// FIX for beta
		if(ote->type->loid.volid() == VolId::null){
			// um, I think this is still done for res. serials.
			type_loid.set(vt[ote->pool->volume],
						  ote->type->loid.volref());
		}
		else // in order to check if 
		{
			W_DO(get_type_loid(ote,ote->type,type_loid));
		}
		// get the LOID of the object's pool
		LOID pool_loid;
		W_DO(get_loid(ote->pool, pool_loid));

		// set up the core vector
		// but ignore core for unix file
		if (type_loid.id.serial!= ReservedSerial::_UnixFile)
				core.put((void *)ote->obj, ote->type->size);

		smsize_t tlen = 0;
		smsize_t hlen = 0;

		W_DO(prepare_for_disk(ote, &heap, hlen, tlen));

		// try another null ?
		// create the object using the serial number we assigned earlier
		VAS_DO(vas->mkAnonymous(pool_loid.id,
								type_loid.id,
								core,
								heap,
								(ObjectOffset)ote->type->size+hlen-tlen,
								ote->type->nindices,
								loid.id));
		if (do_logcr)
		{
			fwrite(&ote->volref.guts._low, 4,1,cf);
		}

		// if we didn't have a serial number before...
		if(ote->volref.is_null()){

			// record its new volref and insert it into the loid table
			ote->volref.set(loid.volref());
			W_DO(lt.add(ote));
		}

		// flag the object as no longer new
		ote->flags &= ~OF_New;
	}

	// if this is an old object that has been modified, write it back
	else{

		core.put(ote->obj, ote->type->size);
		// heap implementation
		smsize_t hlen,tlen;

		/* Unused?
		// here comes a hack to avoid logging heap space twice
		// on initialization.  If the initial size of the object
		// (as show in the brec) is the same as the core size for
		// the type, write the object, then append the heap, instead
		// of doing trunc and write.  2nd hack is to not do trunc
		// if the size of the object hasn't changed.
		BehindRec * brec = (BehindRec *)(ote->obj-sizeof(BehindRec));
		*/
		// now, put the heap stuff into a separate vec.
		W_DO(prepare_for_disk(ote, &heap, hlen, tlen));

		if (hlen>0) // append heap to the core vector.
				core.put(heap);
		// size has changed, so do trunc, then write. We may
		// we used to try and calculate if writeObj could be called,
		// now, don't bother anymore.
		// updateObj is merged version of trunc+write; not clear
		// if writeObj is any more efficient anyway.
		VAS_DO(vas->updateObj(loid.id, 		
				0, core,
				ote->type->size + hlen,
				ote->type->size + hlen - tlen));
	}

	// mark the object no longer dirty
	ote->flags &= ~OF_Dirty;

	if (do_logops)
		log_opr(ote,"writeback");

	// mmgr.audit();
	return return_error(RCOK);
}

w_rc_t
ObjCache::update_pool_timestamps(struct timeval *tvp)
{
	OTEntry *pool, *next;
	LOID pool_loid;

	// for each pool on the pool list...
	for(pool = pool_list; pool != 0; pool = next){

		next = pool->next;

		// get the pool's loid
		W_DO(get_loid(pool, pool_loid));

		// set both the atime and the mtime to the given timeval
		VAS_DO(vas->utimes(pool_loid.id, tvp, tvp));

		// reset the pool ote
		pool->flags &= ~OF_OnPoolList;
		pool->next = 0;
	}

	pool_list = 0;

	return return_error(RCOK);
}

void
ObjCache::clear_pool_list()
{
	OTEntry *pool, *next;

	for(pool = pool_list; pool != 0; pool = next){
		next = pool->next;
		pool->flags &= ~OF_OnPoolList;
		pool->next = 0;
	}

	pool_list = 0;
}

w_rc_t
ObjCache::writeback_all()
// flush: write back all objects to server
// don't reclaim memory or do anything fancy
// this is for transaction chaining.
// in this case , we hold locks and don't free any memory.
{
	OTChunk *otc, *next;
	// as with flush, do writeback_mmgr first; this will keep page writes
	// clustered...
	W_DO(writeback_mmgr(false));
	// but, as before, we still need to scan ot entries...
	// in case there is anything externally allocated 
	// (actually this could be determined from mmgr stats...)
	for(otc = first; otc != 0; otc = next){
		int nentries;

		if(otc == current)
			nentries = next_free;
		else
			nentries = OT_CHUNK_SIZE;

		for(int i = 0; i < nentries; ++i){
			OTEntry *ote = &otc->entries[i];
			// use writeback instead of flush.
			if((ote->is_new() || ote->is_dirty()))
					W_DO(writeback(ote));
			//W_DO(flush(ote, false)); // don't reclaim space
		}
		next = otc->next;
	}

	// fix up transaction stuff
	reset_xact(true); // chained
	return RCOK;
}

void
ObjCache::reset_xact(bool chained)
{
	if(chained) {
		curr_xact_num = next_xact_num++;
	} else {
		curr_xact_num = 0;
	}
	// free all blocks on xact_alloc_list
	{
		XactAllocListNode *n, *next;

		for(n = xact_alloc_list; n != 0; n = next){
			next = n->next;
			delete [] n->p;
			delete n;
		}
		xact_alloc_list = 0;
	}
}

void
ObjCache::reset(bool invalidate, bool flush, bool reset_locks)
{

	int i;

	if(flush || (!invalidate && reset_locks)){

		OTChunk *otc, *next;

		// free up all the space used by the objects
		for(otc = first; otc != 0; otc = next){

			next = otc->next;

			int nentries;
			BehindRec *brec;

			if(otc == current)
				nentries = next_free;
			else
				nentries = OT_CHUNK_SIZE;

			for(i = 0; i < nentries; ++i){

				OTEntry *ote = &otc->entries[i];
				if(flush && ote->obj != 0){
					W_IGNORE(free_mem(ote));
				}
				if (aborting || ote->is_new()) 
				// if aborting, always clear dirty & new; ote can't be reused??
				// if new flag is set at this point, we need to clear
				// it or we will commit some bogosity in the future.
				// in general, this means that an object was allocated
				// but the tranaction is aborting.
				{
					ote->flags &= ~(OF_New | OF_Dirty);
				}

				if(reset_locks)
					ote->lockmode = NO_LOCK_MODE;
			}
		}

		// Do we really want to do this?  At the moment, it appears to make
		// bad things happen with memory allocation, but in general, is
		// this a good idea?

		// FIX: check this with Purify.
		// if(invalidate)
		//	delete otc;
	}

	if(invalidate){

		// object table bookkeeping
		first = 0;
		current = 0;
		next_free = 0;

		// mem manager
		mmgr.reset();

		// loid table
		lt.reset();

		// volume table
		vt.reset();
	}

	// This should already be 0, because we should (at the very
	// least) have called clear_pool_list.
	pool_list = 0;

	// preallocated serials
	// FIX: can serials preallocated in one transaction be used
	// by another?  We assume not until further notice.
	if (serials != 0) // reset is called in destructor ...
		for(i = 0; i < serial_array_size; ++i)
			serials[i].nserials = 0;

	reset_xact();
}

////////////////////////////////////////////////////////////////
//
//		    Error Handling Functions
//
////////////////////////////////////////////////////////////////

w_rc_t
ObjCache::fix_rc(w_rc_t rc)
{
	if(rc.err_num() == eBADVOL ||
		rc.err_num() == fcNOTFOUND ||
		rc.err_num() == eBADLOGICALID ||
		rc.err_num() == SH_NotFound ||
		rc.err_num() == SVAS_NotFound ||
		rc.err_num() == SVAS_BadSerial)
		RC_PUSH(rc, SH_BadObject);

	return rc;
}

int 
ObjCache::get_vas_errcode()
{
    dassert(vas!=0);

    if(vas->status.vasresult == SVAS_OK)
	    return SVAS_OK;

    if(vas->status.vasreason == SVAS_SmFailure){
	    if(vas->status.smreason == fcOS)
		    return vas->status.osreason;
	    return vas->status.smreason;
    }
    return vas->status.vasreason;
}


w_rc_t 
ObjCache::last_error()
{
    return RC(_last_error_num); 
}

w_rc_t
ObjCache::mkdir(const char *path, mode_t mode)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	LOID result;
	VAS_DO(vas->mkDir(path, mode, &result.id));
	return return_error(RCOK);
}

w_rc_t
ObjCache::rmdir(const char *path)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	VAS_DO(vas->rmDir(path));
	return return_error(RCOK);
}

w_rc_t
ObjCache::rmpool(const char *path)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	VAS_DO(vas->rmPool(path));
	return return_error(RCOK);
}

w_rc_t
ObjCache::readlink(const char *path,
					char *buf,
					int bufsize,
					int *resultlen)				// OUT resultlen
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	vec_t vec;
	ObjectSize size = 0;

	// NB: Different systems disagree on whether the resulting string
	// gets null-terminated.  The VAS never null-terminates.  We
	// null-terminate only if *resultlen < bufsize.  This seems the
	// most reasonable alternative (to me at least).
	vec.put(buf, bufsize);
	VAS_DO(vas->readLink(path, vec, &size));
	*resultlen = (int)size;
	if(size < bufsize)
		buf[size] = '\0';
	return return_error(RCOK);
}

w_rc_t
ObjCache::readxref(const char *path, OTEntry *&ote)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	LOID loid;

	VAS_DO(vas->readRef(path, &loid.id));
	W_DO(get_ote(loid, ote));
	return return_error(RCOK);
}

w_rc_t
ObjCache::utimes(const char *path, struct timeval *tvp)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	struct timeval *tvpa, *tvpm;

	if(tvp == 0){
		tvpa = 0;
		tvpm = 0;
	}
	else{
		tvpa = tvp;
		tvpm = tvp + 1;
	}

	VAS_DO(vas->utimes(path, tvpa, tvpm));
	return return_error(RCOK);
}

w_rc_t
ObjCache::chmod(const char *path, mode_t newmode)
{
	OTEntry *ote;
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	W_DO(lookup(path, ote));
	if(ote) {
		W_DO(flush(ote)); // write back and free space
		VAS_DO(vas->chMod(path, newmode));
	}
	return return_error(RCOK);
}

w_rc_t
ObjCache::chown(const char *path, uid_t owner, gid_t group)
{
	OTEntry *ote;
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	if(owner == (uid_t) -1 && group == (uid_t) -1) {
		return return_error(RCOK);
	}
	W_DO(lookup(path, ote));
	if(ote) {
		W_DO(flush(ote)); // write back and free space
		if( owner != (uid_t) -1) {
			VAS_DO(vas->chOwn(path, owner));
		} 
		if( group != (gid_t) -1) {
			VAS_DO(vas->chGrp(path, group));
		}
	}
	return return_error(RCOK);
}

void 
ObjCache::set_mem_limit(int x) 
{
	mmgr.set_mem_limit(x);
}
int 
ObjCache::oc_unix_error(w_rc_t rc) 
{
	if(rc) {
		int e = rc.err_num();
		if( e == w_error_t::fcOS) {
			e = rc.sys_err_num();
		}
		if(e >= OS_ERRMIN && e <= OS_ERRMAX) return e;
		if(e >= SHERRMIN && e <= SHERRMAX) return e;
		if( e >= SVAS_ERRMIN && e <= SVAS_ERRMAX) return oc_unix_error(e);

		// convert all others to unix errnos
		return svas_base::unix_error(rc);
	} else return 0;
}

int 
ObjCache::oc_unix_error(int e) 
{
	if( e >= w_error_t::fcERRMIN && e <= w_error_t::fcERRMAX) switch(e) {
		case fcINTERNAL:
			e = SH_Internal;
			break;

		case fcOUTOFMEMORY:
			e = SH_OutOfMemory;
			break;
		case fcFOUND:
			e = SH_Already;
			break;

		case fcNOTFOUND:
			e = SH_NotFound;
			break;

		case fcREADONLY:
			e = SH_ReadOnly;
			break;

		case fcNOTIMPLEMENTED:
		case fcMIXED:
		case fcNOSUCHERROR:
		case fcFULL:
		case fcOS:
		case fcEMPTY:
		default:
			break;
	}
    if(e >= OS_ERRMIN && e <= OS_ERRMAX) return e;
    if(e >= SHERRMIN && e <= SHERRMAX) return e;

    if( e >= SVAS_ERRMIN && e <= SVAS_ERRMAX)  switch(e) {

	    case SVAS_NotFound:
		    return SH_NotFound;

	    case SVAS_ABORTED:
		    return SH_TxAborted;

	    case SVAS_TxRequired:
		    return SH_TxRequired;

	    case SVAS_TxNotAllowed:
		    return SH_TxNotAllowed;

	    case SVAS_NoIndex:
		    return SH_NoIndex;

	    case SVAS_NoSuchIndex:
		    return SH_NoSuchIndex;

	    case eDUPLICATE:
		    return SH_DuplicateEntry;

	    case SVAS_Already:
		    return SH_Already;

	    default:
			return SH_ROFailure;
		    break;
    }
    // convert all others to unix errnos
    return svas_base::unix_error(e);
}

int 
ObjCache::unix_error(int e) 
{
    return svas_base::unix_error(e);
}

int 
ObjCache::unix_error(w_rc_t rc)
{
    if(rc) return svas_base::unix_error(rc);
    else return 0;
}

w_rc_t 
ObjCache::access(const char *path, int mode, int &error)
{
	bool		ok;

	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	if(vas->access(path, (AccessMode)mode, &ok) != SVAS_OK) {
		error = get_vas_errcode();
	} else if(ok) {
		error = 0;
	} else {
		error = OS_PermissionDenied;
	}
	error = unix_error(error);
	// it's *never* an error to use access
	return RCOK;
}

w_rc_t
ObjCache::rename(const char *oldpath, const char *newpath)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	VAS_DO(vas->reName(oldpath, newpath));
	return return_error(RCOK);
}

#define DEF_MODE	0777

w_rc_t
ObjCache::symlink(const char *contents, const char *linkname)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	LOID loid;

	// NB: the parameters for mkSymlink are backwards wrt symlink(2).
	// {ObjCache,Shore}::symlink match symlink(2).
	VAS_DO(vas->mkSymlink(linkname, contents, DEF_MODE, &loid.id));
	return return_error(RCOK);
}

w_rc_t
ObjCache::umask(mode_t newmask, mode_t *oldmask)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	if(oldmask){
		unsigned int tmp;
		VAS_DO(vas->getUmask(&tmp));
		*oldmask = (mode_t)tmp;
	}
	VAS_DO(vas->setUmask((unsigned int)newmask));
	return return_error(RCOK);
}

w_rc_t
ObjCache::getcwd(char *buf, int buflen)
{
	if(vas==0) { return return_error(RC(SH_NotInitialized)); }
	else {
		char *p;
		vas->status.vasresult = 0;
		p = vas->gwd(buf, buflen);
		VAS_DO(vas->status.vasresult);
		return return_error(RCOK);
	}
}


void 
MemMgr::uncache(CacheBHdr * un_cpt, bool force)
{
	CacheBHdr * cpt = un_cpt;
	// audit();
	if (!cpt->can_free())
			fprintf(stderr,"bad uncache attempt addr %x\n",cpt);
	// all we need to do is mark it free; coalescing will
	// be done by allocation routines as needed.
	// and some statistics.
	// But: if there is a page list, walk through it, writing
	// back everything on the page. If an object on the
	// page list can be freed, do so.
	// 
	// force flag: if force is true, uncache un_cpt regardless
	// of page clustering considerations.
	// otherwise (for now) hold on to pages untill all refs
	// are cleared.
	// alternate strategy would be to delete only un_cpt.
	// The way page cache clustering currently works is that
	// oops, can_free_page is unfortunate almost always too expensif
	// to call.  attempt # 18: free the page
	//if (!force && un_cpt->page_link && !un_cpt->can_free_page())
	//	return;
	// new strategy: relink the page list with still-cached objs.
	// while walking through it.
	// dirty strategies: write dirty if force; else defer freeing
	// dirty pages until forced??
	//strategy wise this is way too complex.
	CacheBHdr * new_list = 0;
	CacheBHdr *npt = 0;
	CacheBHdr *first_cpt = 0;
	for(cpt= un_cpt; cpt ; cpt = npt)
	{
		OTEntry * ote = cpt->otentry;
	 	npt =cpt->page_link; 
		cpt->page_link = 0; // we are rebuilding it.
		// if it's new or dirty, write it back to the vas
		// if we can free this and it's not already free..
		if (cpt->can_free() && !cpt->is_free())
		{
			if (do_logops)
				log_opr(ote,"uncache");
			if(ote->is_new() || ote->is_dirty()) {
				if(!myoc->ObjCache::aborting) {
					if (myoc->ObjCache::do_batch && !myoc->ObjCache::batch_active)
					{	SH_DO(myoc->ObjCache::begin_batch());	}
					SH_DO(myoc->ObjCache::writeback(ote));
				}
			}
			deallocate(cpt);
		}
		else if (myoc->ObjCache::do_pagecluster)
		// alway relink anything without can_free
		{
			if (!first_cpt) first_cpt = cpt;
			first_cpt->add_to_pagelist(cpt);
		}
		if (npt == un_cpt) break;
	}
}


//
//  STATISTICS
//

#include "OCstats_op.i"
const char    *OCstats::stat_names[] = {
#include "OCstats_msg.i"
};


//
// ERROR HANDLING
//

w_rc_t 
ObjCache::return_error(w_rc_t rc) {
	if(rc) { 
		ObjCache::_last_error_num = rc.err_num();
		int	e  = oc_unix_error(rc);
		if(rc.err_num() != e && rc.sys_err_num() !=e) {
			RC_PUSH(rc, e);
		}

		/* suppress Error not checked*/
		// (void) ObjCache::_last_error.is_error(); 
	}
	return rc;
}

ObjCache::~ObjCache()
// clean up old space only.
{
	OTChunk *otc, *next;
	reset(true,true,true); // free any object memory

	for(otc = first; otc != 0; otc = next){
		next = otc->next;
		delete otc;
	}

	if(serials)
		delete serials;
	serial_array_size = 0;
}
