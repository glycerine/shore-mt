/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// Type.h
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/Type.h,v 1.22 1995/09/28 19:39:36 schuh Exp $ */

#ifndef _TYPE_H_
#define _TYPE_H_

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma interface
#endif

#include "OCTypes.h"

class StringRec;

/* pointer to arbitrary fct. */
typedef  void (*pfct)(...);

/* heap apply fct */
typedef  void (*tfpt)(int,void **);

enum HeapOps
{
    Allocate,		// currently not used.
    PrepareForMem,	// format for memory
    PrepareForDisk,	// format for disk
    DeAllocate,		// free any malloc'ed mem
	ComputeHeapSize, // do heap + text size calculation.
	ReSwizzle		// undo PrepareForDisk.

};


// Module type: for now, this just has a name and volid.
struct rModule
{
	char * mname;
	LOID loid;
	int refcount_delta;
	rModule * next; // modules with refcount modified.  
	// because volRef now has a constructor, we need one too.
	rModule(char *mn,uint4 vhi, uint4 vlow, uint4 s) ;
	rModule(char *mn,const class OCRef &r);
};
//
// A type object.  At some future point, this type will contain other
// information useful for format conversion, browsing, etc.
//
class rType
{
 public:

    rType(rType **p, char *n, int nind = 0)
	{
		base_list = p;
		name = n;
		nindices = nind;
		size = 0;
		next = 0;
		mod = 0;
	}
    int is_descendant(const rType *t) const ;
    virtual void setup_vtable(void *instance);
    virtual void *cast(void *,rType *);
    virtual void ref_apply(pfct fpt, void * opt);
    virtual void __apply(HeapOps op, void *opt);
    virtual void get_string(void *opt, StringRec &srec);
    virtual void set_string(void *opt, StringRec &srec);
	virtual void set_toid(); // this is a bit odd.
	virtual bool metaobj_exists();
	virtual bool methods_exist();
    void inc_refcount(int) const;
  

    // name of the type as given in its SDL definition
    const char *name;

    // null-terminated array of base classes
    rType **base_list;

    // oid of corresponding C++ type object, set in constructor.
    LOID loid;

    // (core) size of an instance of the type; set in ctor
    int size;

    // number of indixes and collections in an instance of the object
    int nindices;

    // oid hash table lookup thread.
    rType * next;

    // pointer back to containing module.
    rModule * mod;
};

//inline void rType::setup_vtable(void *){}


//inline void rType::ref_apply(pfct, void *) {}
//inline void rType::__apply(HeapOps, void *) {}
//inline void rType::get_string(void *, StringRec &){}
//inline void rType::set_string(void *, StringRec &){}

#endif
