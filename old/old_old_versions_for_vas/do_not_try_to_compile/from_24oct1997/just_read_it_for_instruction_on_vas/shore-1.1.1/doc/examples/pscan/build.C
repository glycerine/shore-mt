/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// build.C
//

#include <iostream.h>
#include <string.h>
#include "part.h"

#define DEF_POOLNAME	"pool"

void usage(ostream &os, char *progname)
{
    os << "Usage: " << progname << " nparts [poolname]" << endl;
}

main(int argc, char *argv[])
{
    char *progname = argv[0];
    char *poolname;
    int nparts;
    shrc rc;

    if(argc == 2 || argc == 3){
	if(sscanf(argv[1], "%d", &nparts) != 1){
	    usage(cerr, progname);
	    return 1;
	}
	if(argc == 3)
	    poolname = argv[2];
	else
	    poolname = DEF_POOLNAME;
    }
    else{
	usage(cerr, progname);
	return 1;
    }

    // Establish a connection with the vas and initialize the object
    // cache.
    SH_DO(Shore::init(argc, argv, progname, Shore::default_config_file));

    // Begin a transaction.
    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
	cerr << rc << endl;
	return 1;
    }

    // The main body of the transaction goes here.
    else{

	int i;
	REF(Part) part;
	REF(Pool) pool;

	// Lookup the pool using its pathname.  Create a new pool if
	// there isn't one already.
	rc = REF(Pool)::lookup(poolname, pool);
	if(rc.err_num() == SH_NotFound)
	    rc = REF(Pool)::create(poolname, 0755, pool);

	if(rc != RCOK)
	    SH_ABORT_TRANSACTION(rc);

	// Create "nparts" part objects.
	for(i = 0; i < nparts;++i){
	    SH_DO(REF(Part)::new_persistent(pool, part));
	    part.update()->init(i);
	}

	// Commit the transaction.
	SH_DO(SH_COMMIT_TRANSACTION);

	cout << "Created " << nparts
	     << ((nparts == 1) ? " part" : " parts") << endl;
    }
}
