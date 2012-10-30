/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __SVAS_SERVICE_H__
#define __SVAS_SERVICE_H__
/* 
 * $Header: /p/shore/shore_cvs/src/vas/server/svas_service.h,v 1.11 1995/04/24 19:47:44 zwilling Exp $
 */

class _service {
public:
	typedef w_rc_t	(*func)(void *);
	const char *name;
	ErrLog	*log;	// uses options
	LogPriority 	_loglevel;

	w_rc_t	_openlog(const char *fn) {
		log = new ErrLog(name, log_to_unix_file, (void *)fn);
		if(!log) {
			return RC(errno); 
		}
		return RCOK;
	}

	func	__setup_options; // called before options are set
								// presumably to add the options
	func	__start;				// called after options values 
	func	__shutdown;			// to shut itself down -- cannot block
	func	__cleanup;			// after shutdown

	_service(const char *n,
		func optns, func start, func shut, func clean):
		name(n),
		log(0),
		__setup_options(optns),
		__start(start),
		__shutdown(shut),
		__cleanup(clean) {}

	~_service() {}
};
typedef void (*vfunc)(void *);

#endif /*__SVAS_SERVICE_H__*/
