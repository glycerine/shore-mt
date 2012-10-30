/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __TCLSHELL_SERVICE_H__
#define __TCLSHELL_SERVICE_H__
/* 
 * $Header: /p/shore/shore_cvs/src/vas/server/tclshell_service.h,v 1.3 1995/06/23 13:18:35 nhall Exp $
 */

#include "svas_service.h"

class shell_client_t; //forward
class tclshell_service: public _service {
public:
	shell_client_t *thread;

	static w_rc_t	shutdown(void *);
	w_rc_t	_shutdown();
	static w_rc_t	cleanup(void *);
	w_rc_t	_cleanup();
	static w_rc_t	start(void *);
	w_rc_t	_start();

	w_rc_t	set_option_values(const char *log, LogPriority p);

public:
	tclshell_service(const char *n,  func o) : 
		_service(n, o, tclshell_service::start,
			tclshell_service::shutdown, 
			tclshell_service::cleanup) {
				thread = 0;
			}
	void disconnect(shell_client_t *x) { 
		if(x == thread) { thread = 0; }
	}
};

#endif /*__TCLSHELL_SERVICE_H__*/
