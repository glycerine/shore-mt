/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/debug.h,v 1.30 1997/05/19 19:40:57 nhall Exp $
 */
#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef __GNUG__
#pragma interface
#endif

#ifndef W_WORKAROUND_H
#include <w_workaround.h>
#endif

#ifdef __cplusplus
#ifndef ERRLOG_H
#include <errlog.h>
#endif
#endif

/* ************************************************************************
*  This is a set of macros for use with C or C++. They give various
*  levels of debugging printing when compiled with -DDEBUG, and 
*  generate no code under -UDEBUG.
*  With -DDEBUG, message printing is under the control of an environment
*  variable DEBUG_FLAGS (see debug.C).  If that variable is set, its value must 
*  be  a string.  The string is searched for __FILE__ and the function name 
*  in which the debugging message occurs.  If either one appears in the
*  string (value of the env variable), or if the string contains the
*  word "all", the message is printed.  
*
*
**** DUMP(x)  prints x (along with line & file)  
*             if "x" is found in debug environment variable
*
**** FUNC(fname)  makes a local variable "_fname_debug_", whose value is
*             the string "fname", then DUMPs the function name.
*
**** RETURN   prints that the function named by _fname_debug_ is returning
*             (if _fname_debug_ appears in the debug env variable).
*             This macro  MUST appear within braces if used after "if",
*    	      "else", "while", etc.
*
**** DBG(arg) prints line & file and the message arg if _fname_debug_
*    		  appears in the debug environment variable.
*             The argument must be the innermost part of legit C++
*             print statement, and it works ONLY in C++ sources.
*
**** _TODO_     prints line & file and "TODO", then fails on assert.
*
*  Example :
*
*    returntype
*    proc(args)
*    {
*    	FUNC(proc);
*       ....body...
*
*       DBG(
*          << "message" << value
*          << "more message";
*          if(test) {
*             cerr << "xyz";
*          }
*          cerr
*       )
*
*       ....more body...
*       if(predicate) {
*           RETURN value;
*    	}
*    }
*
* Alternative tracing/debugging facility:
*
**** TRACE(id, arg)	prints the message given by arg if appropriate condition
*			is satisfied, as explained below.
*
* The TRACE macro, defined below, is similar to the DBG macro except that the
* control over message printing is different. With TRACE, the message is printed
* only if the trace level of the running thread is greater than the trace level
* of the TRACE statement itself.
* The trace level of a thread is given by the "trace_level" instance var of the
* sthread_t class and it can be set at any time during execution. When a thread
* is created its trace level is initialized to the value of the TRACE_LEVEL env
* variable, if defined, otherwise to the min int value (so that no tracing
* messages are printed).
*
* The trace level of a trace statement is determined indirectly:
* Each trace statement is given an id which serves as an index to a global
* table (trace_tab; defined in debug.C) of trace levels. So, for example, the
* level of a trace statement with id 3 is given by trace_tab[3]. All entries in
* the trace_tab are initialized to max int (so that no trace msg is printed).
* Next, an input ascii file is read, which contains trace levels for particular
* trace ids (see debug.C). The location of this file is given by the TRACE_FILE
* env variable, if defined; otherwise no trace input file is read. 
* Assingment of trace ids to TRACE statements is the responsibility of the
* "user" (i.e. the programmer). Uniqueness of trace ids will be desired in most
* cases, but it is not necessary. A good practice is that all programmers use
* the same trace file, and  that non-overlapping  ranges of ids be assigned for
* use by different source files. The trace.dat file inside the "common" dir is
* the trace file used so far.
* ************************************************************************  */

#include <assert.h>
#include <unix_error.h>
#ifndef CAT_H
#include "cat.h"
#endif
#include "regex.posix.h"

#ifdef __cplusplus
#    include <stream.h>
#    include <fstream.h>
#else 
#    include <stdio.h>
#endif /*__cplusplus*/

/* ************************************************************************ 
 * 
 * DUMP, FUNC, and RETURN macros:
 *
 */

#ifdef DEBUG
#    ifdef __cplusplus

#    	define DUMP(str)\
			if(_debug.flag_on(_fname_debug_,__FILE__)) {\
				_debug.clog << __LINE__ << " " << __FILE__ << ": " << _string(str)\
			<< flushl; }
#    else 
#    	define DUMP(str)\
			fprintf(stderr, "%s %d, %s\n", __FILE__,__LINE__,_string(str));
#    endif /*__cplusplus*/

#    define FUNC(fn)\
    	char		*_fname_debug_ = _string(fn); DUMP(_string(fn));

#    ifdef __cplusplus

#    	define RETURN \
    		if(_debug.flag_on(_fname_debug_,__FILE__)) {\
    			_debug.clog << ::dec(__LINE__) << " " << __FILE__ << ": " \
    			<< "return from " << _fname_debug_ << flushl; }\
    		return 

#    else 
#    	define RETURN\
    		fprintf(stderr,"%s %d, return from %s\n",\
    			__FILE__,__LINE__,_fname_debug_);\
    		return 
#    endif /*else of: ifdef __cplusplus*/

#else /* -UDEBUG */
#    define DUMP(str)
#    define FUNC(fn)
#    define RETURN return
#endif /*else of: ifdef DEBUG*/

/* ************************************************************************  */

/* ************************************************************************  
 * 
 * Class __debug, macros DBG, DBG_NONL, DBG1, DBG1_NONL:
 */

/*
 * Whether -DDEBUG or -UDEBUG, we want an extern _fname_debug_
 * to exist (defined in debug.C) so that some modules can be
 * compiled with debugging and other without.
 */
extern  char    *_fname_debug_;

#if defined(__cplusplus)

	class __debug : public ErrLog {
	private:
		char *_flags;
		enum { _all = 0x1, _none = 0x2 };
		unsigned int	mask;
		int		_trace_level;

		static regex_t		re_posix_re;
		static bool			re_ready;
		static char*		re_error_str;
		inline static char*	re_comp_debug(const char* pattern);
		inline static int	re_exec_debug(const char* string);

		inline int	all(void) { return (mask & _all) ? 1 : 0; }
		inline int	none(void) { return (mask & _none) ? 1 : 0; }

	public:
		__debug(char *n, char *f);
		~__debug();
		int flag_on(const char *fn, const char *file);
		char *flags() { return _flags; }
		void setflags(const char *newflags);
		void memdump(void *p, int len); // hex dump of memory
		int trace_level() { return _trace_level; }
	};

	char* __debug::re_comp_debug(const char* pattern)
	{
		if (re_ready)
			regfree(&re_posix_re);
		
		if (regcomp(&re_posix_re, pattern, REG_NOSUB|REG_EXTENDED) != 0)
			return re_error_str;
		
		re_ready = true;
		return NULL;
	}

	int __debug::re_exec_debug(const char* string)
	{
		if (!re_ready)  // no compiled string
			return -1;
		
		return regexec(&re_posix_re, string, (size_t)0, NULL, 0) == 0;
	}

	extern __debug _debug;
#endif  /*defined(__cplusplus)*/

#if defined(DEBUG)&&defined(__cplusplus)

extern int trace_tab[];


#	define DBG1(a) if(_debug.flag_on((_fname_debug_),__FILE__)) {\
    _debug.clog <<::dec(__LINE__) << " " << __FILE__ << ": "  a	<< flushl;\
    }

#	define DBG1_NONL(a) if(_debug.flag_on((_fname_debug_),__FILE__)) {\
    _debug.clog <<::dec( __LINE__) << " " << __FILE__ << ": "  a  ;\
    }

#	define DBG(a) DBG1(a)
#	define DBG_NONL(a) DBG1_NONL(a)  /* No new-line */

#	define TRACE(id, a) if (me()->trace_level > trace_tab[(id)]) {\
    _debug.clog << a << flushl;\
    }

#       define TRACE_NONL(id, a) if (me()->trace_level > trace_tab[(id)]) {\
    _debug.clog << a ;\
    }

#else
#	define DBG(a) 
#	define DBG_NONL(a)
#	define TRACE(id, a)
#	define TRACE_NONL(id, a)
#endif  /* else of: if defined(DEBUG)&&defined(__cplusplus)*/
/* ************************************************************************  */

/* ******************************************************
 * Use "dassert" to provide another level of debugging:
 * "dasserts" go away if -UDEBUG.
 * whereas
 * "asserts" go away only if -DNDEBUG and  -UDEBUG
 */
#ifdef DEBUG
#	define dassert(a) assert(a)
#else
#	define dassert(a) 
#endif /* else of: ifdef DEBUG*/
/* ****************************************************** */

/* ******************************************************
 * 
 * Macro _TODO_
 */
#	ifdef __cplusplus
#    	define _TODO_ { _debug.clog << "TODO *****" << flushl; assert(0);  }
#	else 
#    	define _TODO_ { fprintf(stderr,"******TODO *****\n"); assert(0); }
#	endif /*else of: ifdef __cplusplus*/
/* ****************************************************** */

#endif /*__DEBUG_H__*/
