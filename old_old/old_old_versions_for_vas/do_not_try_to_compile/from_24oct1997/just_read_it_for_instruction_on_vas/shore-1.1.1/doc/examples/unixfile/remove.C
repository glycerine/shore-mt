/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
// unlink a unix file or directory created by one 
// of the other example programs in this directory
//		unlink <path>
*/

#include <stream.h>
#include <iostream.h>
#include <iomanip.h>
#include <string.h>
#include <ShoreApp.h>
#include <ShoreStats.h>
#include <sdl_UnixFile.h>

void
usage(ostream &os, char *progname)
{
    os << "Usage: " << progname << " path [path] " << endl;
    exit(1);
}


int
perror(shrc rc, bool augment, int line, const char *file) 
{
	int e = Shore::oc_unix_error(rc);
	if(augment) {
		rc.add_trace_info(__FILE__, __LINE__);
		cerr << rc << endl;
		cerr << "(replaced by " << e 
			<< ": " << Shore::perror(e) << ")" << endl;
	}
	return e;
}

int
uerror(shrc rc, bool augment, int line, const char *file) 
{
	int e = Shore::oc_unix_error(rc);
	if(augment) {
		RC_PUSH(rc, e);
		rc.add_trace_info(__FILE__, __LINE__);
		cerr << rc << endl;
	}
	return e;
}

#ifdef DEBUG
#define PERROR(rc) perror(rc, true, __LINE__, __FILE__)
#else
#define PERROR(rc) perror(rc, false, 0, 0);
#endif

#undef PERROR

#define PERROR(rc) uerror(rc, true, __LINE__, __FILE__)

int
doremove(const char *fname) 
{
	OStat	statbuf;
	shrc rc;

	rc = Shore::stat(fname, &statbuf);
	if(rc) { return PERROR(rc); }

	if(statbuf.type_loid == ReservedOid::_Directory) {
		rc = Shore::rmdir(fname);

	}else if(statbuf.type_loid == ReservedOid::_Pool) {
		rc = Shore::rmpool(fname);

	} else {
		rc = Shore::unlink(fname);
	}
	if(rc) return PERROR(rc);
	return 0;
}

int
main(int argc, char *argv[])
{
    char 	*progname = argv[0];
    shrc 	rc;

    if(argc < 2) {
		usage(cerr, progname);
    }
	for(int i=argc-1; i>0; i--) {
		if(strlen(argv[i])<2){
			usage(cerr, progname);
		}
	}

    SH_DO(Shore::init(argc, argv));

    SH_BEGIN_TRANSACTION(rc);
    if(rc){
		cerr << rc << endl;
		return 1;
    } else {
		SH_DO(Shore::chdir("/"));
		int e;
		for(int i=1; i<argc; i++) {
			if(e=doremove(argv[i])) {
				cerr << argv[i] << " not removed: " 
				<< Shore::perror(e) << endl;
			}
		}
		SH_DO(SH_COMMIT_TRANSACTION);
    }
    return 0;
}
