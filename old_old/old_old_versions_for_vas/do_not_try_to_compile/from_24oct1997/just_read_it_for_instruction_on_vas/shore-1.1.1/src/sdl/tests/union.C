/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#define MODULE_CODE
/////////////////
// PR 219
////////////////
#include <iostream.h>
#include <fstream.h>
#include <std.h>
#include "ShoreApp.h"
#include "union.h"

# define dassert(ex)\
    {if (!(ex)){(void)fprintf(stderr,"Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);exit(1);}}


void
usage(ostream &/*os*/, char */*progname*/)
{
    exit(1);
}

int
main(int argc, char *argv[])
{
    char 	*progname = argv[0];
    shrc 	rc;

    if(argc >2 || argc < 1){
	usage(cerr, progname);
    }
    char *fname = "union.example";
    //
    // Establish a connection with the vas and initialize 
	// the object  cache.
	//
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
	// create an object
	//////////////////////////////////
	{
	    REF(union_example_t) o;
#ifdef notdef
	    REF(a) aref;
#endif
	    SH_DO( REF(union_example_t)::new_persistent (fname, 0644, o) ) ;
	    if(!o) {
		cerr << "Cannot create new union_example_t." << endl;
	    } else {
			union_t	b;
#ifdef notdef
			union_t c = b;
#endif
			enumx		t = three;

			// transient union:
			b.set_tag(t);
			o.update()->u.set_tag(b.get_utag());

			t = o->u.get_utag();
			dassert(t == b.get_utag());
			dassert(t == three);

#ifdef notdef
			// what happens if we don't set the tag
			// correctly?
			c.set_u_integer() = 3;
			c.set_u_boolean() = false;
			c.set_u_ref() = aref;
#endif
	    }
	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }

    // 
    // Another transaction to destroy the object...
    //
    SH_BEGIN_TRANSACTION(rc);
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
    } else {
	SH_DO(Shore::unlink(fname));
	SH_DO(SH_COMMIT_TRANSACTION);
    }
    return 0;
}
