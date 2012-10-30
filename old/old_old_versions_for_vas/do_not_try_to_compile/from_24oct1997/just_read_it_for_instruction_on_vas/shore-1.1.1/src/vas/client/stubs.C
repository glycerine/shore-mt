/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/client/stubs.C,v 1.14 1995/04/24 19:43:35 zwilling Exp $
 */
#include <copyright.h>

/* stubs for interpreter */
#include <tcl.h>
#include <server_stubs.h>
#include <svas_error_def.h>
#include <os_error_def.h>

char *sorry = " is avaliable only on the server.";
#define SORRY(a) Tcl_AppendResult((Tcl_Interp *)ip, a, sorry, 0)

void 	pconfig(ostream &out)	{ out << "Config " << sorry << endl; }
void 	print_mount_table()	{ }
void 	_dumpthreads(void*ip)	{ SORRY("\"threads\""); }
void 	_dumplocks(void*ip)	{ SORRY("\"locks\""); }
void 	pclients(void *ip, int unused) 		{ SORRY("\"clients\""); }
void 	crash(void *ip) 		{ SORRY("\"crash\""); }
void 	yield(void *ip) 		{ }

char 	*defaultHost()  				{ return "localhost"; }
int 	mkunixfile(void *ip,
	void*v_unused,
	char *c_unused,
	mode_t m_unused,
	vec_t &h_unused,
	lrid_t *l_unused
) {
	SORRY("\"mkunixfile\""); return SVAS_FAILURE;
}
