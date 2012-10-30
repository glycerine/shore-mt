/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef ERRLOG_H
#define ERRLOG_H

/* $Header: /p/shore/shore_cvs/src/common/errlog.h,v 1.20 1997/05/19 19:41:01 nhall Exp $ */

#include <assert.h>
#include <stdlib.h>
#include <stddef.h>
#include <stream.h>
#include <strstream.h>
#include <stdio.h>

#ifdef Mips
    // on ultrix, the  LOG_* facilities aren't defined in
	// <syslog.h> but they are defined in <syslog.h>
	// (GROT)
	// no guarantee that that stuff will stay around, either.
#	include <sys/syslog.h>
#else
#	include <syslog.h>
#endif

#ifndef W_LIST_H
#include <w_list.h>
#endif

/* errlog.h -- facilities for logging errors */

#ifdef __GNUG__
#pragma interface
#endif

class ErrLog; // forward
class logstream; // forward

enum LogPriority {
	log_none = -1,	// none (for global variable logging_level only)
	log_emerg = LOG_EMERG,		// no point in continuing (syslog's LOG_EMERG)
	log_fatal = LOG_ALERT,		// no point in continuing (syslog's LOG_ALERT)
	log_alert = log_fatal,		// alias
	log_internal = LOG_CRIT,	// internal error 
	log_error = LOG_ERR,		// client error 
	log_warning = LOG_WARNING,	// client error 
	log_info = LOG_INFO,		// just for yucks 
	log_debug=LOG_DEBUG,		// for debugging gory details 
	log_all ,		            // all (for global variable logging_level only)
};
#define default_prio  log_error

extern ostream& flushl(ostream& o);
extern ostream& emerg_prio(ostream& o);
extern ostream& fatal_prio(ostream& o);
extern ostream& internal_prio(ostream& o);
extern ostream& error_prio(ostream& o);
extern ostream& warning_prio(ostream& o);
extern ostream& info_prio(ostream& o);
extern ostream& debug_prio(ostream& o);
extern	void setprio(ostream&, LogPriority);
extern logstream *is_logstream(ostream &);

#define LOGSTREAM__MAGIC 0xad12bc45
#define ERRORLOG__MAGIC  0xa2d29754

class logstream : public ostrstream {
	friend class ErrLog;
	friend ostream &flush_and_setprio(ostream& o, LogPriority p);
	friend ostream& emerg_prio(ostream& o);
	friend ostream& fatal_prio(ostream& o);
	friend ostream& internal_prio(ostream& o);
	friend ostream& error_prio(ostream& o);
	friend ostream& warning_prio(ostream& o);
	friend ostream& info_prio(ostream& o);
	friend ostream& debug_prio(ostream& o);

	unsigned int __magic1;
	LogPriority _prio;
	ErrLog 	&_log;
	unsigned int __magic2;

public:
	friend logstream *is_logstream(ostream &);

private:
	static ostrstream static_stream;
public:
	logstream(ErrLog &mine, char *buf, size_t bufsize = 1000) : 
		ostrstream(buf, bufsize), __magic1(LOGSTREAM__MAGIC),
		_prio(log_none), _log(mine), __magic2(LOGSTREAM__MAGIC)
		{
			// tie this to a hidden stream; 
			tie(&static_stream);
			assert(tie() == &static_stream) ;
			assert(__magic1==LOGSTREAM__MAGIC);
		}
};

enum LoggingDestination {
	log_to_ether, // no logging
	log_to_unix_file, 
	log_to_open_file, 
	log_to_syslogd,
	log_to_stderr
}; 

typedef void (*ErrLogFunc)(ErrLog *, void *);

class ErrLog {
	friend class logstream;
	friend logstream *is_logstream(ostream &);
	friend ostream &flush_and_setprio(ostream& o, LogPriority p);

	LoggingDestination	_destination;
	LogPriority _level;
	FILE 	*_file;		// if local file logging is used
	int		_facility; // if syslog  is used
	const char * _ident; // identity for syslog & local use
	char *_buffer; // default is static buffer
	size_t	_bufsize; 
	unsigned int __magic1;
public:

	ErrLog(
		const char *ident,
		LoggingDestination dest, // required
		void *arg = 0, 			// one of : pathname, FILE *,
								// lrid_t,  or syslog facility
		LogPriority level =  default_prio,
		char *ownbuf = 0,
		int  ownbufsz = 0  // length of ownbuf, if ownbuf is given
	);
	~ErrLog();

	static LogPriority parse(const char *arg, bool *ok=0);

	// same name
	logstream	clog;
	void log(enum LogPriority prio, const char *format, ...);

	const char * ident() { 
		return _ident;
	}
	LoggingDestination	destination() { return _destination; };

	LogPriority getloglevel() { return _level; }
	const char * getloglevelname() { 
		switch(_level) {
			case log_none:
				return "log_none";
			case log_emerg:
				return "log_emerg";
			case log_fatal:
				return "log_fatal";
			case log_internal:
				return "log_internal";
			case log_error:
				return "log_error";
			case log_warning:
				return "log_warning";
			case log_info:
				return "log_info";
			case log_debug:
				return "log_debug";
			case log_all:
				return "log_all";
			default:
				return "error: unknown";
				// w_assert1(0);
		}
	}

	LogPriority setloglevel( LogPriority prio) {
		LogPriority old = _level;
		_level =  prio;
		return old;
	}

	static ErrLog *find(const char *id);
	static void apply(ErrLogFunc func, void *arg);

private:
	void _flush();
	void _openlogfile( const char *filename );
	void _closelogfile();
	NORET ErrLog(const ErrLog &); // disabled

} /* ErrLog */;

#endif /* ERRLOG_H */
