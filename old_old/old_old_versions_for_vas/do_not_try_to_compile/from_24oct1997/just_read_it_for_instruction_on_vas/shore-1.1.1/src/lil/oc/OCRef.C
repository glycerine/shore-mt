/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// OCRef.C
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/OCRef.C,v 1.51 1996/07/24 19:15:28 schuh Exp $ */

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma implementation "OCRef.h"
#endif

#include "OCInternal.h"
#include "ObjCache.h"
#include "OCRef.h"
#include <debug.h>

#define OCREF_DO(x)				\
{						\
    shrc __e = (x);				\
    if(__e)					\
	call_error_handler(__e,__FILE__,__LINE__,true);	\
}

sh_error_handler OCRef::error_handler = 0;

void
OCRef::default_error_handler(shrc &rc)
{
    cerr << rc << endl;
    _exit(1);
}

void
OCRef::call_error_handler(shrc &rc,
    const char *file, int line, bool fatal)
{
			
    rc.add_trace_info(file, line);
	
    // call the current error handler
    if(error_handler) {
	error_handler(rc);
    } else {
	if( check_my_oc() !=0 && get_my_oc()->is_einitialized() ) {
	    default_error_handler(rc);
	} else {
	    // Cannot even call error handle if not initialized!
	    cerr << 
	    "Object cache not initialized. Fatal error."
	    << endl;
	    fatal = true;
	}
    }

    // in case the error handler didn't do it
    if(fatal) {
	 _exit(1);
    }
}

shrc
OCRef::create_registered(const char *path,
			 mode_t mode,
			 rType *type,
			 OCRef &ref)		// OUT ref
{
    OTEntry *ote;

    W_DO(OC_ACC(create_registered)(path, mode, type, ote));
    ref.u.otentry = ote;
    return RCOK;
}

shrc
OCRef::create_anonymous(const OCRef &pool,
			rType *type,
			OCRef &ref)		// OUT ref
{
    OTEntry *ote;

    W_DO(OC_ACC(create_anonymous)(pool.u.otentry, type, ote));
    ref.u.otentry = ote;
    return RCOK;
}

shrc
OCRef::create_pool(const char *path,
		   mode_t mode,
		   OCRef &ref)			// OUT ref
{
    OTEntry *ote;

    W_DO(OC_ACC(create_pool)(path, mode, ote));
    ref.u.otentry = ote;
    return RCOK;
}

shrc
OCRef::create_xref(const char *path, mode_t mode) const
{
    if(u.otentry->has_serial() && u.otentry->is_secondary())
	W_DO(snap());

    W_DO(OC_ACC(create_xref)(path, mode, u.otentry));

    return RCOK;
}

shrc
OCRef::destroy() const
{
    W_DO(OC_ACC(destroy_object)(u.otentry));
    return RCOK;
}


shrc
OCRef::valid(LockMode lm) const
{
    if(u.otentry == 0)
	return RC(SH_BadObject);
	W_DO(OC_ACC(valid)(u.otentry,lm));
    return RCOK;
}

shrc
OCRef::valid(LockMode lm,bool block) const
{
    if(u.otentry == 0)
	return RC(SH_BadObject);
	if (block)
	{
		W_DO(OC_ACC(valid)(u.otentry,lm));
	}
	else
	{
		// return the real err no.
		return (OC_ACC(lock_obj)(u.otentry, lm,block));
	}

    return RCOK;
}

shrc
OCRef::fetch(LockMode lm) const
{
    if(u.otentry == 0)
	return RC(SH_BadObject);

    W_DO(OC_ACC(fetch)(u.otentry, lm));
    return RCOK;
}

shrc
OCRef::flush() const
{
    if(u.otentry == 0)
	return RC(SH_BadObject);

    W_DO(OC_ACC(flush)(u.otentry));
    return RCOK;
}

shrc
OCRef::is_resident(bool &res) const
{
    W_DO(OC_ACC(is_resident)(u.otentry, res));
    return RCOK;
}

shrc
OCRef::lookup(const char *path, OCRef &ref,rType *tpt)
{
    OTEntry *ote;

    W_DO(OC_ACC(lookup)(path, ote,tpt));
    ref.u.otentry = ote;
    return RCOK;
}

shrc
OCRef::lookup(const char *path, LOID &type, OCRef &ref)
{
    OTEntry *ote;

    W_DO(OC_ACC(lookup)(path, type, ote));
    ref.u.otentry = ote;
    return RCOK;
}

shrc
OCRef::ostat(OStat *osp)
{
    if(u.otentry == 0)
	return RC(SH_BadObject);

    W_DO(OC_ACC(stat)(u.otentry, osp));
    return RCOK;
}

// This function returns the volid on which the given object resides.
// There is no "non-snapping" version of this method, as it would be
// meaningless; what volid would it return?

shrc
OCRef::get_primary_volid(VolId &volid) const
{
    // if the ref is nil, then return the null volid
    if(u.otentry == 0)
	volid = VolId::null;

    else{
	W_DO(snap());
	volid = OC_ACC(vt)[u.otentry->volume];
    }

    return RCOK;
}

shrc
OCRef::get_primary_loid(LOID &loid) const
{
    // if the ref is nil then return the nil loid
    if(u.otentry == 0)
	loid.set(LOID::null);

    else{
	W_DO(snap());
	W_DO(OC_ACC(get_primary_loid)(u.otentry, loid));
    }

    return RCOK;
}

shrc
OCRef::get_loid(LOID &loid) const
{
    // if the ref is nil then return the nil loid
    if(u.otentry == 0)
	loid.set(LOID::null);
    else
	W_DO(OC_ACC(get_loid)(u.otentry, loid));

    return RCOK;
}

shrc
OCRef::simple_get_loid(LOID &loid) const
{
    // if the ref is nil then return the nil loid
    if(u.otentry == 0)
	loid.set(LOID::null);
    else
	W_DO(OC_ACC(simple_get_loid)(u.otentry, loid));

    return RCOK;
}

shrc
OCRef::get_type(rType *&type) const
{
    FUNC(OCRef::get_type);
    // if the ref is nil then return 0
    if(u.otentry == 0)
	type == 0;
    else
	W_DO(OC_ACC(get_type)(u.otentry, type));

    return RCOK;    
}

shrc
OCRef::get_type(LOID &type_loid) const
{
    FUNC(OCRef::get_type);

    // if the ref is nil then return 0
    if(u.otentry == 0)
	type_loid.set(LOID::null);
    else
	W_DO(OC_ACC(get_type)(u.otentry, type_loid));

    return RCOK;    
}

shrc
OCRef::get_lockmode(LockMode &lm) const
{
    W_DO(OC_ACC(get_lockmode)(u.otentry, lm));
    return RCOK;
}

shrc
OCRef::_get_pool(OCRef &ref) const
{
    OTEntry *pool_otentry;

    // if the ref is nil then return a nil ref
    if(u.otentry == 0)
	ref.u.otentry == 0;

    else{
	W_DO(OC_ACC(get_pool)(u.otentry, pool_otentry));
	ref.u.otentry = pool_otentry;
    }

    return RCOK;
}

void
OCRef::assign(const LOID &loid)
{
    OTEntry *ote;
	if (loid.id.serial.is_null()){
		ote = 0;
	}
	else {
		OCREF_DO(OC_ACC(get_ote)(loid, ote));
	}
    u.otentry = ote;
}

int
OCRef::equal(const LOID &loid) const
{
    OTEntry *ote;

    // get the object's ote
    OCREF_DO(OC_ACC(get_ote)(loid, ote));

    // simple case: the otentries match
    if(u.otentry == ote)
	return 1;

    // if either of the ote's doesn't have a serial, then they can't
    // be the same
    if(!u.otentry->has_serial() || !ote->has_serial())
	return 0;

    // if the LHS is remote...
    if(u.otentry->is_secondary()){

	// reswizzle it to its primary ote
	OCREF_DO(snap());

	// if the RHS is also remote, snap it as well
	if(ote->is_secondary()){
	    OTEntry *primary;
	    OCREF_DO(OC_ACC(snap)(ote, primary));
	    ote = primary;
	}

	// now see if they are the same
	return u.otentry == ote;
    }

    // (the LHS is local)

    // if the RHS is remote...
    if(ote->is_secondary()){

	OTEntry *primary;

	// snap the RHS
	OCREF_DO(OC_ACC(snap)(ote, primary));

	// now see if they are equal
	return u.otentry == ote;
    }

    // both are local otentries; they must not be equal
    return 0;
}

int
OCRef::internal_equal(const OCRef &ref) const
{
    // simple case: the otentries match
    if(u.otentry == ref.u.otentry)
	return 1;

    // if either object has no serial hen they must not be equal
    if(!u.otentry->has_serial() || !ref.u.otentry->has_serial())
	return 0;

    // if the LHS is remote...
    if(u.otentry->is_secondary()){

	// reswizzle it to its primary ote
	OCREF_DO(snap());

	// if the RHS is also remote, reswizzle it as well
	if(ref.u.otentry->is_secondary()){
	    OCREF_DO(ref.snap());
	}

	// now see if they are the same
	return u.otentry == ref.u.otentry;
    }

    // (the LHS is local)

    // if the RHS is remote ...
    if(ref.u.otentry->is_secondary()){

	// reswizzle the RHS to its primary ote
	OCREF_DO(ref.snap());

	// now see if they are equal
	return u.otentry == ref.u.otentry;
    }

    // both are primary otentries; they must not be equal
    return 0;
}

int
OCRef::internal_equal(const void *p) const
{
    // see if they are both 0
    if(p == 0)
	return u.otentry == 0;

    // see if the ote points to p
    if(u.otentry->obj == p)
	return 1;

    // if the ote has no serial then they must not be equal
    if(!u.otentry->has_serial())
	return 0;

    // if the above failed and it's a primary ref, then they aren't equal
    if(u.otentry->is_primary())
	return 0;

    // snap the remote ref and see if the primary ote points to the object
    OCREF_DO(snap());

    return u.otentry->obj == p;
}

caddr_t
OCRef::internal_eval() const
{
    // if the OT entry doesn't have a pointer to the object...
    if(u.otentry->obj == 0){

	// ...because it is an off-volume reference...
	if(u.otentry->is_secondary()){
	    OCREF_DO(snap());
	}

	// if the object isn't already cached, cache it
	if(u.otentry->obj == 0){
	    OCREF_DO(OC_ACC(fetch)(u.otentry));
	}
    }
    // see if we need to reclaim a freed object...
    else if (get_brec(u.otentry->obj)->is_free())
    // mark it used so it doesn't get reallocated.
    {
		OC_ACC(mmgr).reclaim(get_brec(u.otentry->obj));
    }
	if (u.otentry->is_unswizzled())
		OCREF_DO(reclaim_object(u.otentry));


    return u.otentry->obj;
}

shrc
OCRef::internal_make_writable() const
{
	return OC_ACC(make_writable)(u.otentry);
}

shrc
OCRef::snap() const
{
    // if this is the primary ote, then we're done
    if(u.otentry->is_secondary()){
	OTEntry *primary;
	W_DO(OC_ACC(snap)(u.otentry, primary));
	((OCRef *)this)->u.otentry = primary;
    }

    return RCOK;
}

// NB: This is the ONLY OCRef member that deals with unswizzled refs

shrc
OCRef::swizzle(int vindex, bool primary)
{
    OTEntry *ote;

    // if the volid is nil then make it a nil ref.
    if(volref().is_null())
	ote = 0;

    // get the primary ote
    else if(primary){
	W_DO(OC_ACC(get_primary_ote)(vindex, volref(), ote));
    }

    // or just any old ote
    else
	{
	W_DO(OC_ACC(get_ote)(vindex, volref(), ote));
	}

    u.otentry = ote;
    return RCOK;
}

shrc
OCRef::unswizzle(int vindex)
{
    // check for null ref
    if(u.otentry == 0)
	volref().set(VolRef::null);

    else{

	OTEntry *ote;

	// get the matching OT entry
	if(u.otentry->volume == vindex)
	    ote = u.otentry;
	else{
	    W_DO(OC_ACC(get_matching)(u.otentry, vindex, ote));
	}

	// if the object doesn't have a serial yet, ask for one
	if(!ote->has_serial()){
	    W_DO(OC_ACC(get_volref)(ote));
	}

	// unswizzle the ref
	volref().set(ote->volref);
    }

    return RCOK;
}

ostream &
operator<<(ostream &os, const OCRef &ref)
{
    if(ref.u.otentry == 0)
	os << "nil";
    else{
	LOID loid;
	shrc rc;

	os << "[ ote = " << (void *)ref.u.otentry;
	os << ", addr = " << (void *)ref.u.otentry->obj;
	os << ", loid = ";

	rc = ref.simple_get_loid(loid);
	if(rc)
	    os << "<error>";
	else
	    os << loid;

	os << " ]";

	if(rc)
	    os << "Couldn't get loid because " << rc;
    }

    return os;
}

istream &
operator<<(istream &os, OCRef &ref)
{
    shrc rc;
    LOID loid ;
    os >> loid ;

    if( ! os.fail()) {
	ref.assign(loid);
    }
    return os;
}


// formerly inlined functions from OCRef.h
// gcc blows up with too much inlining...
//////////////////////////////////////////////////////////////////////
//
// For efficiency (we hope), some of the member functions of OCRef
// are inlined here.
//
//////////////////////////////////////////////////////////////////////

// Assignment methods

void OCRef::assign(const OCRef &ref)
{
    u.otentry = ref.u.otentry;
}

void OCRef::assign(const void *p)
{
    if(p == 0)
	u.otentry = 0;
    else{
	BehindRec *brec = get_brec(p);
	u.otentry = brec->otentry;
    }
}

// Initialization methods (currently the same as assign methods).

void OCRef::init(const OCRef &ref) { assign(ref);  }
void OCRef::init(const LOID &loid) { assign(loid); }
void OCRef::init(const void *p)    { assign(p);    }

// Comparison methods

int OCRef::equal(const OCRef &ref) const
{

#ifdef __GNUG__

    if(u.otentry == ref.u.otentry)
	return 1;
    else if(u.otentry == 0 || ref.u.otentry == 0)
	return 0;
    return internal_equal(ref);

#else

    int eq;

    if(u.otentry == ref.u.otentry)
	eq = 1;
    else if(u.otentry == 0 || ref.u.otentry == 0)
	eq = 0;
    eq = internal_equal(ref);

    return eq;

#endif

}

int OCRef::equal(const void *p) const
{
    return (p == 0) ? (u.otentry == 0) : internal_equal(p);
}

caddr_t OCRef::quick_eval() const
{
	if(!this || !u.otentry) {
		shrc tmp_rc = RC(SH_BadObject);
		call_error_handler(tmp_rc,__FILE__,__LINE__,true);
	}

#ifdef __GNUG__

    if(u.otentry->obj == 0 || u.otentry->is_unswizzled())
	// if unswizzled, need to reclaim it.
	return internal_eval();
    return u.otentry->obj;

#else /* cfront */

    caddr_t addr;

    if((addr = u.otentry->obj) == 0 || u.otentry->is_unswizzled())
	addr = internal_eval();
    return addr;

#endif /* __GNUG__ */

}

caddr_t OCRef::eval(rType *&type) const
{
    caddr_t addr;

    if(u.otentry == 0){
	addr = 0;
	type = 0;
    }

    else{

	// if the entry doesn't have a pointer to the object...
	if((addr = u.otentry->obj) == 0)
	    addr = internal_eval();

	// don't have to check for type == 0 - we will never have an
	// object in the cache without knowing its type
	type = u.otentry->type;
    }

    return addr;
}

void OCRef::make_writable(bool fatal) const
{
	shrc rc;
	if(rc = internal_make_writable()) {
		RC_PUSH(rc, SH_BadWRef);
		call_error_handler(rc,__FILE__,__LINE__,fatal);	
	}
}
