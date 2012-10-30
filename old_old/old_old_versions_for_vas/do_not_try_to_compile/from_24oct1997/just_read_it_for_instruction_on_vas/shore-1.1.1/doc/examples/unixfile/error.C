/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
// demonstrate various kinds of 
// error-handling
// These examples are used in the manual page
// errors(cxxlb)
*/

#include <stream.h>
#include <ctype.h>
#include <iostream.h>
#include <iomanip.h>
#include <string.h>
#include <ShoreApp.h>
#include <sdl_UnixFile.h>

#define PERROR(rc) { if(rc) {\
	cerr << __LINE__ << " " <<__FILE__<<":"<<endl;\
	cerr << rc << endl; if(rc = RCOK);\
	} }

shrc
dostat0(const char *fname) 
{
	OStat	statbuf;
	shrc rc;

	rc = Shore::stat(fname, &statbuf);
	SH_NEW_ERROR_STACK(rc);
	PERROR(rc);
	return RCOK;
}
shrc
dostat1helper(const char *fname) 
{
	OStat	statbuf;
	shrc rc;

	rc = Shore::stat(fname, &statbuf);
	SH_NEW_ERROR_STACK(rc);
	return rc;
}
shrc
dostat1(const char *fname) 
{
	shrc rc = dostat1helper(fname);
	SH_RETURN_ERROR(rc);
}

shrc
dostat2(const char *fname) 
{
	OStat	statbuf;
	shrc rc;

	SH_HANDLE_NONFATAL_ERROR( Shore::stat(fname, &statbuf));
	// exits right away
	return rc;
}

void 
my_error_handler(shrc rc)
{
	cerr << "my_error_handler(): \n" 
		<<  rc << endl;
	cerr << "To debug this with gdb, " 
		<< "put a breakpoint in my_error_handler()." <<endl;
}


shrc
test1 (int argc, char **argv)
{
    shrc 	rc;

	///////////////////////////////////
	// Demonstrate error handling with
	// shrc, adding your own stack trace
	// info to the return code.
	// To show what it does in case
	// of stat() error, give a bad path
	// for a command-line argument
	///////////////////////////////////
	cerr << "***** Test 1 at " << __LINE__
		<< ": stack traces with Shore::stat() error " << endl;

    SH_BEGIN_TRANSACTION(rc);
    if(rc){
		cerr << "No tx because " << rc << endl;
    } else {
		SH_DO(Shore::chdir("/"));
		cerr << "Using dostat 0" << endl;
		int i;
		for(i=2; i<argc; i++) { 
			if(rc=dostat0(argv[i])) SH_RETURN_ERROR(rc);
		}

		cerr << "Using dostat 1" << endl;

		for(i=2; i<argc; i++) { 
			if(rc=dostat1(argv[i])) PERROR(rc);
		}

		cerr << "Using dostat 2" << endl;
		// Unfortunately, if we let any error_handler
		// get called under-the-covers, we exit.
		for(i=2; i<argc; i++) { 
			if(rc=dostat2(argv[i])) PERROR(rc);
		}
		SH_DO(SH_COMMIT_TRANSACTION);
    }
	cerr << "***** end Test 1 " << endl;
	return rc;
}

shrc
test2 ()
{
    shrc 	rc;
	///////////////////////////////////
	// demonstrate catching an error in new()
	// described in
	// .SS "ERRORS IN new() and Ref<T>::new_persistent()"
	///////////////////////////////////
	cerr << "***** Test 2 at " << __LINE__
		<< ": error in new()" << endl;

    SH_BEGIN_TRANSACTION(rc);
    if(rc){
		cerr << "No tx because " << rc << endl;
    } else {
		SH_DO(Shore::chdir("/"));
		{
			char *fname = "/nonexistentdir/junk";
			Ref<SdlUnixFile> o; 		
			o = new (fname, 0755) SdlUnixFile;
			assert(0);
		}
		SH_DO(SH_COMMIT_TRANSACTION);
    }
	cerr << "***** end Test 2 " << endl;
	return rc;
}

shrc
test3 ()
{
    shrc 	rc;
	/////////////////////////////////////////
	// demonstrate catching an error in new_persistent()
	// described in
	// .SS "ERRORS IN new() and Ref<T>::new_persistent()"
	/////////////////////////////////////////
	cerr << "***** Test 3 at " << __LINE__
		<< ": error in new_persistent()" << endl;

    SH_BEGIN_TRANSACTION(rc);
    if(rc){
		cerr << "No tx because " << rc << endl;
    } else {
		SH_DO(Shore::chdir("/"));
		{
			char *fname = "/nonexistentdir/junk";
			Ref<SdlUnixFile> o; 		// const reference
			rc = Ref<SdlUnixFile>::new_persistent(fname, 0755, o);

			SH_NEW_ERROR_STACK(rc);
			if(!o) {
				cerr << "Could not create " << fname << endl;
				PERROR(rc);
			}
		}
		SH_DO(SH_COMMIT_TRANSACTION);
    }
	cerr << "***** end Test 3 " << endl;
	return rc;

}

checkref(
	int line,
	Ref<SdlUnixFile> &o
) 
{
	shrc __rc; 

	__rc = o.valid();
	if(__rc) {
		cerr << line << " invalid SH"  << __rc <<endl;
	} else {
		cerr << line << " valid SH" <<endl;
	} 

	__rc = o.valid(EX);
	if(__rc) {
		cerr << line << " invalid EX"  << __rc <<endl;
	} else {
		cerr << line << " valid EX" <<endl;
	} 

	bool	ok=true;
	__rc = o.is_resident(ok);
	if(__rc) {
		cerr << line << " is_resident err"  << __rc <<endl;
	} else {
		if(!ok) {
			cerr << line << " NOT RESIDENT " <<endl;
		} else {
			cerr << line << " is resident " <<endl;
		}
	} 
}

shrc
test4 ()
{
    shrc 	rc;
	////////////////////////////////////////
	// demonstrate access error : updating a 
	// non-writable object
	// described in
	// .SS "ACCESS PERMISSIONS"
	////////////////////////////////////////
	cerr << "***** Test 4 at " << __LINE__
		<< ": updating a non-writable object" << endl;

    SH_BEGIN_TRANSACTION(rc);
    if(rc){
		cerr << "No tx because " << rc << endl;
    } else {
		SH_DO(Shore::chdir("/"));
		{
			const char *fname = "/readonly";
			Ref<SdlUnixFile> o(0); 		// const reference

			rc = Ref<SdlUnixFile>::new_persistent(fname, 0755, o);
			cerr << "line " << __LINE__ 
				<<  "o=" << (int) o << endl;
			PERROR(rc);
			checkref(__LINE__,o);

			SH_DO(Shore::chmod(fname,0000));
			checkref(__LINE__,o);


			/////////////////////////////////////////
			// preferably check write access first:
			/////////////////////////////////////////

			SH_DO(Shore::access(fname, W_OK, errno));
			if(errno) {
				rc = RC(errno);
				SH_NEW_ERROR_STACK(rc);
				PERROR(rc);
			}


			/////////////////////////////////////////
			// here's what happens if you update or
			// deref anyway
			/////////////////////////////////////////

			cerr << "line " << __LINE__ << endl;

			// assignment
			WRef<SdlUnixFile> w(o); // not w=o;
			rc = Shore::rc();
			PERROR(rc);

			checkref(__LINE__,w);

			w->UnixText.strcat(fname);
			rc = Shore::rc();
			PERROR(rc);
		}
		SH_DO(SH_COMMIT_TRANSACTION);
    }
	cerr << "***** end Test 4 " << endl;
	return rc;
}

shrc
test5 ()
{
    shrc 	rc;
	////////////////////////////////////////
	// demonstrate application checking for
	// null ref, and also library catching error 
	// in dereferencing a null ref in update()
	// described in
	// .SS "DEREFERENCING A BAD Ref<T>"
	////////////////////////////////////////
	cerr << "***** Test 5 at " << __LINE__
		<< ": update() on a NULL ref (fatal,clean)" << endl;

    SH_BEGIN_TRANSACTION(rc);
    if(rc){
		cerr << "No tx because " << rc << endl;
    } else {
		SH_DO(Shore::chdir("/"));
		{
			const char *fname = "/nonexistent/readonly";
			Ref<SdlUnixFile> o(0); 		
			rc = Ref<SdlUnixFile>::lookup(fname, o);
			// lookup does not touch o if error occurs

			////////////////////////////////////////
			// preferably check result:
			////////////////////////////////////////
			if(!o) {
				if(rc) { PERROR(rc); }
				else {
					cerr << "OC gave no error" << endl;
				}
			}

			////////////////////////////////////////
			// demonstrate what happens if you 
			// dereference it, anyway
			////////////////////////////////////////
			cerr << "update() about to occur with MY err handler " << endl;
			o.update()->UnixText.strcat(fname);
		}
		SH_DO(SH_COMMIT_TRANSACTION);
    }
	cerr << "***** end Test 5 " << endl;
	return rc;
}

shrc
test6 ()
{
    shrc 	rc;
	const 	char *fname = "test6";

	////////////////////////////////////////
	// demonstrate inability to catch error in 
	// dereferencing a non-null, garbage ref 
	// with update()
	// described in
	// .SS "DEREFERENCING A BAD Ref<T>"
	////////////////////////////////////////
	cerr << "***** Test 6 at " << __LINE__
		<< ": update() on a bad ref (fatal, unclean)" << endl;

    SH_BEGIN_TRANSACTION(rc);
    if(rc){
		cerr << "No tx because " << rc << endl;
    } else {
		SH_DO(Shore::chdir("/"));
		{
			Ref<SdlUnixFile> o; 		
			o.update()->UnixText.strcat(fname);
		}
		SH_DO(SH_COMMIT_TRANSACTION);
    }
	cerr << "***** end Test 6 " << endl;
	return rc;
}

shrc
test7 ()
{
    shrc 	rc;
	////////////////////////////////////////
	// demonstrate inability to catch error in 
	// dereferencing a bad, non-null ref
	// with operator->()
	// described in
	// .SS "DEREFERENCING A BAD Ref<T>"
	////////////////////////////////////////
	cerr << "***** Test 7 at " << __LINE__
		<< ": dereferencing a bad ref (fatal, unclean)" << endl;

    SH_BEGIN_TRANSACTION(rc);
    if(rc){
		cerr << "No tx because " << rc << endl;
    } else {
		SH_DO(Shore::chdir("/"));
		{
			Ref<SdlUnixFile> o; 		
			const char *c = (const char *) o->UnixText;
			cerr << c << endl;
		}
		SH_DO(SH_COMMIT_TRANSACTION);
    }
	cerr << "***** end Test 7 " << endl;
	return rc;
}

shrc
test8 ()
{
    shrc 	rc;
	////////////////////////////////////////
	// demonstrate catching error in 
	// dereferencing a null ref
	// with operator->()
	// described in
	// .SS "DEREFERENCING A BAD Ref<T>"
	////////////////////////////////////////
	cerr << "***** Test 8 at " << __LINE__
		<< ": dereferencing a null ref (fatal, clean)" << endl;

    SH_BEGIN_TRANSACTION(rc);
    if(rc){
		cerr << "No tx because " << rc << endl;
    } else {
		SH_DO(Shore::chdir("/"));
		{
			Ref<SdlUnixFile> o(0); 		
			const char *c = (const char *) o->UnixText;
		}
		SH_DO(SH_COMMIT_TRANSACTION);
    }
	cerr << "***** end Test 8 " << endl;
	return rc;
}

int
main(int argc, char *argv[])
{
    shrc 	rc;
    if(argc < 2 ||
		strlen(argv[1])<1 ||
		!isdigit(*argv[1])
	) {
		cerr << "Usage: " << argv[0] << " <1-8> [path [path]] " << endl;
		exit(1);
	}
	cerr <<  "Begin" << endl;

    SH_DO(Shore::init(argc, argv));
	// Shore::set_error_handler(my_error_handler);

	switch(atoi(argv[1]))  {
	case 1:
		rc = test1(argc, argv);
		break;
	case 2:
		rc = test2();
		break;
	case 3:
		rc = test3();
		break;
	case 4:
		cerr <<  "before" << endl;
		rc = test4();
		cerr <<  "after" << endl;
		break;
	case 5:
		rc = test5();
		break;
	case 6:
		rc = test6();
		break;
	case 7:
		rc = test7();
		break;
	case 8:
		rc = test8();
		break;
	default:
		cerr << "No test called " << argv[1] << endl;
		exit(1);
	}
	PERROR(rc);
}
