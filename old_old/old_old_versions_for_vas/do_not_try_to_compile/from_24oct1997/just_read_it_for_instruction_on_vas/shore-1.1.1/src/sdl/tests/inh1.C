/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

///////////////////////////////
// COMPILE ONLY - don't run
///////////////////////////////

#define MODULE_CODE

#include <iostream.h>
#include <fstream.h>
#include <std.h>
#include "ShoreApp.h"
#include "inh1.h"

void
usage(ostream &/*os*/, char */*progname*/)
{
    exit(1);
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
	SH_DO(Shore::unlink(fname));
	SH_DO(SH_COMMIT_TRANSACTION);
    }
}

// Ref<one> o_ref;
Ref<three> o_ref;

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

	    // Try with two different class methods:
	    SH_DO( REF(three)::new_persistent (fname, 0644, o_ref) ) ;
	    SH_DO( REF(one)::new_persistent (fname, 0644, o_ref) ) ;

	    if(!o_ref ) {
		cerr << "Cannot create new objects " << fname << endl;
		ok = false;
	    }
	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }

    destroy_obj(fname);
    return 0;
}
