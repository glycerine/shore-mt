/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/sdl/sdl_support.C,v 1.71 1997/06/13 21:58:34 solomon Exp $
 */

#define OBJECT_CACHE 1
#include "ShoreApp.h"
#include "reserved_oids.h"
#include "vec_t.h"
#include "sdl_UnixFile.h"
#include "sdl_internal.h"
#include "debug.h"
#include "PoolScan.h"
#include "Any.h"
#include <string.h>
#ifndef roundup
// roundup may or may not have been defined, according to include dependencies.
#define         roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif


// global runtime state pointer, automaticly maintained by sdl_rt_state
// defined below.
// no longer static.
struct sdl_rt_state *rt_pt;

// the following struct encapsulates the global state used by the per-object
// sdl runtime routines.  That is, these variables are things that refer
// to a particular shore object as it is being moved between disk and
// cache.
struct sdl_rt_state {
	char * obj_hpt; // pointer to current heap position.
	char * text_hpt;// pointer to text field, if there is one.
	size_t text_length;// text length, if it is set.
	int cur_vindex; // volume index for set in prepare_ ... routines
	OCRef cur_OCRef; // also set in prepare, needed for indicess.
	int cur_mindex_count;   // increment in sdl_index_base::ref_apply.
	sdl_rt_state *old_rt_pt;
	vec_t *obj_outvec;
	size_t tot_hlen;
	sdl_rt_state() // null out everything.
	{	obj_hpt = 0; text_hpt = 0; text_length = 0;
		cur_vindex = 0; cur_OCRef.assign(0); 
		cur_mindex_count = 0;
		// a little dubious: always save the old rt_pt,
		// and reset it with this. This automaticly
		// changes the global state whenever an sdl_rt_state
		// variable comes into scope.
		old_rt_pt = rt_pt;
		rt_pt = this;
		obj_outvec = 0;
		tot_hlen = 0;
	}
	~sdl_rt_state() // reset the rt_pt pointer.
	// this is a bit too clever.
	{	rt_pt = old_rt_pt; }
};

int
push_sdlrt(vec_t * vpt,int vin) { 
	
	sdl_rt_state * new_st = new sdl_rt_state;
	w_assert1(new_st == rt_pt);
	rt_pt->cur_vindex = vin;
	rt_pt->obj_outvec = vpt;
	return 0;
};
int
pop_sdlrt() { delete rt_pt; rt_pt  = 0; return 0;};

// yet another (trivial) hash table implementation
const int TYPE_HSIZE = 1024;
static rType * T_Htab[TYPE_HSIZE];
int TOID_hash(LOID  i)
{
	// return i % TYPE_HSIZE;
	int toid = i.id.serial.guts._low;
	return toid % TYPE_HSIZE;

} 
// simple linked list of all rModules encountered.
static rModule * mod_list = 0;

// the following routine is called from the constructor of a type object
//void sdl_InsertTypeObj(LOID oid, rType *tpt)
void sdl_InsertTypeObj(rType *tpt)
{
	int hindex = TOID_hash(tpt->loid);
	rType * chain = T_Htab[hindex];
	T_Htab[hindex] = tpt;

	// for now, just use a null volid; later on, we will use a real
	// honest-to-goodness loid
	//VolRef volref;
	//volref.guts._high = 0;
	//volref.guts._low = oid;
	//tpt->loid.set(VolId::null, volref);
	//if (oid==0) return; // messed up here, may be unix file.


	tpt->next = chain;
	// now, do a duplicate check.
	for( ; chain; chain = chain->next)
	if (chain->loid.id.serial == tpt->loid.id.serial){
	    fprintf(stderr,"type hash collision oid %d p1 %x:%s p2 %x:%s\n",
		    tpt->loid.id.serial.guts._low,tpt,tpt->name,chain,chain->name);
	    abort();
	}
}

extern rType *
lookup_type(LOID oid);

rModule *
get_rmod(Ref<sdlDeclaration> tdecl)
// look for a n rmodule object corresponding to the given
// tdecl; if not found, make one up, if possible.
{
	Ref<sdlModule> mref;
	mref = TYPE_OBJECT(sdlModule).isa(tdecl->scope);
	if (mref != 0) // we found the type obj, it is ok.
	{
		for (rModule *mlist = mod_list; mlist != 0; mlist = mlist->next)
		{
			if (mref.equal(mlist->loid))
				return mlist;
		}
		// need to cons up a new one.
		return new rModule(mref->name,mref);
	}
}

rType *
get_type_object(LOID &oid);
rType *
get_type_object(LOID &oid)
// look up type oid for compiled in support; fault in type
// infor for database if compiled in support is not present.
// type faulting routine for accessing objects that don't have
// compiled in language binding support. Try to get the type
// object from the data base and fill in the rType thing
// as best we can.
// look up type object with given oid.
{
	rType * tobj = 0; // logical oid of the type object's type.
	LOID toid;
	Ref<InterfaceType> tref;
	tref.assign(oid); // set with the given type oid.
	// before doing the hash lookup, make sure we have a primary loid.
	if (oid.is_remote()) // we need to get a local reference...
	{
		W_COERCE(tref.snap());
		W_COERCE(tref.get_loid(oid));
	}
	int hindex = TOID_hash(oid);
	rType * chain = T_Htab[hindex];

	for(; chain; chain = chain->next)
		// if (chain->loid.id.serial.guts._low == oid)
		// temporary hack: compare only serial # of type oids for hashing.
		if (chain->loid.id.serial == oid.id.serial)
			return chain;

	// we need to check the type of the tref; otherwise
	// we can get recursion if metatype language binding
	// is not consistent. 
	// first, check if the type oid is a valid oid; if it's not,
	// we still enter it in the table but we flag the entry saying
	// we have no info on the type.
	if (tref.valid()  )
	// make a dummy entry
	{
		tobj = new rType(0,"unknown_type",0);
		tobj->loid = oid;
	}
	else if (tref.get_type(toid) || TYPE_OBJECT(sdlInterfaceType).loid != toid)
	// the type of the type object is incorrect.
	{
		tobj = new rType(0,"unknown_type_type",0);
		tobj->loid = oid;
		fprintf(stderr,"Type object has bad type oid.(bad type type)\n");
	}
	// ignore inheritance and indices; just set the
	// name if we can.
	else // we found the type obj and its type is correct.
	{
		int stlen = tref->name.strlen();
		char *name_str = new char[stlen+1];
		strcpy(name_str,(char *)(tref->name));
		tobj = new rType(0,name_str,0);
		tobj->size = tref->size;
		tobj->nindices = tref->numIndexes;
		tobj->loid = oid;
		tobj->next = 0;
		tobj->mod = get_rmod(tref->myDecl);
	}
	if (tobj)
	{
		tobj->next = T_Htab[hindex];
		T_Htab[hindex] = tobj;
		return tobj;
	}
	return 0; // couldn't do anything..
}


void
OCRef::__apply(HeapOps op)
{
	switch(op) {
	case PrepareForMem:
	case ReSwizzle:
		W_COERCE(swizzle(rt_pt->cur_vindex));
	break;
	case PrepareForDisk:
		W_COERCE(unswizzle(rt_pt->cur_vindex));
	break;
	}
}
	
void set_sdl_heappt(void * newheap)
{
	rt_pt->obj_hpt = (char *)newheap;
}

void set_sdl_outvec(vec_t * vpt)
{
	rt_pt->obj_outvec = vpt;
	rt_pt->tot_hlen = 0;

}

int log_pins = 0;
rType * sdl_insert_defered = 0;
// this routine is called during initialization, to do anything
// we need to do. For now, there is just one hack to fix up
// UnixFile ctor problem.
void sdl_init()
{
	TYPE_PT(SdlUnixFile)->size = 0; // no core.
	if (getenv("DO_PINLOG"))
		log_pins = 1;
	if (sdl_insert_defered)
	{
		rType * rpt = sdl_insert_defered;
		while(rpt)
		{
			rType * nextpt = rpt->next;
			rpt->set_toid(); // try again.
			sdl_InsertTypeObj(rpt);
			rpt = nextpt;
		}
	}
	sdl_insert_defered = 0; // so we can run again in server context.

}
	
// this function does the language-specific stuff to make an object
// usable in memory.  obj is a pointer to the memory containing the
// disk impage of the object.  Currently, we call a constructor to
// initialize vtables and call the apply routine on the type object
// to 

static OTEntry * c_ote = 0;
void ote_check()
{	
	fprintf(stderr, "found ote %x\n",c_ote);
}

shrc
prepare_for_application(LOID &type_oid, OTEntry *ote,
			StringRec &srec, smsize_t tlen)
{
	sdl_rt_state cur_obj_state;
	// first, get the compiled-in type object from the LOID

	int toid = type_oid.id.serial.guts._low;
	if (c_ote && ote == c_ote)
		ote_check();
	// rType * tpt = sdl_LookUpTypeObj(toid);
	rType * tpt = get_type_object(type_oid);
	rt_pt->cur_vindex = ote->volume;
	// rt_pt->cur_OCRef.assign(ote->obj); 
	// how could this ever work?? brec otentry not necessarily set here...
	// rt_pt->cur_OCRef.assign(ote);
	rt_pt->cur_OCRef.u.otentry = ote;
	rt_pt->cur_mindex_count = 0;
	tpt->setup_vtable(ote->obj);
	// now the convoluted routine to swizzle the object.
	// we need to set the global context needed for swizzling
	// tpt->ref_apply((pfct)swizzleref,obj);
	// tpt->set_string(obj, srec);
	//  set the incoming heap from srec.
#ifndef nocontig
	// adjust the heap pointer for padding which may have been inserted.
	srec.string += tpt->size%sizeof(double);
	srec.length -= tpt->size%sizeof(double);
#endif
	set_sdl_heappt(srec.string);
	rt_pt->text_length = tlen;
	if (tlen != 0 ) // set text_hpt from end of heap.
	{
		rt_pt->text_hpt = srec.string + (srec.length - tlen);
		rt_pt->text_length = tlen;
	}
	else
		rt_pt->text_hpt = 0;

	tpt->__apply(PrepareForMem,ote->obj);
	ote->type = tpt;
	ote->flags &= ~OF_UnSwizzled;
	return RCOK;
}

shrc
reclaim_object(OTEntry *ote)
{
	sdl_rt_state cur_obj_state;
	if (c_ote && ote == c_ote)
		ote_check();
	if (ote->is_unswizzled())
	{
		rt_pt->cur_vindex = ote->volume;
		rt_pt->cur_OCRef.assign(ote->obj); // get an otentry by working packwards.
		// all we need do here is call the appropriate apply...
		ote->type->__apply(ReSwizzle,ote->obj);
		ote->flags &= ~OF_UnSwizzled;
	}
	return RCOK;
}


shrc
prepare_for_disk(OTEntry * ote,
		 vec_t *vpt, smsize_t &hlen, smsize_t &tlen)
{
	sdl_rt_state cur_obj_state;
	rt_pt->text_length = 0; // initially, for each object;
	rt_pt->cur_vindex = ote->volume;
	rt_pt->cur_OCRef.assign(ote->obj); // get an otentry by working packwards.
	rt_pt->cur_mindex_count = 0;
	if (c_ote && ote == c_ote)
		ote_check();
	set_sdl_outvec(vpt);
	// pad the heap vector if necessary.
	int padlen = ote->type->size % sizeof(double);
	if (padlen)
		vpt->put(ote->obj+ote->type->size,ote->type->size%sizeof(double));
	ote->type->__apply(PrepareForDisk,ote->obj);
	hlen= rt_pt->tot_hlen + rt_pt->text_length;
	hlen += padlen;
	tlen = rt_pt->text_length;
	// put text at end if there is any;
	if (rt_pt->text_length != 0)
		vpt->put(rt_pt->text_hpt,rt_pt->text_length);
	else if (rt_pt->tot_hlen==0)
		vpt->put(0,0);
	// set unswizzled flag.
	ote->flags |= OF_UnSwizzled;
	return RCOK;
}


size_t
compute_heapsize(OTEntry *ote,size_t *tlen)
{
	size_t hlen = 0;
	sdl_rt_state cur_obj_state;
	ote->type->__apply(ComputeHeapSize,ote->obj);
	hlen= rt_pt->tot_hlen + rt_pt->text_length;
	if (tlen)
		*tlen = rt_pt->text_length;
	return hlen;
}
	


// Use OCRef::init(OTEntry *) to get an OCRef from the given ote.
// Then call OCRef::get_type to get the object's type.
shrc
prepare_to_destroy(OTEntry *ote)
{
	// this object is about to be deleted; decrement the type
	// reference count and free any non-contiguous heap.
	if (ote->type==0) // get the type.
	{
		OCRef oref;
		oref.init(ote);
		rType * ntype;
		W_DO(oref.get_type(ntype));
		ote->type= ntype;
	}
	if (ote->type)
		ote->type->inc_refcount(-1);
	// used to do ote->type->__apply(DeAllocate,ote->obj) here, but
	// it is always done in remove_destroyed so never mind.
	return RCOK;
}



// heap  in/out routines: currently we don't bother swizzling outbound
// heap pointers, and we compute inbound pointers according to length.

void
sdl_heap_op(HeapOps op, void **spt, size_t length, int free_it)
// generic routine: allocate or deallocate space on the heap, depending
// on op. spt points to the place where the heap pointer is within
// the current objece; lenght is the size of the heap element; free_it
// indicates if the memory chunk should be freed on a DeAllocate op.
// currently this is used for sdl_val_set only.
{
	switch(op) {
	case PrepareForMem:
	if (rt_pt->obj_hpt  && length>0)
	{
		*spt = (char *)rt_pt->obj_hpt;
		// obj_hpt += length;
		rt_pt->obj_hpt += roundup(length,sizeof(double));
		free_it = 0;
	}
	break;
	case ReSwizzle: // object has ben PreparedForDisk but is
	// still in memory.  Since we didn't rewrite the heap
	// space pt. on preparefordisk, we need do nothing
	// at this point??
	{
	}
	break;

	case PrepareForDisk:
	if (length > 0)
	{
		int len = roundup(length,sizeof(double));
		rt_pt->obj_outvec->put(*spt, len);
		rt_pt->tot_hlen += len;

	}
	break;
	case DeAllocate:
	// check if space should be freed
		if (free_it)
			delete [] *spt;
	break;
	case ComputeHeapSize:
		rt_pt->tot_hlen += roundup(length,sizeof(double));
	break;
	}
}

// the text __apply is analagous to the string version, but the
// there can only be one pointer.
// similary, text is for the most part directly eqivalent to sdl_heap_base,
// but  we need to set the text pt & len..
void
sdl_heap_base::text__apply(HeapOps op)
{
	switch(op) {
	case PrepareForMem:
	if (rt_pt->text_hpt  )
	{
		space = (rt_pt->text_hpt);
		cur_size = rt_pt->text_length + 1;
		rt_pt->text_hpt[rt_pt->text_length]=0;
		free_it = 0;
	}
	else
	{
		space = 0;
		cur_size = 0;
		free_it = 0;
	}
	break;
	case ReSwizzle: // object has ben PreparedForDisk but is
	// being reclaimed.
	// nothing to do at this point..
	break;
	case PrepareForDisk:
	case ComputeHeapSize:
	if (cur_size > 1)
	{
		// NB:  The text field is an sdl_string, so it ends with a null that is
		// not supposed to be counted in the length of the field.  To keep NFS
		// from being confused, we chop that null here.  If cur_size==1, the
		// field contains nothing but that null, so we treated it as if it were
		// zero-length.
		rt_pt->text_hpt = space;
		rt_pt->text_length = cur_size - 1;
	}
	break;
	case DeAllocate:
	// check if space should be freed
		if (free_it)
			delete [] space;
	break;
	}
}

// ref-based sdl_set is obosolete; now, use as a generic heap elt only..

void
sdl_set::__apply(HeapOps op)
{
		sdl_heap_base::__apply(op);
}

void
sdl_index_base::__apply(HeapOps op)
{
	//this is a bit odd, we use this as the place to set the
	// index_id value of the heap.
	switch(op) {
	case PrepareForMem:
	break;
	case PrepareForDisk:
	// set the oid portion of the class to be the object "this"
	// is contained in, e.g. the object of the current "prepare_for".
	// this is not really the right place to do this, but it's
	// a sufficient hack for now.
	// oops, we only want to do this once...
	if (index_id().obj != lid_t::null) // this is a new object.
	{
		LOID tmp;
		W_COERCE(rt_pt->cur_OCRef.simple_get_loid(tmp));
		// alternateley, use  
		//  Cur_OCRef.simple_get_loid((LOID &)(iind.index_id().obj))
		// may save a copy but breaks encapsulation
		index_id().obj = tmp.id;
		// also set the index here.
		index_id().i = rt_pt->cur_mindex_count;
		++(rt_pt->cur_mindex_count);
	}
	break;
	}
}

// for all heap-base elements, use sdl_heap_base for storage allocation.
// this replaces sdl_set, sdl_union_base, and is used as a base class
// for sdl_set<t>
void
sdl_heap_base::__apply(HeapOps op)
// do the space management part of unions.
{
	switch(op) {
	case PrepareForMem:
	if (rt_pt->obj_hpt  && cur_size>0)
	{
		space = rt_pt->obj_hpt;
		rt_pt->obj_hpt += roundup(cur_size,sizeof(double));
		free_it = 0;
	}
	else
	{
		free_it= 0;
		space = 0;
	}
	break;
	case ReSwizzle: // object has ben PreparedForDisk but is
	// being reclaimed.
	// nothing to do at this point..
	break;
	case ComputeHeapSize:
		rt_pt->tot_hlen += roundup(cur_size,sizeof(double));
	break;
	case PrepareForDisk:
	if (cur_size > 0)
	{
		size_t len = roundup(cur_size,sizeof(double));
		rt_pt->obj_outvec->put(space, len);
		rt_pt->tot_hlen += len;
		// ideally, we'd free the space now, but 
		// can't do that till after it's written.
	}
	break;
	case DeAllocate:
	// check if space should be freed
		if (free_it)
		{
			free_it = 0;
			delete  space;
			// space = 0;
		}
	break;
	}
}

// If this method gets put somewhere else, then we can remove
// #include "reserved_oids.h".
shrc
Ref<Pool>::lookup(const char *path, Ref<Pool> &ref)
{
	LOID type_loid;
	Ref<Pool> tmp;

	W_DO(OCRef::lookup(path, type_loid, tmp));
	if(type_loid.id != ReservedOid::_Pool)
		return RC(SH_NotAPool);
	ref = tmp;
	return RCOK;
}

shrc
Ref<Pool>::create(const char *path, mode_t mode, Ref<Pool> &ref)
{
	W_DO(OCRef::create_pool(path, mode, ref));
	return RCOK;
}

shrc
Ref<Pool>::destroy_contents()
// remove any existing objects.
{
	int i;
	Ref<any> ref;
	PoolScan scan(*this);
	W_DO (scan.rc())
	for(i = 0; !scan.next(ref, false) /* rc ok */  ; ++i)
	{
		W_DO(ref.destroy());
	}
	return RCOK;
}


shrc
Ref<any>::lookup(const char *path, Ref<any> &ref)
{
	W_DO(OCRef::lookup(path, ref));
	return RCOK;
}

void rType::__apply(HeapOps op, void * opt)
// generic version of __apply, which is normally generated by
// the language binding generator for defined types.
{
	// ok, interpret this as follows: get the oid of the type,
	// and toss off to it.
	REF(InterfaceType) tpt;
	if (metaobj_exists())
	// don't try to do this if type obj isn't valid...
	{
		tpt.assign(loid);
		tpt->sdl_apply(op,opt);
	}
}

// plus a bunch of obsolete fcts.
void rType::ref_apply(pfct, void *) {}
void rType::get_string(void *, StringRec &){}
void rType::set_string(void *, StringRec &){}
void rType::set_toid() { // should never be called... 
	abort();
}

void rType::setup_vtable(void * p)
// this routine normally just sets up the vptr for the object, but
// if we got here, the compiled in support for this type isn't present.
// for safety, we zero out the first word of the object, which would
// normally contain the virtual fct ptr. Then, any attempt to dispatch
// a function will fault and die miserably.  We may want to improve
// this at some  point by trying to patch in available fcts from
// base classes that are present, or by patching in a vtbl full of
// functions that will print some kind of appropriate error msg.
//
{
	char ** cpt = (char **)p;
	*cpt = 0;
}


void *rType::cast(void *p, rType *t) 
{
	if(t == this)
	return p;
	else
	return 0;
}

// reference count support for object creation...
void
rType::inc_refcount(int delta) const
{
	if (mod==0) // unknown type of some sort
		return;
	mod->refcount_delta += delta;
}

// constructor for module: always link every module into mod_list.
rModule::rModule(char *mn,uint4 vhi, uint4 vlow, uint4 s) 
		// :m_volid(vhi,vlow) // this doesnt work, m_serial(s,false)
	
{	
	VolRef m_serial;
	refcount_delta = 0; 
	next = mod_list; 
	mname = mn;
	m_serial.data._low = s; // get around bogus ctor.
	loid.set(VolId(vhi,vlow),m_serial);
	mod_list = this;
}

rModule::rModule(char *mn, const OCRef &r)
{
	refcount_delta = 0; 
	mname = strdup(mn);
	SH_DO(r.get_primary_loid(loid));
	next = mod_list;
	mod_list = this;
}
// clear any module refcount deltas set in rModule structs.
// this is called either after prepare_to_commit has sent its updates
// or on prepare_to_abort.
void
clear_mod_deltas()
{	
	// clear pointers on the mod. list.
	rModule *next = 0;
	rModule *modpt;
	for (modpt = mod_list; modpt; modpt = modpt->next)
	{
		modpt->refcount_delta = 0;
	}
}

// transaction is about to commit; update module refcounts for
// any modified modules.
shrc
prepare_to_commit( bool do_refc)
{
	rModule * modpt = mod_list;
	while (modpt)
	{
		if (modpt->refcount_delta != 0 && do_refc)
		// refcount has really been changed; get a module oid
		// and modify the object.
		// Note: since at this point the oc has been flushed,
		// we manually flush each module after it has been 
		// modified.
		{
			LOID moid;
			OTEntry * ote;
			Ref<sdlModule> mref;
			mref.assign(modpt->loid);
			if (!mref.valid() && mref.type_ok())
			{
				shrc rc;
				// we now have a valid volume reference; now do the update.
				mref.update()->increment_refcount(modpt->refcount_delta);
				// try to work around a gcc constructor bug.
				// W_DO(mref.flush());
				rc = mref.flush();
				if (rc ) return rc;
			}
			else if (strcmp(modpt->mname,"metatypes"))
			{
				fprintf(stderr,"warning: module %s does not exist; cannot update refcount\n",modpt->mname);
			}
			modpt->refcount_delta = 0;
			// finally, continue through list.
		}
		modpt = modpt->next;

	}
	clear_mod_deltas();
	return RCOK;
}

// transaction is about to abort; clear module refcounts for
// any modified modules.
shrc
prepare_to_abort()
{
	clear_mod_deltas();
	return RCOK;
}



			


// predicate for existance of compiled-in language binding, here
// called "methods".  This is overridden in derived classes of
// rType in the C++ language binding; for rType instances this
// is always false.

bool
rType::methods_exist()
{
	return false;
}


// predicate for existance of the persistant type object;
// here, we check for validity of the type oid itself.
// we need to dig the oid out and check for its validity.
bool
rType::metaobj_exists()
{
	OCRef mref;
	mref.assign(loid);
	bool rval;
	rType * ttpt;
	if (!mref.valid() ) // the sense of valid is really stupid now.
	{
		// make sure its type is valid.
		if (mref.get_type(ttpt) || TYPE_OBJECT(sdlInterfaceType).loid != ttpt->loid)
			return false;
		return true;
	}
	else
		return false;
}

// dummy persistent object base virtual fcts.
const void * 
sdlObj::get_top() const
{
	return this;
}

const void *
sdlObj::isa(rType *) const
{
	return 0;
}

void
log_opr(OTEntry *ote, char * desc);


int pin_check = 0;
#include "MemMgr.h"


sdlPinBase::sdlPinBase (const OCRef * r,rType *tpt)
// new version for use with less inlined code.
// we allways call a function; code blows up in size otherwise.
{

	if (r->u.otentry==0) {
		shrc tmp_rc = RC(SH_BadObject);
		OCRef::call_error_handler(tmp_rc,__FILE__,__LINE__,true);
	}
	opt = r->quick_eval();
	ote = r->u.otentry;
	opt = ote->obj;
	CacheBHdr *cpt = get_brec(opt);
	if (cpt->is_free()) // reclaim the object.
	{
		opt = r->internal_eval();
	}
	++(cpt->pin_count);
	++stats.total_pins;
	if (log_pins)
		log_opr(cpt->otentry,"pin")	;
	if (ote->type!=tpt) // don't do cast call for == types
	{
		opt = ote->type->cast(opt,tpt);
		if (opt==0) {
			shrc tmp_rc = RC(SH_BadType);
			OCRef::call_error_handler(tmp_rc,__FILE__,__LINE__,true);
		}
	}
}

sdlPinBase::~sdlPinBase()
// for debugging, external version of unpinning routine,
// normally done in RefPin dtor.
{
	if (ote->obj !=0) // may be null due to delete; I'm not
	// sure what to do about that...
	{
		CacheBHdr *cpt = get_brec(ote->obj);
		--cpt->pin_count;
		cpt->ref_bit = 1;
		cpt->ref2_bit = 1;
	}
	++stats.total_unpins;
	// if (pin_check && stats.total_unpins >= pin_check)
		//mptr->audit();
}

void
null_apply(void *p, HeapOps op) {}


sdl_heap_base::sdl_heap_base()
// check if we are within sdl runtime context, where we need
// to leave the contents alone; otherwize, clear the contents.
{
	if (rt_pt == 0) // there is no active sdl context pointer
		init();
}

#include "sdl_templates.h"
// the following are needed to make various type ops work correctly
// for any and Pool.
template class srt_type<Pool>;
template class RefPin<Pool>;
template class srt_type<any>;
template class RefPin<any>;

// now dummy up the srt_type<Pool>, srt_type<any> methods.
void srt_type<any>::__apply(HeapOps op, void *obpt){};
const Ref<any>   srt_type<any>::isa(const OCRef &ref) { return (Ref<any>&) ref;}
void  srt_type<any>::setup_vtable(void *) {};
void srt_type<any>::set_toid() {} // doesn't really have one.
bool srt_type<any>::methods_exist() { return false; }

srt_type<any>::srt_type(rType **b): rType(b,"any",0) {}

void * srt_type<any>::cast(void *p,rType *t) { return p? p:(void *)1;}

void srt_type<Pool>::__apply(HeapOps op, void *obpt);
const Ref<Pool>   srt_type<Pool>::isa(const OCRef &ref) 
{ 
	LOID tloid;
	SH_DO(ref.get_type(tloid));
	if(tloid.id.serial == ReservedSerial::_Pool)
		return (Ref<Pool>&) ref;
	else
	{
		Ref<Pool> rval;
		rval.assign(0);
		return rval;
	}
}
void  srt_type<Pool>::setup_vtable(void *) {};

void srt_type<Pool>::set_toid() 
{
	lvid_t dummy; // it's ignored..
	serial_t_data tmp;
	tmp._low = ReservedSerial::_Pool.guts._low;
	loid.set(dummy,tmp);
}

bool srt_type<Pool>::methods_exist() { return false; }
srt_type<Pool>::srt_type(rType **b): rType(b,"Pool",0) 
// we need a ctor ala SETUP_VTAB macro.
{
	// always put this on insert_defered.
	next = sdl_insert_defered;
	sdl_insert_defered = this;
	size = 0;
}
void * srt_type<Pool>::cast(void *p,rType *t) { return p? p:(void *)1;}
void  srt_type<Pool>::__apply(HeapOps op, void *obpt){};

TYPE(any) TYPE_OBJECT(any);
TYPE(Pool) TYPE_OBJECT(Pool);

