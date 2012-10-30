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
#include "union2.h"

void print_union(Ref<I> p) {
	switch(p->tv.get_tag()) {
		case REP_INT:
			cout << "integer " << p->tv.get_int_val() << endl;
			break;
		case REP_STRING:
			cout << "string " << p->tv.get_string_val() << endl;
			break;
		default:
			cout << "??? unknown type" << endl;
	}
}
bool 	hack = true;
shrc 	rc;

int main(int argc, char *argv[]) {
	{

	SH_DO(Shore::init(argc, argv));

#ifndef notdef
	    // for the purpose of testing automagically, we can't
	    // have any arguments
	    if (argc != 3) {
		    hack = true;
		    argv[1] = "3";
		    argv[2] = "three";
	    }
#else
	    hack = false;
	    if (argc != 3) {
		    cerr << "usage: " << progname << " test int_val string_val" << endl;
		    return 1;
	    }
#endif

	SH_BEGIN_TRANSACTION(rc);

	if(rc) {
		    // could be because the tx was aborted
		    exit(1);
	    }

	    Ref<I> ip, sp;

	    rc = Ref<I>::lookup("an_int", ip);

	    if (rc != RCOK) {
		    if(!hack) {
			    cout << "First time" << endl;
		    } else {
			    cout << "previous int: ";
			    cout << "integer " << 3 << endl;
			    cout << "previous string: ";
			    cout << "string " << "three" << endl;
		    }

		    SH_DO( Ref<I>::new_persistent ("an_int", 0644, ip) ) ;
		    if (!ip) {
			    cerr << "Cannot create registered object `an_int'" << endl;
			    SH_DO(SH_COMMIT_TRANSACTION);
		    }
		    SH_DO( Ref<I>::new_persistent ("a_string", 0644, sp) ) ;
		    if (!sp) {
			    cerr << "Cannot create registered object `a_string'" << endl;
			    SH_ABORT_TRANSACTION(rc);
		    }
	    }
	    else {
		    SH_DO(Ref<I>::lookup("a_string", sp));

		    cout << "previous int: ";
		    print_union(ip);
		    cout << "previous string: ";
		    print_union(sp);
		    if (strcmp(argv[2],"remove") == 0) {
			    SH_DO(Shore::unlink("an_int"));
			    SH_DO(Shore::unlink("a_string"));
			    SH_DO(SH_COMMIT_TRANSACTION);
			    goto done;
		    }
	    }

	    ip.update()->tv.set_tag(REP_INT);
	    ip.update()->tv.set_int_val() = atoi(argv[1]);

	    sp.update()->tv.set_tag(REP_STRING);
	    sp.update()->tv.set_string_val() = argv[2];
		    

	    SH_DO(SH_COMMIT_TRANSACTION);
	}
done:
    return 0;
}

