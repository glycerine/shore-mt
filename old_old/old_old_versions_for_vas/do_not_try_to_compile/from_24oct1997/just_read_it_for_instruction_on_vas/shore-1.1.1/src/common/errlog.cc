/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#define __ERRLOG_C__
/* errlog.C -- error logging functions */

#include <stdio.h>
#include <stream.h>
#include <iostream.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

#ifdef __GNUC__
#pragma implementation "errlog.h"
#pragma implementation "errlog_s.h"
#pragma implementation "debug.h"
#endif

#include "errlog.h"
#include <unix_error.h>

#include <errlog_s.h>

#ifdef __GNUC__
template class w_list_t<ErrLogInfo>;
template class w_list_i<ErrLogInfo>;
template class w_keyed_list_t<ErrLogInfo, simple_string>;
#endif

char __c;
ostrstream  logstream::static_stream(&__c,1);

ostream &operator<<(ostream &out, const simple_string x) {
	out << x._s;
	return out;
}
static char buffer[8192]; // big enough for any result that	
						  // can fit on a page

ErrLogInfo::ErrLogInfo(ErrLog *e)
    : _ident(e->ident()), _e(e)
{ 
}

// grot- have to wrap w_keyed_list_t to get its destructor
static w_keyed_list_t<ErrLogInfo,simple_string> 
	_tab(offsetof(ErrLogInfo,_ident),offsetof(ErrLogInfo,hash_link));


class errlog_dummy {
	// class exists *JUST* to get rid of all the logs
	// so that the hash_t code doesn't choke on exit().
	// 
	// ... and for debugging purposes

	friend class ErrLog;

protected:
	bool table_cleared;

public:
	errlog_dummy(){ 
		table_cleared = false;
#ifdef PURIFY
	if(purify_is_running()) {
		memset(buffer, '\0', sizeof(buffer));
	}
#endif
	}
	~errlog_dummy();
	void dump();
}_d;


errlog_dummy::~errlog_dummy() {
	ErrLogInfo *ei;

	while((ei = _tab.pop())) {
		delete ei;
	}
	table_cleared = true;
}

void
errlog_dummy::dump() {
	ErrLogInfo *ei;
    w_list_i <ErrLogInfo> iter(_tab);
	while((ei=iter.next())) {
		ei->dump();
	}
}

LogPriority
ErrLog::parse(const char *arg, bool *ok)
	//doesn't change *ok if no errors
{
	LogPriority level = log_none;

	if(strcmp(arg, "off")==0) {
		level = log_none;
	} else
	if(strcmp(arg, "trace")==0 || strcmp(arg,"debug")==0) {
		level = log_debug;
	} else
	if(strcmp(arg, "info")==0) {
		level = log_info;
	} else
	if(strcmp(arg, "warning")==0) {
		level = log_warning;
	} else
	if(strcmp(arg, "error")==0) {
		level = log_error;
	} else
	if(strcmp(arg, "internal")==0 || strcmp(arg,"critical")==0) {
		level = log_internal;
	} else
	if(strcmp(arg, "fatal")==0 || strcmp(arg,"alert")==0) {
		level = log_fatal;
	} else
	if(strcmp(arg, "emerg")==0) {
		level = log_emerg;
	} else {
		if (ok) *ok = false;
	}
	return level;
}

ErrLog *
ErrLog::find(const char *id) 
{
	const simple_string ss(id);
	ErrLogInfo *ei = _tab.search(ss);
	if(ei) return ei->_e;
	else return NULL;
}

void
ErrLog::apply(ErrLogFunc func, void *arg)
{
	ErrLogInfo *ei;
    w_list_i <ErrLogInfo> iter(_tab);
	while((ei=iter.next())) {
		(*func)(ei->_e, arg);
	}
}

void 
ErrLog::_closelogfile() 
{ 
	assert(_file != NULL);
	fclose(_file);
}

void
ErrLog::_openlogfile(
	const char *fn  	
) 
{
	const char *filename=fn;
	if((strcmp(filename, "syslogd")==0) 
		|| (strcmp(filename, "syslog")==0) ) {
		_destination = log_to_syslogd;
		_file = 0;
		errno = 0;
		(void) openlog(_ident, LOG_PID 
#ifndef Mips
							| LOG_CONS | LOG_NDELAY | LOG_NOWAIT
#endif
		,
			_facility);
		if(errno != 0) {
			cerr << "Cannot openlog()." << endl;
			perror("openlog");
			abort();
		}
		return;
	}
	if(strcmp(filename, "-")==0) {
		// cerr << "log to stderr" << endl;
		_destination = log_to_stderr;
		_file = stderr;
		return;
	}
	if(filename) {
		_destination = log_to_unix_file;
		if(strncmp(filename, "unix:", 5) == 0) {
			filename += 5;
		} else if (strncmp(filename, "shore:", 6) == 0) {
			filename += 6;
		}
		_file = fopen(filename, "a+");
		if(_file == NULL) {
			cerr << "Cannot open Unix file " << filename << endl;
			perror("fopen");
			abort();
		}
	} else {
		cerr << "Unknown logging destination." << endl;
		abort();
	}

}
ErrLog::ErrLog(
	const char *ident,			// required
	LoggingDestination dest, 	// required
	void *arg, 					// = 0, one of : pathname, FILE *,
								// lrid_t,  or syslog facility
	LogPriority level, 			//  = log_error
	char *ownbuf, 				//  = 0
	int  ownbufsz 				//  = 0

) :
		_destination(dest),
		_level(level), 
		_file(0), 
		_facility(LOG_LOCAL6),
		_ident(ident), 
		_buffer(ownbuf?ownbuf:buffer),
		_bufsize(ownbuf?ownbufsz:sizeof(buffer)),
		__magic1(ERRORLOG__MAGIC),
		clog(*this, ownbuf?ownbuf:buffer, ownbuf?ownbufsz:sizeof(buffer))
{
	this->clog.seekp(ios::beg);
	switch( dest ) {
		case log_to_unix_file: 
			{ 	char *filename;
				if(!arg) {
					filename = "-"; // stderr
				} else {
					filename = (char *)arg;
				}
				_openlogfile(filename);
			}
			break;

		case log_to_open_file: 
			_file = (FILE *)arg;
			break;

		case log_to_syslogd:
			_facility = (int) arg;
			break;

		case log_to_stderr:
			_file = stderr;
			break;

		case log_to_ether:
			break;

		default:
			_destination = log_to_ether;
			break;
	}
	ErrLogInfo *ei;
	if((ei = _tab.search(this->_ident)) == 0) {
		ei = new ErrLogInfo(this);
		_tab.put_in_order(ei); // not really ordered
	} else {
		cerr <<  "An ErrLog called " << _ident << " already exists." << endl;
		abort();
	}
}

ErrLog::~ErrLog() 
{
	switch(_destination) {
		case log_to_unix_file:
		case log_to_open_file:
			_closelogfile();
			break;

		case log_to_stderr: 
			// let global destructors 
			// do the close - we didn't open
			// it, we shouldn't close it!
			break;

		case log_to_syslogd:
#if defined(SOLARIS2) || defined(Linux) || defined(AIX41)
			closelog();
#else
			if(closelog() < 0) { 
				cerr << "Cannot closelog()" << endl;
				perror("closelog");
				abort();
			}
#endif
			break;
		case log_to_ether:
			break;
	}
	if( !_d.table_cleared ) {
		ErrLogInfo *ei = _tab.search(this->_ident);
		assert(ei!=NULL);
		// remove from the list
		(void) ei->hash_link.detach();
		delete ei;
	}
}

void 
ErrLog::log(enum LogPriority prio, const char *format, ...) 
{
	va_list ap;
	va_start(ap, format);

	_flush(); 

	if (prio > _level)
		return;

	switch(_destination) {
		case log_to_syslogd:
			// deal with varargs
			// can't use %m - oh well

			(void) vsprintf(_buffer, format, ap);
			// brain-dead library function...
			// vsprintf should return # chars written, like
			// vfprintf does.
			if(strlen(_buffer) >= _bufsize) {
				cerr << "Overran buffer." << endl;
				abort();
			}
			(void) syslog(prio,_buffer);
			break;

		case log_to_unix_file:
		case log_to_open_file:
		case log_to_stderr:

			(void) vfprintf(_file,format, ap);
			fputc('\n', _file);
			fflush(_file);
			break;
			
		case log_to_ether:
			break;
	}
	va_end(ap);

	// clear the slate for the next use of operator<<
	this->clog.seekp(ios::beg);

	if (prio == log_fatal) {
		abort();
	}
}

void 
ErrLog::_flush() 
{ 
	// cerr << "ErrLog::_flush()" << endl;

	this->clog << ends ;

	if (this->clog._prio <= _level) {
		switch(_destination) {
			case log_to_syslogd:
				// never malloced so this is ok
				(void) syslog(this->clog._prio,this->clog.str());
				break;

			case log_to_unix_file:
			case log_to_open_file:
			case log_to_stderr:
				fprintf(_file, "%s", this->clog.str());
				// fprintf(_file, "%s\n", this->clog.str());
				fflush(_file);
				break;
				
			case log_to_ether:
				break;
		}
	} else {
		// cerr << "wrong priority" << endl;
	}
	if (this->clog._prio == log_fatal) {
		abort();
	}

	// reset to beginning of buffer
	this->clog.seekp(ios::beg);
}

logstream *
is_logstream(ostream &o) 
{
	logstream *l=0;
	const ostream *tied = o.tie();
		// cerr << "tied " << ::hex((unsigned int)tied) << endl;
	if(tied == &logstream::static_stream) {
		l = (logstream *)&o;
	}
	if(l) {
		// cerr << "magic1 " << (unsigned int)l->__magic1 << endl;
		// cerr << "magic2 " << (unsigned int)l->__magic1 << endl;
		// cerr << "_prio" << l->_prio << endl;
	}
	if(l && 
		(l->__magic1 == LOGSTREAM__MAGIC) &&
		(l->__magic2 == LOGSTREAM__MAGIC) &&
		(l->_prio >= log_none) &&
		(l->_prio <= log_all) &&
		(l->_log.__magic1 == ERRORLOG__MAGIC)
	   ) {
		// cerr << " IS log stream" << endl;
		return l;
    } else {
		// cerr << " NOT log stream" << endl;
		return (logstream *)0;
	}
}

ostream & 
flush_and_setprio(ostream& o, LogPriority p)
{
	// cerr << "flush_and_setprio o=" << ::hex((unsigned int)&o) << endl;
	logstream *l = is_logstream(o);
	if(l) {
		l->_log._flush(); 
		if(p != log_none) {
			l->_prio =  p;
		}
    } else {
		o << flush;
	}
	return o;
}

ostream& flushl(ostream& out)
{
	out << endl;
	return flush_and_setprio(out, log_none); 
}
ostream& emerg_prio(ostream& o){return flush_and_setprio(o, log_emerg); }
ostream& fatal_prio(ostream& o){return flush_and_setprio(o, log_fatal); }
ostream& internal_prio(ostream& o){ return flush_and_setprio(o, log_internal); }
ostream& error_prio(ostream& o){return flush_and_setprio(o, log_error); }
ostream& warning_prio(ostream& o){ return flush_and_setprio(o, log_warning); }
ostream& info_prio(ostream& o){ return flush_and_setprio(o, log_info); }
ostream& debug_prio(ostream& o){ return flush_and_setprio(o, log_debug); }

#include "debug.cc"
