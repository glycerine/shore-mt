/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

// sdl_templates.h: macros used for the C++ language binding
#ifndef __SDL_TEMPLATES_H__
#define __SDL_TEMPLATES_H__ 1
#include <stdlib.h>
// #include <sdl_index.h> // template for typed indices.
#include <sdl_set.h>


#define HANDLE_NEW_ERROR(op)\
{							\
	shrc __rc = (op);		\
	if(__rc){				\
	    OCRef::call_error_handler(__rc,"in operator new()",0,true);\
		return 0; 			\
	}						\
}

#define REFCHECKTHIS(x) \
	if(!this || !this->u.otentry) {\
		shrc __rc = RC(SH_BadObject); \
		OCRef::call_error_handler(__rc,x,0,true);\
		return 0; \
	}

#ifdef NOREFCHECK
/* give the user an easy way to turn this off */
#undef REFCHECKTHIS
#define REFCHECKTHIS(x)
#endif /*NOREFCHECK*/


// constructors are a problem...
// REF_DCL(T) declares ref<t>, e.g. the class encapsulating pointer 
// behaviour for the interface t.
// template version of ref class
extern long total_pins;
extern long total_unpins;
template <class T> class Ref;
template <class T> class WRef;
template <class T> class RefPin;
template <class T, class B> class DRef;
//class Ref<any>;
// typedef void any;
//typedef any_ref Ref<any>;
// class Ref<any>;
//class Ref<Pool>;
// need to include Pool.h to properly override Ref<Pool> definition.
#include <Pool.h>

// use pinning class
class sdlPinBase { 
public:
	OTEntry *ote;  // primary ote for object pinned.
	void *opt; 		// properly cast pointer to obj.
	sdlPinBase(const OCRef *r,rType *tpt);  // intialize with a ref and return type
	~sdlPinBase(); // unpin.
};

template <class T>
class RefPin : protected sdlPinBase
{
public:
	static srt_type<T> TypeObj;
	RefPin(const OCRef *r) : sdlPinBase(r,&RefPin<T>::TypeObj) {}
	//operator const	bool() const {return uval.ocr ?true:false; }
	// i'm not sure of the roll of operator bool.
	const T * operator->() {return (const T *)(opt); }
};

template <class T>
class WRefPin :protected sdlPinBase
{
public:
	WRefPin(const OCRef &r) :sdlPinBase(&r,&RefPin<T>::TypeObj)
	{ r.make_writable(); }
	WRefPin(const OCRef *r) :sdlPinBase(r,&RefPin<T>::TypeObj)
	{ r->make_writable(); }
	T * operator->() { return (T *)(opt); }
};

#define TYPE_OBJECT(T) RefPin<T>::TypeObj
// address of Type  object
template <class T,class _Base_>
class BRef :public _Base_ { /* ref<A> */				
public:									
	// constructors come from Ref now.
	// inline BRef(const BRef<T,_Base_> &r) { OCRef::init(r); }			
	inline BRef &operator=(const BRef<T,_Base_> &r)			
	{ OCRef::assign((OCRef &)r); return *this; }				
	inline BRef & operator=(const T *p)				
	{ OCRef::assign(p); return *this; }					
	inline int operator==(const BRef<T,_Base_> &ref) const			
	{ return OCRef::equal((const OCRef &)ref); }				
	inline int operator==(const OCRef &ref) const			
	{ return OCRef::equal(ref); }						
	inline int operator==(const T *p) const				
	{ return OCRef::equal(p); }						
	inline int operator!=(const BRef<T,_Base_> &ref) const			
	{ return !OCRef::equal((const OCRef &)ref); }				
	inline int operator!=(const OCRef &ref) const			
	{ return !OCRef::equal(ref); }						
	inline int operator!=(const T *p) const				
	{ return !OCRef::equal(p); }						
#ifndef NO_REFINT
	// the following operators can cause bizarre overloading bugs
	// in g++; -DNO_REFINT should eliminate them if they occur,
	// but will also cause problems with use of refs as cond. exprs.
	inline operator int() const					
	{ return !OCRef::equal(0); }						
	inline  operator bool() const {return !equal(0); }
#endif
	static shrc new_persistent(const char *path, mode_t mode,	
				   Ref<T> &r)				
	{	return OCRef::create_registered(path, mode, &TYPE_OBJECT(T), (OCRef &)r);  }
	static shrc new_persistent(Ref<Pool> pool, Ref<T> &r)		
	{	return OCRef::create_anonymous(pool, &TYPE_OBJECT(T), (OCRef &)r); }
	// non static versions of new_persistent that set "this".
	shrc new_persistent(const char *path, mode_t mode)				
	{	return OCRef::create_registered(path, mode, &TYPE_OBJECT(T), *this);  }
	shrc new_persistent(Ref<Pool> pool)		
	{	return OCRef::create_anonymous(pool, &TYPE_OBJECT(T), *this); }
	static shrc lookup(const char *path, BRef<T,_Base_> &r)		
	{	return OCRef::lookup(path,r,&TYPE_OBJECT(T));}
	shrc lookup(const char *path)		
	{	return OCRef::lookup(path,*this,&TYPE_OBJECT(T));}
	// in order to use the TYPE_OBJECT macro, the following
	// must be defined separately, explicitly for each type.
	// inline const T * operator->() const;					
	// inline T *update() const;
	RefPin< T>  operator->() const					
	{								
	    return RefPin<T>(this);
	}
	WRefPin<T> update() const { /* mark object as updated */		
	    return WRefPin<T>(this);
	}
	BRef<T,_Base_> isa(rType * want_type) const 				
	{ rType *my_type;						
		SH_HANDLE_ERROR(get_type(my_type));			
		if (my_type->cast(0,want_type))				
			return *this;					
		else							
		{	// construct null by default(OCRef) ctor
			BRef<T,_Base_>  nullref;
			return nullref;
		}
	}								
	bool type_ok() // check if the type of the object is
	// consistent with the type of this ref.
	{ 	rType *my_type;						
		SH_HANDLE_ERROR(OCRef::get_type(my_type));			
		if (my_type->cast(0,&RefPin<T>::TypeObj))
			return true;
		else
			return false;
	}
	shrc get_pool(Ref<Pool> &pool)					
	{ W_DO(_get_pool(pool)); return RCOK; }				
};


// ref class for derived classes, e.g. interface a : b ->
// typedef DRef<a,b> class Ref<a>;
#ifdef NOT_defined
template <class T,class B>
class DRef :public BRef<T> { /* ref<A> */				
public:									
	inline DRef() {};						
	inline DRef(const T *p):BRef<T>(p) {  }				
	inline DRef (const DRef<T,B> &r):BRef<T>(r) {  }			
	operator class Ref<B> () { return (Ref<B> &) *this; }
};

// multiple inheritance instances..
template <class T,class B, class C>
class DRef2 :public BRef<T> { /* ref<A> */				
public:									
	// inline DRef() {};						
	// inline DRef(const T *p):BRef<T>(p) {  }				
	// inline DRef (const DRef<T> &r):BRef<T>(r) {  }			
	operator Ref<B> () { return (Ref<B> &) *this; }
	operator Ref<C> () { return (Ref<C> &) *this; }
};

// multiple inheritance instances..
template <class T,class B, class C,class D>
class DRef3 :public BRef<T> { /* ref<A> */				
public:									
	// inline DRef() {};						
	// inline DRef(const T *p):BRef<T>(p) {  }				
	// inline DRef (const DRef<T> &r):BRef<T>(r) {  }			
	operator Ref<B> () { return (Ref<B> &) *this; }
	operator Ref<C> () { return (Ref<C> &) *this; }
	operator Ref<D> () { return (Ref<D> &) *this; }
};
template <class T> 
class Ref<T> : public BRef<T,Ref<any> >
{
public:	
	Ref() {};						
	Ref(const T *p);  :BRef<T,Ref<any> >(p) {  }				
	Ref(const BRef<T,Ref<any> > &r):BRef<T,Ref<any> >(r) {  }			
};
#endif
/* this is unreliable; now done in INTERFACE_POSTDEFS.
#ifdef MODULE_CODE
#define EXPL_T(name,base) template class BRef<name,Ref<base> >;
#else
#define EXPL_T(name,base)
#endif
*/
#define EXPL_T(name,base)
#define REF_CTORS(T) Ref(){}; Ref(const T *p); \
Ref(const Ref<T> &r) { OCRef::init(r);}
#define DCL_DREF(T)										\
EXPL_T(T,any)												\
class Ref<T> : public BRef<T,Ref<any> > { 	public:REF_CTORS(T) };

#define DCL_DREF1(T,A)										\
EXPL_T(T,A )													\
class Ref<T> : public BRef<T,Ref<A> > { 	public:REF_CTORS(T) };

#define DCL_DREF2(T,A,B)									\
EXPL_T(T,A )													\
class Ref<T> : public BRef<T,Ref<A> > { public:	\
	REF_CTORS(T)								\
	operator Ref<B> () const { return (Ref<B> &) *this; }	\
};

#define DCL_DREF3(T,A,B,C)									\
EXPL_T(T,A )													\
class Ref<T> : public BRef<T,Ref<A> > { public:						\
	REF_CTORS(T)								\
	operator Ref<B> () const { return (Ref<B> &) *this; }	\
	operator Ref<C> () const { return (Ref<C> &) *this; }	\
};


template <class T>
class WRef : public Ref<T> {
public:
	WRef(){}
    inline WRef(const T *p) : Ref<T>(p) { if (p!= 0)  make_writable(); }
    inline WRef (const Ref<T> &r) : Ref<T>(r) {  if (r!= 0) make_writable(); }
	WRefPin<T> operator->() { 
		// You wouldn't think we'd have to do this, but we do:
		return this->update(); 
	}

	inline WRef &operator=(const Ref<T> &r)			
		{ assign((OCRef &)r); make_writable(); return *this; }				
	inline WRef & operator=(const T *p)				
		{ assign(p); make_writable(); return *this; }					
};

// macros to declare ref's.
// if T has no base classes use DCL_BREF
#define DCL_BREF(T) typedef BRef<T> class Ref<T>;
// if T has a base class B, use DCL_DREF(T,B)
// #define DCL_DREF(T,B) typedef DRef<T,B> class Ref<T>;
// more may be needed for multiple inheritance...




template <class T> // replacement for former TYPE(T)
class srt_type	:public rType {
public:
	srt_type( rType **b=0);
	void *cast(void *,rType *);
	void  __apply(HeapOps op, void *obpt);
	const Ref<T>   isa(const OCRef &ref);
	void  setup_vtable(void *) ;
	void set_toid();
	bool methods_exist();  // true if this type has methods available.
};


// consistency-checking class to make sure that all .o's have the
// same language binding.
template <class T>
class SetupClass {
	public: SetupClass(int); // just a ctor.
};

// ref with inverse class for use in relationship implementation
template <class T, class TCONT, int TC_OFFSET> 
// ref to T, inverse ref in TINV, containod in TCONT, field offset TC_OFFSET
class RefInv : public Ref<T>
{
	public:
	TCONT * get_contpt() { 
		return (TCONT *) (((char *) this) -  TC_OFFSET);
	}
	void operator=(const Ref<T> &Targ);
	/* need to disabmiguate operator= in the following 2 ops. */
	void del_from_inverse(const Ref<T> &targ)
	{ this->Ref<T>::operator=( NULL); };
	void add_from_inverse(const Ref<T> &Targ)
	{ this->Ref<T>::operator=( Targ); };
}; /* end REF_INV_DECL; note no semi; added in language binding */

template <class T, class TCONT,int TC_OFFSET>
class SetInv : public Set< Ref<T> >
{	public:	
	SetInv() {} // default ctor only.
	TCONT * get_contpt() {
		return (TCONT *) (((char *) this) -  TC_OFFSET);
	};
	void add(const Ref<T>  &arg);
	void del(const Ref<T> &arg);
	Ref<T> delete_one();
	void del_from_inverse(const Ref<T>  &arg) { Set< Ref<T> >::del(arg); };
	void add_from_inverse(const Ref<T>  &arg) { Set< Ref<T> >::add(arg); };
}; /* end of SetInv template */

template <class T, class TCONT,int TC_OFFSET>
class BagInv : public Bag< Ref<T> >
{	public:	
	BagInv() {}; // default ctor only.
	TCONT * get_contpt() {
		return (TCONT *) (((char *) this) -  TC_OFFSET);
	};
	void add(const Ref<T>  &arg);
	void del(const Ref<T> &arg);
	Ref<T> delete_one();
	void del_from_inverse(Ref<T>  arg) { Bag< Ref<T> >::del(arg); };
	void add_from_inverse(Ref<T>  arg) { Bag< Ref<T> >::add(arg); };
}; /* end of BagInv template */


// the sdl built-in boolean class
// to get around bogus include conflicts, try this.
typedef bool boolean;

// Type_ref A_type_ref(A_OID);
// Type A_type_object(...); // if needed (see above)

// Type_ref B_type_ref(B_OID);
// Type B_type_object(...); // if needed (see above)

// overview: interface things are printed out in 3 parts.
// 1: before any class definitions are printed out for an
// interface, the macro INTERFACE_PREDEFS(iname) is called.
// this prints out (currently) the defintion for the
// ref for that class as well as forward declartions for the
// type object.
// next, the class definition for the interface is defined.
// along with some defs that need to know the type specification
// in INTERFACE_POSTDEFS(name)
// finally, the Code portion of the binding is build
// bey INTERFACE_CODEDEFS(iname) for things that depend
// only on the type name, and TYPE_CAST_DEF .. TYPE_CAST_END
// and REF_APPLY_DEF ...REF_APPLY_END for class specific parts.


#define INTERFACE_PREDEFS(name)	class name; \
DCL_DREF(name)
#define INTERFACE_PREDEFS1(name,bname)	class name; \
DCL_DREF1(name,bname)
#define INTERFACE_PREDEFS2(name,b1,b2)	class name; \
DCL_DREF2(name,b1,b2)
#define INTERFACE_PREDEFS3(name,b1,b2,b3)	class name; \
DCL_DREF3(name,b1,b2,b3)
// class Apply<Ref<name> >: public DoApply<Ref<name > > {};
#define INTERFACE_POSTDEFS(name)
#define INTERFACE_CODEDEFS(name,nind,base)	\
SETUP_VTAB(name,nind)			\
VMADDR(name)				\
NEW_PERSISTENT(name)			\
CLASS_VIRTUALS(name)			\
template class BRef<name,Ref<base> >; 	\
template class WRef<name >; 	\
/* template class BRef<name,Ref<any> >; */	\
template class Apply<Ref<name> >;		\
template class RefPin<name>;		\
template class WRefPin<name>;		\
template class srt_type<name>;		\
TYPE(name) TYPE_OBJECT(name);
#ifdef oldcode
// these used to be in INTERFACE_CODEDEFS, but now, don't print
// them unless they've been used someplace.
template class Set< Ref<name> >;		\
template class Bag< Ref<name> >;		\
template class Sequence< Ref<name> >;		\

#endif

#if defined __GNUG__  || defined __ANSI_CPP__
// this should really be if ansi_cpp or something
#define CONCAT(a,b) a##b
#define CONCAT3(a,b,c) a##b##c
#define STRING(LIT) #LIT
#else 
/* riesser cpp hack */
#define CONCAT(a,b) a/* */b
#define CONCAT3(a,b,c) a/* */b/* */c
#define DQMARK "
#define STRING(LIT) DQMARK LIT"
// unfortunately this produce " litval" instead of "litval"
#endif
#define REF(T) Ref<T>
#define WREF(T) WRef<T> 
#define INDEX(t1,t2) sdl_index<t1,t2>
#define VAL_INDEX(t1,t2) sdl_index<t1,t2>
#define REF_INDEX(t1,t2) sdl_ref_index<t1,REF(t2)>
#define STR_VAL_INDEX(t1,t2) sdl_str_index<t2>
#define STR_REF_INDEX(t1,t2) sdl_str_ref_index<REF(t2)>
// typedef REF(any) REF(void);
// name of set<T>
#define SET(T) Set<T>
// name of bag<T>
#define BAG(T) Bag<T>
// name of  type<T>
#define TYPE(T) srt_type<T>
#define TYPE_PT(T) (&TYPE_OBJECT(T))	
// name of setup<T> ( compiled in type oid consistency checking )
// name of local instance of setup<T> (more consistency checking)
#define SDL_STRING(dim) sdl_string
// dimensions of strings are ignored for now.
#define T_OID(T) CONCAT(T,_OID)
// #include "s_cache.h"
//extern void sdl_InsertTypeObj(int,rType *);
extern void sdl_InsertTypeObj(rType *);
extern rType * sdl_insert_defered;




// the SETUP_VTAB macro defines some functions declared in various
// interface support classses.  The following 4 member functions
// are defined:
// TYPE(T):setup_vtable(void * instances) -> used to set the vtbl pointer
// when a new instance is created.
// TYPE(T):__apply((*)f(...),void *)   -> swizzling support:
// gets passed in a function
// pointer which is applied to each instance of a REF in a given instance
// of interface T, when applied.
// TYPE(T)::TYPE(T) -> sets the size field according to the compiled-in
// size; also inserts the object into a program-specific hash table.
// TYPE(T)::isa(const OCRef &) -> given a ref<any> returns a ref<T>
// if the ref<any>  poinst to a T or a subclass of T; else, returns
// a null reference.
typedef  void (*tfpt)(int,void **);
#define SETUP_VTAB(T,NIND)  /* setup_vtable member function of type */ 	\
void TYPE(T)::setup_vtable( void * instance) 				\
{									\
	new(instance) T;						\
} 									\
/* define apply's here also */						\
void TYPE(T)::__apply(HeapOps op, void * objpt)			\
{									\
	((T *)objpt)->__apply(op);					\
}									\
/* also define the constructor here. */					\
TYPE(T)::srt_type(rType **b): rType(b,STRING(T),NIND)			\
{									\
	size = sizeof(class T);						\
	serial_t_data tmp;						\
	tmp._low = T_OID(T);						\
	loid.set(CUR_MOD.loid.volid(),tmp);					\
	mod = &CUR_MOD;							\
	if (T_OID(T) != 0)						\
	    sdl_InsertTypeObj(this);					\
	else /* add to defered list */					\
	{								\
	    next = sdl_insert_defered;					\
	    sdl_insert_defered = this;					\
	}								\
};									\
/* extra function in case the type loid isn't set initially  */ 	\
void TYPE(T)::set_toid()				 		\
{									\
	serial_t_data tmp;						\
	tmp._low = T_OID(T);						\
	loid.set(CUR_MOD.loid.volid(),tmp);					\
};									\
/* finally, isa; not inlined due to dependencies. */			\
const REF(T)   								\
TYPE(T)::isa(const OCRef &ref) {					\
	rType * reftype; void * refpt; 					\
	refpt = ref.eval(reftype);					\
	return (T *)(cast(refpt,reftype));				\
};									\
/* one more for the road */						\
bool 									\
TYPE(T)::methods_exist() { return true; };				\
/* and another */							\
REF(T)::Ref(const T *p) { OCRef::init(p?p->get_top():0); }			\



// redefine this from being in REF_PIN to being in REF.
// leave in a dummy version for now.
// VMADDR(T) is no longer used.
#define VMADDR(T)

/* 2 versions of new_persistent now... */
/* also bind them into operator new */
// allocation and deallocation support: objects can be created
// as registered or anonymous.
#define NEW_PERSISTENT(T)						\
void * 									\
T::operator new(unsigned int, const char *path, mode_t mode) {		\
	REF(T) ref;							\
	HANDLE_NEW_ERROR(REF(T)::new_persistent(path, mode, ref));	\
	return ref.quick_eval();					\
}									\
void * 									\
T::operator new(unsigned int, REF(Pool) pool) {				\
	REF(T) ref;							\
	HANDLE_NEW_ERROR(REF(T)::new_persistent(pool, ref));		\
	return ref.quick_eval();					\
}									\
/* we temporarily throw in a transient object allocator... */		\
/* note: this only works for dummy storage manager....     */		\
void * 									\
T::operator new(unsigned int) {						\
	REF(Pool) dpool; /* dummy reference; not used */		\
	REF(T) ref;							\
	HANDLE_NEW_ERROR(REF(T)::new_persistent(dpool, ref));		\
	return ref.quick_eval();					\
}


#define CLASS_VIRTUALS(T)						\
const void * T::get_top() const { return this; };			\
const void * T::isa(rType * tpt) const 					\
{ return get_type()->cast((void *)this,tpt); }

// cache based version of new.
#define TYPE_CAST_DEF(T)						\
void * TYPE(T)::cast(void * opt, rType * cast_type)			\
{	if (this== cast_type) return opt?opt:(void *)1;

// subclass test.
#define TYPE_CAST_CASE(T,T2)						\
	{	void * rpt;	if ((rpt = TYPE_OBJECT(T2).cast(opt,cast_type))) 			\
		return (T *)((T2 *)rpt); }
#define TYPE_CAST_END(T)						\
	return 0; }

#define APPLY_DEF(cname)						\
void cname::__apply( HeapOps op) {
#define END_APPLY_DEF(cname) }

/* define the functions  of the interface class itself that need to
	be defined for all classes. */
#define COMMON_FCT_DECLS(T)						\
	public:								\
	void __apply(HeapOps);						\
	static void sdl_apply(void *p,HeapOps op) 			\
	{	((T*)p)->__apply(op);}					\
	void * operator new(unsigned int);				\
	void * operator new(unsigned int, void * p) { return p;}	\
	public:								\
	/* void   operator delete(void *)	; */				\
	void *  operator new(unsigned int, const char *path, mode_t mode);\
	void *  operator new(unsigned int, Ref<Pool> pool);		\
	virtual const void * get_top() const;				\
	virtual const void * isa(rType *) const;			\
	REF(T)	get_ref() const	{ return REF(T)((T *)get_top());}	\
	/* T * update() const { get_ref().update(); return(T *)this;}*/	\
	/* should be ifdef nopin, but no easy way to do that...      */ \
	/* WRefPin<T>  update() const { return get_ref().update();} */	\
	/* go back to T* version since pinning might not be done.	*/	\
	 T * update() const { get_ref().make_writable(); return(T *)this;}	\
	rType * get_type() const { rType *rv; get_ref().eval(rv);	\
		return rv;}						\

// the main purpose for the sdlObj common base class is to force the virtual
// function pointer to be at the beginning of each real class.
// we may wish to add other stuff here later though.
class sdlObj// define virtuals common to all fcts;
{
public:
	virtual const void * get_top() const;
	virtual const void * isa(rType *) const;
};
		
#define INST_STRING_FCT_DECLS(string_attr)				\
	void get_string(StringRec &srec){				\
	    srec.string = string_attr.string;				\
	    srec.length = string_attr.length;				\
	}								\
	void set_string(StringRec &srec){				\
	    string_attr.string = srec.string;				\
	    string_attr.length = srec.length;				\
	}

// some additonal stuff:
#define SDL_ref(T) REF(T)
// dubious lref def; worry about this!
#define SDL_lref(T) T *
#define LREF(T) T *

/* the following section implements relationship types for the c++ binding.
   Relationships end up being just ref/sets with some additional operator
   overloading/ member overriding..
*/


#define SET_INV(T,TINV,TC_OFFSET)  SetInv<T,TINV,TC_OFFSET>
#define BAG_INV(T,TINV,TC_OFFSET) BagInv<T,TINV,TC_OFFSET>
#define REF_INV(T,TINV,TC_OFFSET) RefInv<T,TINV,TC_OFFSET>
// declaration of ref with inverse for sdl relatonship in, e.g.
//interface TCONT {
//	relationship ref<T>  a inverse TINV;
//}
// e.g. a relationship where the inverse side is the TINV field of 
// interface T, and the containing interface is TCONT.
#define REF_INV_DECL(T,TINV,TCONT,TC_OFFSET)                            \
RefInv<T,TCONT,TC_OFFSET>
#define SET_INV_DECL(T,TINV,TCONT,TC_OFFSET)                            \
SetInv<T,TCONT,TC_OFFSET>
/* also define the equivalent for bags (sets with duplicates */
#define BAG_INV_DECL(T,TINV,TCONT,TC_OFFSET)                            \
BagInv<T,TCONT,TC_OFFSET>
		
// implementation of operator= for REF_INV class
// this one needs the implementation to be sep. from definition 
// 3 things: get rid of backpointer to "this" class in old inverse;
// place new backpointer in new inverses;
// update the invers field
#define REF_INV_IMPL(T,TINV,TCONT,TOFF)					\
template class RefInv<T,TCONT,TOFF>;					\
void REF_INV(T,TCONT,TOFF)::operator=(const REF(T)  &Targ)		\
{									\
	REF(TCONT) cont_ref = REF(TCONT)(get_contpt());			\
	/* cont_ref is a REF pointing to the obj containing "this". */	\
	if (*this!=NULL) /* need to update other side */		\
		(*this).update()->TINV.del_from_inverse(cont_ref);	\
	if (Targ!=NULL) /* use inheritance, this is like */		\
		(Targ).update()->TINV.add_from_inverse(cont_ref);	\
	this->REF(T)::operator=(Targ);					\
} /* end of REF_INV_IMPL macro */
/* add and del must have sep. implementation */
/* similar to ref inverse, but use add and del as 2 parts */
/* del deletes "this" from the inverse side; add adds "this" */
/* to the inverse side */
#define SET_INV_IMPL(T,TINV,TCONT,TC_OFFSET)					\
template class SetInv<T,TCONT,TC_OFFSET>;				\
void SET_INV(T,TCONT,TC_OFFSET)::add(REF(T)  arg) {			\
	if (arg != NULL) {						\
		Set<Ref<T> >::add(arg);					\
		arg.update()->TINV.add_from_inverse(REF(TCONT)(get_contpt()));\
	}								\
};									\
/* del perhaps needs some error checking */				\
void SET_INV(T,TCONT,TC_OFFSET)::del(REF(T)  arg)	{			\
	if (arg!= NULL)							\
	{								\
		Set<Ref<T> >::del(arg);					\
		arg.update()->TINV.del_from_inverse(REF(TCONT)(get_contpt()));\
	}								\
};									\
REF(T) SET_INV(T,TCONT,TC_OFFSET)::delete_one(){ 				\
	Ref<T> delt = Set<Ref<T> >::delete_one();				\
	if (delt != NULL)						\
	    delt.update()->TINV.del_from_inverse(REF(TCONT)(get_contpt()));\
	return delt;							\
};  /* end of SET_INV_IMP macro */
// and similarly for bags.
#define BAG_INV_IMPL(T,TINV,TCONT,TC_OFFSET)					\
template class BagInv<T,TCONT,TC_OFFSET>; 				\
void BAG_INV(T,TCONT,TC_OFFSET)::add(REF(T)  arg) {			\
	if (arg != NULL) {						\
		Bag<Ref<T> >::add(arg);					\
		arg.update()->TINV.add_from_inverse(REF(TCONT)(get_contpt()));\
	}								\
};									\
/* del perhaps needs some error checking */				\
void BAG_INV(T,TCONT,TC_OFFSET)::del(REF(T)  arg)	{			\
	if (arg!= NULL)							\
	{								\
		Bag<Ref<T> >::del(arg);					\
		arg.update()->TINV.del_from_inverse(REF(TCONT)(get_contpt()));\
	}								\
};									\
REF(T) BAG_INV(T,TCONT,TC_OFFSET)::delete_one(){ 				\
	REF(T) delt; delt.init(Bag<Ref<T> >::delete_one()); 			\
	if (delt != NULL)						\
		delt.update()->TINV.del_from_inverse(REF(TCONT)(get_contpt()));\
	return delt;							\
};  /* end of BAG_INV_IMP macro */
#endif
