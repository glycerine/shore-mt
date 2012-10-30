/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// pscan.C
//

#include <iostream.h>
#include <string.h>
#include "part.h"

#define DEF_POOLNAME	"pool"
#define STRINGLEN	80

void usage(ostream &os, char *progname)
{
    os << "Usage: " << progname << " [poolname]" << endl;
}

main(int argc, char *argv[])
{
    char *progname = argv[0];
    char *poolname;
    shrc news;
    int i;

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
    else{
	{  // open a new scope for the PoolScan object.

	    REF(Part) part;
	    REF(any) ref;
	    long partid;
	    char name[STRINGLEN];

	    PoolScan scan(poolname);
	    // Open a scan over the named pool.

	    // If we couldn't open the scan, give up.
	    if(scan != RCOK) {
		news = scan.rc().delegate();
		goto done;
	    }

	    // Scan until end-of-scan or error.
	    // Note: by default, next() does not fetch the object into the
	    // cache.  Since we want to print some of the fields of each
	    // object, we override the default behavior by passing "true"
	    // for the "fetch" parameter.

	    for(i = 0; scan.next(ref, true) == RCOK; ++i){

		// Make sure the object really is a part.
		part = TYPE_OBJECT(Part).isa(ref);
		if(part == 0){
		    cerr << "Object is not a Part" << endl;
		    continue;
		}

		// Print the fields of the object.
		partid = part->get_partid();
		part->get_name(name, STRINGLEN);
		cout << partid << ", " << name << endl;

		part->check();
	    }
	}

done:
	if(news) {
 	    SH_ABORT_TRANSACTION(news);
	} else  {
	    // Commit the transaction.
	    SH_DO(SH_COMMIT_TRANSACTION);

	    cout << "Found " << i
		 << ((i == 1) ? " part" : " parts") << endl; 
	}
    }
}
