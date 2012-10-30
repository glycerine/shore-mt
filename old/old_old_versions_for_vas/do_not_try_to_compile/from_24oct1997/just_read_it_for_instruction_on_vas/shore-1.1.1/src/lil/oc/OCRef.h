/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// OCRef.h
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/OCRef.h,v 1.45 1997/01/24 20:14:07 solomon Exp $ */

#ifndef _OCREF_H_
#define _OCREF_H_

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma interface
#endif

#include "OCTypes.h"
#include "OTEntry.h"

// An OCRef is an object reference, as exported by the object cache.
// The language binding uses OCRef to build a typed persistent ref,
// which exports typed (and more convenient) versions of most of the
// OCRef methods.

// The OCRef methods are divided into 3 categories.  The first
// category consists of those methods that can be used directly by
// application programs; they do not depend on the type of the ref on
// which they are invoked.  The second category contains those methods
// that should be wrapped by the language binding, but are otherwise
// to be used more-or-less directly by application programs.  The
// third category contains methods that are used internally by OCRef.
// Application programs call them indirectly through the typed ref
// classes created by the language binding.

// a hack to make constructors workable: check for the (global)
extern struct sdl_rt_state *rt_pt;
ostream &operator<<(ostream &os, const OCRef &ref);
istream &operator>>(istream &os, OCRef &ref);

class OCRef
{
    friend ostream &operator<<(ostream &os, const OCRef &ref);
    friend istream &operator>>(istream &os, OCRef &ref);

 public:

    // Constructors
	// operator bool() const;
	bool is_null() const;

    inline OCRef() { if (rt_pt==0) u.otentry = 0; }

    // inline OCRef(const OCRef &ref): volref(ref.volref) {}
    // set otentry instead of serial #; this is more semanticly correct,
    // and propably at least as efficient.
    inline OCRef(const OCRef &ref) { u.otentry = ref.u.otentry; }

    inline OCRef(int zero) { assert(zero == 0); u.otentry = 0; }

    ////////////////////////////////////////////////////////////////
    //
    //  Category 1: These methods do not depend in any way on the
    //  type of the ref on which they are invoked.
    //
    ////////////////////////////////////////////////////////////////

    // Returns the volid of the volume in which the referenced object
    // lives.
    shrc get_primary_volid(VolId &volid) const;

    // These methods convert refs into LOIDs.  The first form always
    // returns a primary LOID.  To do so, it may have to go to the vas
    // either to request a new LOID for a newly-created object, or to
    // resolve a secondary LOID to a primary LOID (but never both).
    // The second form can return either a primary or a secondary
    // LOID.  It may still have to go to the vas to request a new LOID
    // for a newly-created object.  The third form may return a
    // primary or secondary LOID, or it may return the nil LOID, if no
    // LOID has been assigned to the object.  This form never makes a
    // call to the vas.

    shrc get_primary_loid(LOID &loid) const;
    shrc get_loid(LOID &loid) const;
    shrc simple_get_loid(LOID &loid) const;

    // Returns lots of info about the given object
    shrc ostat(OStat *osp);

    // Returns the object's C++ type object.
    shrc get_type(rType *&type) const;
    shrc get_type(LOID &type_loid) const;

    // Returns the current lock state of the object with respect to
    // the current transaction.  Note that if the object was creatd
    // during the current transaction, then the object is effectively
    // locked in exclusive mode.  In this case, this method will
    // return EX, but it may be the case that no lock is actually held
    // on the object.
    shrc get_lockmode(LockMode &lm) const;

    // To destroy anonymous objects.  (To destroy registered objects,
    // use ObjCache::unlink).
    shrc destroy() const;

    // Determines whether this ref can be safely followed by the
    // current transaction.  `Safely' means that there is an object at
    // the other end of the ref, and that object can be locked in the
    // indicated lock mode.  This function will obtain a lock in the
    // indicated mode on the object, but will not read it into the
    // object cache.  The `fetch' method can be used for this purpose.
    shrc valid(LockMode lm = READ_LOCK_MODE) const;
    shrc valid(LockMode lm , bool block) const;

    // Similar to `valid,' but does read the referenced object into
    // the object cache.  The `eval' and `quick_eval' methods (which
    // are wrapped by the language binding as `REF(T)::operator->()')
    // fetch objects into the cache if they are not already resident.
    // Therefore, it is never necessary for applications to use this
    // method.  Applications may use it to verify that a ref is valid
    // before actually dereferencing the ref, or to explicitly
    // pre-load objects into the cache.  If the object is already in
    // the cache, and a lock is held in at least the indicated mode,
    // then no action is taken.
    shrc fetch(LockMode lm = READ_LOCK_MODE) const;

    // Removes an object from the object cache, reclaiming the memory
    // held by the object.  If the object is modified, then it is
    // first written back to the server.  If the object is not
    // resident in the cache, then no action is taken.  Note that when
    // a transaction commits, all modified objects are written back to
    // the server.  Therefore, it is never necessary for applications
    // to use this method.  Applications can use it to flush objects
    // that they do not intend to use in the near future.  Note also
    // that this method does not release any locks held on the object;
    // all locks are held until the end of the current transaction.
    shrc flush() const;

    // Indicates whether the given object is currently cached in the
    // object cache.  Note that if the given ref is secondary, we may
    // have to go to the vas to find the object's primary ref to
    // determine whether the object is indeed cached.
    shrc is_resident(bool &res) const;

    // Creates a cross reference pointing to the given object.
    shrc create_xref(const char *path, mode_t mode) const;

    // Hack: from here down should really be protected.
    // protected:
    public:

    ////////////////////////////////////////////////////////////////
    //
    //  Category 2: These methods should be wrapped inside a typed
    //  ref class by the language binding.
    //
    ////////////////////////////////////////////////////////////////

    // NB: The assignment, initialization, comparison, and evaluation
    // methods do not return an error status (i.e., a shrc).
    // Ideally they would throw an exception on failure, but there are
    // no reliable implementations of C++ exceptions yet.  Instead, on
    // error, these methods call the current value of `error_handler,'
    // or `default_error_handler' if the former is nil.

    // Assignment methods
    void assign(const OCRef &ref);
    void assign(const LOID &loid);
    void assign(const void *p);

    // Initialization methods (like constructors)
    void init(const OCRef &ref);
    void init(const LOID &loid);
    void init(const void *p);

    // Comparison methods
    int equal(const OCRef &ref) const;
    int equal(const LOID &loid) const;
    int equal(const void *p) const;

    // Evaluation methods.
    // Eval and quick_eval both return a pointer to the referenced
    // object.  There are 2 differences between them:
    // 1. Quick_eval doesn't check whether the ref is null before
    //    proceeding.  This means that an attempt to dereference a
    //    null ref will cause a fault in quick_eval.  Eval does check,
    //    and returns a null pointer on an attempt to dereference a
    //    null ref, which will most likely cause a fault somewhere
    //    else.  Thus, the check is of dubious value.
    // 2. Eval returns a pointer to the object's type object as well
    //    as a pointer to the object, itself.
    caddr_t eval(rType *&type) const;
    caddr_t quick_eval() const;

    // Creation methods for registered and anonymous objects.
    static shrc create_registered(const char *path, mode_t mode,
				    rType *type, OCRef &ref);
    static shrc create_anonymous(const OCRef &pool, rType *type,
				   OCRef &ref);

    // Pathname resolution.  The latter form returns the object's type
    // loid as well as a ref to the object.  This form is more
    // expensive than the simpler form, and shold only be used when
    // the type is needed.
	// add optionional type * arg to lookup; if passed in, lookup
	// will check if the object is of the type specified.
    static shrc lookup(const char *path, OCRef &ref, rType *type=0);
    static shrc lookup(const char *path, LOID &type, OCRef &ref);

    // Returns an OCRef to the pool containing the referenced object.
    // This method does not depend on the type of the referenced
    // object.  However, the language binding should provide a typed
    // version of this method that returns a `Pool *', not a `void *',
    // for each type ref class.  (This method should really be
    // private.)
    shrc _get_pool(OCRef &ref) const;

    ////////////////////////////////////////////////////////////////
    //
    //  Category 3: These methods are used by the language binding.
    //         Application programs do not use them directly.
    //
    ////////////////////////////////////////////////////////////////

    // Prepares the object for updating.  This function must be called
    // before the object is updated to make sure that the server is made
    // aware of any updates to the object.
    void make_writable(bool fatal=false) const;

    // These methods implement the hairier versions of the `equal,'
    // `eval,' and `make_writable' methods.  They should only be used
    // in the implementations of those methods.
    int internal_equal(const OCRef &ref) const;
    int internal_equal(const void *p) const;
    caddr_t internal_eval() const;
    shrc internal_make_writable() const;

    // Changes the representation of the OCRef from a volref (a serial
    // number) to a pointer to an otentry.  `Vindex' indicates the
    // volume that should be used to resolve to volref (i.e., the volume
    // on which the object containing the ref lives).  If `primary' is
    // True, then the ref will be swizzled to its primary OT entry.
    // Otherwise, it may be swizzled to either a primary or secondary
    // entry.  The ref should not be swizzled before this function is
    // called.
    //
    // This function must be called on any ref before the application
    // is allowed to see it; otherwise, a fault will occur when an
    // attempt is made to dereference the ref.

    shrc swizzle(int vindex, bool primary = false);

    // Changes the representation of the OCRef from an otentry pointer
    // to a volref (a serial number).  `Vindex' indicates the volume
    // that will be used to resolve the volref (i.e., the volume on
    // which the object containing this OCRef lives).
    //
    // Each ref that has been swizzled (via the `swizzle' method, above)
    // must be unswizzled before the object is written back to the
    // server.  If a swizzled ref is written back to the server then it
    // will not be possible to determine the target of the ref.
    shrc unswizzle(int vindex);
    // the __apply fct is called for swizzling/unswizzling from
    // sdl support code. Only 2 ops need to be handled.
    void __apply(HeapOps op); //should probably be inline but isn't

    // Reswizzles the ref to point at its primary OTEntry.
    shrc snap() const;

    ////////////////////////////////////////////////////////////////
    //
    //  Finally, after all those methods, we get to the data content
    //  of an OCRef.
    //
    ////////////////////////////////////////////////////////////////

    // `Volref' is used in the unswizzled form.  `Otentry' is used in
    // the swizzled form.
    // A depressingly global change: get rid of volref, substitute
    // a char space array; also, unanonymize the  union so we can
    // refer to the otentry in the debugger.  This sucks somewhat.
    union{
	// VolRef volref;
	char vspace[sizeof(VolRef)];
	OTEntry *otentry;
    } u;

    VolRef & volref() { return (VolRef &) u; }

    // Creates a pool and return a ref to it.  (Used only by the
    // Pool_ref class.)
    static shrc create_pool(const char *path, mode_t mode, OCRef &ref);

    // Calls the current error handler, passing the given rc as
    // argument
    static void call_error_handler(shrc &rc, const char *file,
	int line, bool fatal=true);

    // For `prepare_to_destroy'
    friend shrc prepare_to_destroy(rType *type, OTEntry *ote);
    inline void init(OTEntry *ote)	{ u.otentry = ote; }

    ////////////////////////////////////////////////////////////////
    //
    //  Error handlers.  An error handler is called in response to an
    //  error in an `equal,' `eval,' or `assign' method.  The language
    //  binding also call it in response to a failed operator new.
    //  The `rc' argument indicates the error condition.  Other
    //  methods just return error codes.  If `error_handler' is 0 then
    //  `default_error_handler' is called.  Applications are free to
    //  install their own error handler by calling
    //  Shore::set_error_handler.  The error handler is 0 is default.
    //
    ////////////////////////////////////////////////////////////////

    static void default_error_handler(shrc &rc);
    static sh_error_handler error_handler;
};

#ifdef DO_INLINES
// gcc blows up with too much inlining...
//////////////////////////////////////////////////////////////////////
//
// For efficiency (we hope), some of the member functions of OCRef
// are inlined here.
//
//////////////////////////////////////////////////////////////////////

// Assignment methods

inline void OCRef::assign(const OCRef &ref)
{
    u.otentry = ref.u.otentry;
}

inline void OCRef::assign(const void *p)
{
    if(p == 0)
	u.otentry = 0;
    else{
	BehindRec *brec = get_brec(p);
	u.otentry = brec->otentry;
    }
}

// Initialization methods (currently the same as assign methods).

inline void OCRef::init(const OCRef &ref) { assign(ref);  }
inline void OCRef::init(const LOID &loid) { assign(loid); }
inline void OCRef::init(const void *p)    { assign(p);    }

// Comparison methods

inline int OCRef::equal(const OCRef &ref) const
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

inline int OCRef::equal(const void *p) const
{
    return (p == 0) ? (u.otentry == 0) : internal_equal(p);
}

inline caddr_t OCRef::quick_eval() const
{
	if(!this || !u.otentry) {
		call_error_handler(RC(SH_BadObject),__FILE__,__LINE__,true);
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

inline caddr_t OCRef::eval(rType *&type) const
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

inline void OCRef::make_writable(bool fatal) const
{
	shrc rc;
	if(rc = internal_make_writable()) {
		RC_PUSH(rc, SH_BadWRef);
		call_error_handler(rc,__FILE__,__LINE__,fatal);	
	}
}
#endif  /* DO_INLINES */
#endif /* __OCREF_H__ */
