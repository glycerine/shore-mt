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
#include "pool_destroy.h"

void
usage(ostream &/*os*/, char */*progname*/)
{
    exit(1);
}

#define PERROR(rc) { if(rc && rc.err_num()!=SH_NotFound) {\
	cerr << __LINE__ << " " <<__FILE__<<":"<<endl;\
	cerr << rc << endl; if(rc = RCOK);\
	} }


void
destroy_pool(const char *fname)
{
    Ref<Pool> p;
    shrc rc;

    cerr << "destroying pool " << fname << endl;
    // 
    // Proper way to destroy a pool
    //
    SH_BEGIN_TRANSACTION(rc);
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return;
    } else {
	// Correct way to do it:
	W_IGNORE(Ref<Pool>::lookup (fname, p)) ;
	W_IGNORE(p.destroy_contents());
	W_IGNORE(Shore::unlink(fname));
	SH_DO(SH_COMMIT_TRANSACTION);
    }
}
int
main(int argc, char *argv[])
{
    char 	*progname = argv[0];
    char 	*fname = "pool_4_destroy";
    shrc 	rc;
    Ref<Pool> pool;
    Ref<IndexObj> w;

    if(argc >1 ) {
	usage(cerr, progname);
    }

    SH_DO(Shore::init(argc, argv));

    destroy_pool(fname);

    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
    } else {
	// The main body of the transaction goes here.

	SH_DO(Shore::chdir("/"));

	/////////////////////////////////////////////////
	// create a pool and one anonymous object containing
	// an index
	/////////////////////////////////////////////////
	{

	    SH_DO( REF(Pool)::create(fname, 0755, pool) );
	    SH_DO( REF(IndexObj)::new_persistent (pool, w) ) ;

	    if(!w) {
		cerr << "Cannot create " << fname << endl;
	    } else {
		rc= w->indx.init(UniqueBTree);
		PERROR(rc);
	    }
	    cerr << fname << " created. " << endl;
	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }
    destroy_pool(fname);

    return 0;
}
