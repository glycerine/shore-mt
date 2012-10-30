/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// destroy.C
//

#include <iostream.h>
#include <string.h>
#include "part.h"

#define DEF_POOLNAME	"pool"

void usage(ostream &os, char *progname)
{
    os << "Usage: " << progname << " [poolname]" << endl;
}

main(int argc, char *argv[])
{
    char *progname = argv[0];
    char *poolname;
    shrc news;

    // get the name of the pool
    if(argc == 1)
	poolname = DEF_POOLNAME;
    else if(argc == 2)
	poolname = argv[1];
    else{
	usage(cerr, progname);
	return 1;
    }

    // Establish a connection with the vas and initialize the object
    // cache.
    SH_DO(Shore::init(argc, argv, progname, Shore::default_config_file));

    // Begin a transaction.
    SH_BEGIN_TRANSACTION(news);

    // If that failed, or if a transaction aborted...
    if(news){
	cerr << news << endl;
	return 1;
    }

    // The main body of the transaction goes here.
    else {
	int i=0;
	{  /* open scope for PoolScan */

	    REF(Part) part;
	    REF(any) ref;

	    // Open a scan over the named pool.
	    PoolScan scan(poolname);

	    // If we couldn't open the scan, give up.
	    if(scan != RCOK) {
		news = scan.rc();
		goto done;
	    }

	    // Scan until end-of-scan or an error is encountered.
	    // Note: by default, next() does not fetch the object into the
	    // cache; this is the behavior we want here.  Contrast this
	    // for-loop with the corresponding loop in pscan.C.

	    for(i = 0; scan.next(ref) == RCOK; ++i){

		// Make sure the object really is a part.
		part = TYPE_OBJECT(Part).isa(ref);
		if(part == 0){
		    cerr << "Object is not a Part" << endl;
		    continue;
		}

		// Destroy the object.
		SH_DO(part.destroy());
	    }
	}

done:
	if(news) {
	    SH_ABORT_TRANSACTION(news);
	} else {
	    // Commit the transaction.
	    SH_DO(SH_COMMIT_TRANSACTION);

	    cout << "Destroyed " << i
		 << "part" << ((i == 1) ? " " : "s ") 
		 << endl;
	}


    }

}
