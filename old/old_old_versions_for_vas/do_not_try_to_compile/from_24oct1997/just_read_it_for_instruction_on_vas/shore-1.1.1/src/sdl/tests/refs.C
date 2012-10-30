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
#include "refs.h"


# define dassert(ex)\
    {if (!(ex)){(void)fprintf(stderr,"Assertion failed: file \"%s\", line %d\n", __FILE__, __LINE__);exit(1);}}

void cleanup();

main (int argc, char **argv) 
{
	Ref<atype> a;
	Ref<btype> b;
	Ref<btype> b_null;

	shrc rc;

    // Establish a connection with the vas and initialize 
    // the object  cache.
    SH_DO(Shore::init(argc, argv));

	cleanup();

    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
		// after longjmp
		cerr << rc << endl;
		return 1;
	} else {
		// The main body of the transaction goes here.

		SH_DO(Shore::chdir("/"));

		SH_DO(a.new_persistent("aref", 0644, a));
		SH_DO(b.new_persistent("bref", 0644, b));

		// set from the ref side 
		{
			a.update()->b_ref = b;
			dassert(a->b_ref == b);

			int i;
			i = b->a_set.get_size();

			dassert(i>=1);
			dassert(b->a_set.get_elt(i-1) == a  );
			dassert(a->b_ref == b);
		}
		
		// clear
		{
			a.update()->b_ref = b_null;
			dassert(a->b_ref != b);
			int i;
			i = b->a_set.get_size();

			dassert(i==0);
			dassert(b->a_set.get_elt(0) != a  );
		}

		// set from the set side 
		{
			b.update()->a_set.add(a);
			// try the same set of assertions
			dassert(a->b_ref == b);
			int i;
			i = b->a_set.get_size();
			dassert(i>=1);
			dassert(b->a_set.get_elt(i-1) == a  );
			dassert(a->b_ref == b);
		}

		// clear
		{
			b.update()->a_set.del(a);
			dassert(a->b_ref != a);
			dassert(a->b_ref == b_null);
			int i;
			i = b->a_set.get_size();

			dassert(i==0);
			dassert(b->a_set.get_elt(0) != a  );
		}

		SH_DO(SH_COMMIT_TRANSACTION);
	}
	cleanup();
}

void
cleanup()
{
	shrc 	rc;

    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
		// after longjmp
		cerr << rc << endl;
		return;
	} else {
		// The main body of the transaction goes here.

		SH_DO(Shore::chdir("/"));

		SH_DO(Shore::unlink("aref"));
		SH_DO(Shore::unlink("bref"));

		SH_DO(SH_COMMIT_TRANSACTION);
	}
}
