/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <iostream.h>
#include <fstream.h>
#include <std.h>
#include "ShoreApp.h"
#include <sdl_UnixFile.h>
#define MODULE_CODE
#include "isa.h"

# define dassert(ex)\
    {if (!(ex)){(void)fprintf(stderr,"Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);exit(1);}}


rType		*tu;
rType		*tt;
rType		*ts;

void
analyze(const Ref<any> &a, const char *name) 
{
	Ref<T>  	t1;
	Ref<U>  	u1;
//	Ref<Pool>  	p1;
	rType		*typ;
	shrc		rc;

	if(rc = a.get_type(typ)) {
		cerr << "error in get_type: " << rc << endl;
	}
	// The following doesn't work because isa is a method of
	// srt_type<T>, not of rType
	//
	//?? dassert(typ->isa(a));

// dassert(TYPE_OBJECT(any).isa(a));

	// Is it a U?  Is it a U?
	if(u1 = TYPE_OBJECT(U).isa(a)) {
		cerr << name << " is a U" << endl;
		dassert(u1 == TYPE_OBJECT(U).isa(u1));
	} else if(t1 = TYPE_OBJECT(T).isa(a)) {
		cerr << name << " is a T" << endl;
		dassert(t1 == TYPE_OBJECT(T).isa(t1));
// PR: these are missing for Pool
//	} else if(p1 = TYPE_OBJECT(Pool).isa(a)) {
//		cerr << name << " is a Pool" << endl;
	}
}

void
destroy_obj(const char *fname) 
{
	shrc rc;
    // 
    // Another transaction to destroy the file...
    //
    SH_BEGIN_TRANSACTION(rc);
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return;
    } else {
	SH_DO(Shore::chdir("/"));
	Ref<any> anyref;
	shrc e = Ref<any>::lookup(fname, anyref);
	if(e == 0) {
		// cerr << "destroying " << fname << endl;
	    e = Shore::unlink(fname);
		if(e) {
			cerr << "error on unlink(" <<fname<<") :" << rc << endl;
		}
	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }
}

main (int argc, char **argv) 
{
	shrc rc;

    // Establish a connection with the vas and initialize 
    // the object  cache.
    SH_DO(Shore::init(argc, argv));

	destroy_obj("T");
	destroy_obj("U");

    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
	} else {
		// The main body of the transaction goes here.
		SH_DO(Shore::chdir("/"));

		///////////////////// create t, u ////////////////
		Ref<T>	t;
		Ref<U>  u;

		rc = Ref<T>::lookup("T", t);
		if(rc != 0 && rc.err_num() == SH_NotFound) {
			rc = Ref<T>::new_persistent("T", 0755, t);
		}
		if(rc != 0) {
			cerr << "Cannot make " << "T" << rc << endl;
			exit(1);
		}
		// get the rType for t
		if(rc = t.get_type(tt)) {
			cerr << __LINE__ << "error in get_type: " << rc << endl;
			exit(1);
		}
		////////////////////////////////////////////////
		rc = Ref<U>::lookup("U", u);
		if(rc != 0 && rc.err_num() == SH_NotFound) {
			rc = Ref<U>::new_persistent("U", 0755, u);
		}
		if(rc != 0) {
			cerr << "Cannot make " << "U" << rc << endl;
			exit(1);
		}
		if(rc = u.get_type(tu)) {
			cerr << __LINE__ << "error in get_type: " << rc << endl;
			exit(1);
		}

		///////////// compare types /////////////////
		if(tt == tu) {
			cerr << "rType for T == rType for U" <<endl;
		}
		if(&(TYPE_OBJECT(T)) != tt) {
			cerr << "rType for T != TYPE_OBJECT(T)" << endl;
		}
		if(&(TYPE_OBJECT(U)) != tu) {
			cerr << "rType for U != TYPE_OBJECT(U)" << endl;
		}
// PR: these are missing for Pool
//		if(&(TYPE_OBJECT(Pool)) == tu) {
//			cerr << "rType for U == TYPE_OBJECT(Pool)" << endl;
//		}

		// obvious assertions with rTypes
		dassert(u->isa(tu));
		dassert(! u->isa(tt));
		dassert(t->isa(tt));
		dassert(! t->isa(tu));

		// obvious assertions with Refs
		dassert(TYPE_OBJECT(U).isa(u));
		dassert(! TYPE_OBJECT(U).isa(t));
		dassert(TYPE_OBJECT(T).isa(t));
		dassert(! TYPE_OBJECT(T).isa(u));

		////////////// materialize through  Ref<any> ///////////

		Ref<any>  	a;
		rc = Ref<any>::lookup("T", a);
		if(rc) {
			cerr << "Cannot find " << "T" << endl;
			exit(1);
		}
		analyze(a, "T");

		rc = Ref<any>::lookup("U", a);
		if(rc) {
			cerr << "Cannot find " << "U" << endl;
			exit(1);
		}
		analyze(a, "U");


		SH_DO(SH_COMMIT_TRANSACTION);
	}

	destroy_obj("T");
	destroy_obj("U");
}

