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
#include "array.h"

void
opi::init() 
{
}

main (int argc, char **argv) 
{
	shrc rc;

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
		

	//  see PR 250

		// Try to materialize a (hopefully) bogus oid:
		//
		LOID l(0,10,33333);

		char * bogus = "0.0.0.0:10.33333";

		istrstream i(bogus, strlen(bogus));
		LOID k;
		i >> k;

		assert(k==l);
		Ref<any> o(l);
		Ref<any> w = l;
		Ref<attri> o_ref;


		// ARRAY 
		long j;
		o_ref.update()->an_array[0] = 1; 
		j = o_ref->an_array[0]; 

		// What can we do with a ref<any>???
		Ref<opi> o1 = (Ref<opi>&)o;
		WRef<opi> w1 = (WRef<opi>&)w;

		w1->init();
		o1.update()->init();

		SH_DO(SH_COMMIT_TRANSACTION);
	}
}
