/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <errlog.h>
#include <assert.h>

typedef void (*testfunc)(ErrLog &);

void test1(ErrLog &e) { 
	e.setloglevel(log_debug);
	e.log( log_error, "test1 printf.");
	e.clog << "test1 operator << endl " << endl; 
	e.clog << "test1 operator << flush" << flush; 
	e.clog << "test1 operator << flushl" << flushl; 
	cout << "cout test1 operator << flushl" << flushl; 
	cerr << "cerr test1 operator << flush" << flushl; 
	e.log( log_error, "test1 printf after clog used.");
}
void test2(ErrLog &e) { 
	e.clog << error_prio << "This is test 2, error prio." << flushl;
}
void test3(ErrLog &e) { 
	e.clog << info_prio << "This is test 3, info prio." << flushl;
}
void final(ErrLog &e) { 
	e.clog << fatal_prio << "This is test final, fatal prio." 
		<< flushl << "after fatal" ;
}

void mixed(ErrLog &e) { 
	e.clog << error_prio << "This is test mixed, error prio." ;
	e.clog << error_prio << "Using output operator. " << error_prio; 
	e.log(log_error, "Using syslog style");
	e.clog << error_prio << "Using output operator again. " << error_prio; 
	e.log(log_error, "Using syslog style again");
	e.clog << flushl;
}

testfunc array[] = {
	test1, test2, test3, mixed, final, 0
};

void 
alltests(ErrLog *e, void *arg)
{
	int i = (int) arg;
	testfunc f = array[i-1];
	assert(f);
	cerr << "test " << i  << " on " << e->ident() << "...";
	(*f)(*e);
	cerr << " DONE" << endl;
}

ErrLog *tfile, *topen, *tsyslog, *terr, *tether;
main()
{
	char *path = "tfile";

/*
	tether = new ErrLog("to-ether", log_to_ether, 0, log_debug);
	cerr << "tether created." << endl;
*/

	tsyslog = 
		new ErrLog("to-syslog", log_to_syslogd, (void *)LOG_USER, log_debug);
	cerr << "tsyslog created." << endl;

	terr = new ErrLog("to-err", log_to_stderr, 0, log_debug);
	cerr << "terr created." << endl;

	topen = new ErrLog("to-open", log_to_open_file, stdout, log_debug);
	cerr << "topen created." << endl;

	tfile = new ErrLog("to-file", log_to_unix_file, path, log_debug);
	cerr << "tfile created." << endl;
	
	ErrLog::apply(alltests, (void *)1);
	/*
	ErrLog::apply(alltests, (void *)2);
	ErrLog::apply(alltests, (void *)3);
	ErrLog::apply(alltests, (void *)4);
	ErrLog::apply(alltests, (void *)5);
	*/
}
