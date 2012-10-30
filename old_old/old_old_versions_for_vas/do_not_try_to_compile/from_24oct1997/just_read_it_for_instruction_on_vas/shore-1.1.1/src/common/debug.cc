/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifdef __ERRLOG_C__

// gcc implementation in errlog.C since it is in #included there

// NB: in order to guarantee order of constructors, we
// have to include this at the end of errlog.cc.

#if defined(__cplusplus)

/* compile this stuff even if -UDEBUG because
 * other layers might be using -DDEBUG
 * and we want to be able to link, in any case
 */

#include <stream.h>
#include <iostream.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "basics.h"
#include "debug.h"

#define MAX_TRACE_IDS 2000

int trace_tab[MAX_TRACE_IDS];

__debug _debug("debug", getenv("DEBUG_FILE"));

#include "regex.posix.h"


#ifdef SUNOS41
extern "C" {
	char *getenv(const char *);
	char *mktemp(const char *);
};
#endif

#ifdef DEBUG
char *_fname_debug_ = "Function compiled without -DDEBUG";
#else
//don't forever print out "Function compiled without -DDEBUG"
char *_fname_debug_ = "";
#endif

bool		__debug::re_ready = false;
regex_t		__debug::re_posix_re;
char*		__debug::re_error_str = "Bad regular expression";

__debug::__debug(char *n, char *f) : ErrLog(n, log_to_unix_file, f?f:"-")
{
	//re_ready = false;
	//re_error_str = "Bad regular expression";
	re_syntax_options = RE_NO_BK_VBAR;

	mask = 0;
	_flags = getenv("DEBUG_FLAGS");
	// malloc the flags so it can be freed
	if(!_flags) {
		_flags = "";
		mask = _none;
	}  else {
#ifdef HPUX8
		// since regexec seems buggy in hpux, no debugging output
		// will be available (see re_exec_hpux() defn. above).
		// So, print a message warning the user.
		cerr << endl << "WARNING WARNING WARNING" << endl << endl;
		cerr << "The DEBUG_FLAGS environment variable is ignored" << endl;
		cerr << "due to a bug in HP's regular expression" << endl;
		cerr << "library.  See common/debug.C for more info." << endl;
#endif
	}

	if(!strcmp(_flags,"all")) {
		mask |= _all;
	} else if(!none()) {
		char *s;
		if((s=re_comp_debug(_flags)) != 0) {
			if(strstr(s, "No previous regular expression")) {
				// this is ok
			} else {
				cerr << "Error in regex, flags not set: " <<  s << endl;
			}
			mask = _none;
		}
	}

	// make a copy of the flags so we can delete it later
	char *temp;
	temp = new char[strlen(_flags)+1];
	strcpy(temp, _flags);
	_flags = temp;
	assert(_flags != NULL);

	char* trace_level = getenv("TRACE_LEVEL");
	_trace_level = (trace_level ? atoi(trace_level) : min_int4);

	for (int i = 0; i < MAX_TRACE_IDS; i++) trace_tab[i] = max_int4;

	char* trace_filename = getenv("TRACE_FILE");
	if (trace_filename) {
	    char tmp[100];
	    ifstream trace_file(trace_filename, ios::in);
	    int ntraces = 0;
	    trace_file >> ntraces;
	    // cout << "ntraces = " << ntraces << endl;
	    for (int i = 0; i < ntraces; i++) {
		int id = 0;
		trace_file >> id;
		trace_file >> trace_tab[id];
		// cout << '(' << id << ", " << trace_tab[id] << ')' << endl;
		trace_file.getline(tmp, 100);
	    }
	}
	assert( !( none() && all() ) );
}

__debug::~__debug()
{
	if(_flags) delete [] _flags;
	_flags = NULL;

}

void
__debug::setflags(const char *newflags)
{
	if(!newflags) return;
	{
		char *s;
		if((s=re_comp_debug(newflags)) != 0) {
			cerr << "Error in regex, flags not set: " <<  s << endl;
			mask = _none;
			return;
		}
	}

	mask = 0;
	if(_flags) delete []  _flags;
	_flags = new char[strlen(newflags)+1];
	strcpy(_flags, newflags);
	if(strlen(_flags)==0) {
		mask |= _none;
	} else if(!strcmp(_flags,"all")) {
		mask |= _all;
	}
	assert( !( none() && all() ) );
}

int
__debug::flag_on(
	const char *fn,
	const char *file
)
{
	assert( !( none() && all() ) );
	if(_flags==NULL) return 0; //  another constructor called this
							// before this's constructor got called. 
	if(none()) 	return 0;
	if(all()) 	return 1;

#ifdef notdef
	if(fn || file) {
		char *s;
		if((s=re_comp_debug(_flags)) != 0) {
			cerr << "Error in regex: " <<  s << endl;
			mask = _none;
			return 0;
		}
	}
#endif

	if(file && re_exec_debug(file)) {
		return 1;
	}
	if(fn && re_exec_debug(fn)) {
		return 1;
	}
	// if the regular expression didn't match,
	// try searching the strings
	if(file && strstr(_flags,file)) {
		return 1;
	}
	if(fn && strstr(_flags,fn)) {
		return 1;
	}
	return 0;
}
/* This function prints a hex dump of (len) bytes at address (p) */
void
__debug::memdump(void *p, int len)
{
	register int i;
	char *c = (char *)p;
	
	clog << "x";
	for(i=0; i< len; i++) {
		W_FORM(clog)("%2.2x", (*(c+i))&0xff);
		if(i%32 == 31) {
			clog << endl << "x";
		} else if(i%4 == 3) {
			clog <<  " x";
		}
	}
	clog << "--done" << endl;
}
#endif
#endif /* __ERRLOG_C__ */
