/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#define MODULE_CODE

#include <iostream.h>
#include <fstream.h>
#include <std.h>
#include "ShoreApp.h"
#include "inheritance.h"

void
usage(ostream &/*os*/, char */*progname*/)
{
    exit(1);
}

char *one::me() const { return "one"; }
char *other::whatis(Ref<one>r) const { return r->me(); }

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
	SH_DO(Shore::unlink(fname));
	SH_DO(SH_COMMIT_TRANSACTION);
    }
}

Ref<three> a_ref;
Ref<other> o_ref;

int
main(int argc, char *argv[])
{
    char 	*progname = argv[0];
    char 	*fname;
    shrc 	rc;

    if(argc != 1){
	usage(cerr, progname);
    }

    // Establish a connection with the vas and initialize 
    // the object  cache.
    SH_DO(Shore::init(argc, argv));

    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
    } else {
	// The main body of the transaction goes here.

	SH_DO(Shore::chdir("/"));

	//////////////////////////////////
	// GUTS HERE
	//////////////////////////////////
	fname = "XXX";
	{
	    bool ok = true;

	    SH_DO( REF(other)::new_persistent (fname, 0644, o_ref) ) ;
	    if(!o_ref ) {
		cerr << "Cannot create new objects " << fname << endl;
		ok = false;
	    } else {
		SH_DO( REF(three)::new_persistent ("a_ref_junk", 0644, a_ref) ) ;
		if(!a_ref ) {
		    cerr << "Cannot create new objects " << "a_ref_junk" << endl;
		    ok = false;
		} 
	    }

	    cerr << o_ref->whatis(a_ref) << endl;
	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }

    destroy_obj(fname);
    destroy_obj("a_ref_junk");
    return 0;
}
