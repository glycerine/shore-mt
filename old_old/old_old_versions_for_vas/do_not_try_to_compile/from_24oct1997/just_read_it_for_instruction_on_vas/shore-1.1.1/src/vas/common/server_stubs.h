/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __SERVER_STUBS_H__
#define __SERVER_STUBS_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/server_stubs.h,v 1.15 1995/04/24 19:44:49 zwilling Exp $
 */
#include <copyright.h>

#include <stream.h>
#include <basics.h>
#include <vec_t.h>
#include <lid_t.h>
#include <w.h>
#include <externc.h>

/* 
// these have to be stubbed out in the
// client
*/
BEGIN_EXTERNCLIST
	void 	pclients(void *,int);
	void 	pconfig(ostream &out);
	char 	*defaultHost();
	int 	mkunixfile(void *ip,
			void 	*v, char *c,mode_t m,vec_t &h,lrid_t *l);
	void   _dumpthreads(void *);
	void   _dumplocks(void *);
	void   print_mount_table();
	void	crash(void *);
	void	yield(); // so server commands don't hog the
					// cycles
END_EXTERNCLIST
#endif /*__SERVER_STUBS_H__*/
