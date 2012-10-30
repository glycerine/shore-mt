/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <iostream.h>
#include <fstream.h>
#include <std.h>
#include "ShoreApp.h"
#include <sdl_UnixFile.h>
#define MODULE_CODE
#include "volid.h"

VolId				v1, v2;

void
test(Ref<any> &a)
{
	a = 0;
    LOID l;

    char *s = "0";
    istrstream i(s, strlen(s));
    i >> l;
    a = l;
	cerr << "test" <<endl;
    if(a) {
        cerr << a << " is NOT null" <<endl;
    } else {
        cerr << a << " is null" <<endl;
    }
}

main (int argc, char **argv) 
{
	shrc rc;

#if 0
	// now, do what was formerly in /oo7/small in /types instead.
	cerr << "For this test to run, your server must have created " << endl
		<< "/oo7/small, etc. -- set prepoo7 to 1 in your shore.rc." 
		<<endl;
#endif

    // Establish a connection with the vas and initialize 
    // the object  cache.
    SH_DO(Shore::init(argc, argv));


	///////////////////////////////////////////////////////
	// VOLIDS
	///////////////////////////////////////////////////////
    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
	} else {
		Ref<SdlUnixFile>	u1, u2;

		test(u1);

		const char *fname	="x";

		SH_DO(Shore::chdir("/"));
		rc = Ref<SdlUnixFile>::lookup(fname, u1);
		if(rc != 0 && rc.err_num() == SH_NotFound) {
			rc = Ref<SdlUnixFile>::new_persistent(fname, 0755, u1);
		}
		if(rc != 0) {
			cerr << "Cannot make " << fname << endl;
			exit(1);
		}
		SH_DO(Shore::chdir("/types"));
		rc = Ref<SdlUnixFile>::lookup(fname, u2);
		if(rc != 0 && rc.err_num() == SH_NotFound) {
			rc = Ref<SdlUnixFile>::new_persistent(fname, 0755, u2);
		}
		if(rc != 0) {
			cerr << "Cannot make " << fname << endl;
			exit(1);
		}

		SH_DO(u1.get_primary_volid(v1));
		SH_DO(u2.get_primary_volid(v2));

		SH_DO(SH_COMMIT_TRANSACTION);

		cerr << v1 << " " << v2 << endl;
		if(v1 != v2) {
			cerr << "differ" <<endl;
		} else {
			cerr << "same" << endl;
		}

	}

	///////////////////////////////////////////////////////
	// TYPE OBJECTS
	///////////////////////////////////////////////////////
    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
	} else {
		Ref<SdlUnixFile>	u1, u2;
		LOID				l1, l2;
		rType				*t1, *t2;
		const char *fname	="x";

		SH_DO(Shore::chdir("/"));
		rc = Ref<SdlUnixFile>::lookup(fname, u1);
		if(rc != 0 && rc.err_num() == SH_NotFound) {
			rc = Ref<SdlUnixFile>::new_persistent(fname, 0755, u1);
		}
		if(rc != 0) {
			cerr << "Cannot make " << fname << endl;
			exit(1);
		}
		SH_DO(Shore::chdir("/types"));
		rc = Ref<SdlUnixFile>::lookup(fname, u2);
		if(rc != 0 && rc.err_num() == SH_NotFound) {
			rc = Ref<SdlUnixFile>::new_persistent(fname, 0755, u2);
		}
		if(rc != 0) {
			cerr << "Cannot make " << fname << endl;
			exit(1);
		}

		SH_DO(u1.get_type(t1));
		SH_DO(u2.get_type(t2));
		SH_DO(u1.get_type(l1));
		SH_DO(u2.get_type(l2));

		if(l1 == l2) {
			cerr << "types same" << endl;
		}
		if(t1 == t2) {
			cerr << "ptrs same" << endl;
		}

		SH_DO(SH_COMMIT_TRANSACTION);

		cerr << v1 << " " << v2 << endl;
		if(v1 != v2) {
			cerr << "differ" <<endl;
		} else {
			cerr << "same" << endl;
		}

	}
}
