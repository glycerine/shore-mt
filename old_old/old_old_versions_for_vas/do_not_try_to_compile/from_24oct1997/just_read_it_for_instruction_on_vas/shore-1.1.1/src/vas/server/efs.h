/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef EFS_H
#define EFS_H

/*$Header: /p/shore/shore_cvs/src/vas/server/efs.h,v 1.22 1995/09/15 03:45:33 zwilling Exp $*/

#include <assert.h>
#include <debug.h>

#include "Object.h"
#include "Directory.h"
#include "nfs.h"
#include "svas_nfs.h"
#include "msg_stats.h"
#include <unix_error.h>
#include <errlog.h>
#include "udp_clients.h"
#include "svas_layer.h"
#include "svas_service.h"
#include "rpc_service.h"
#include "string.h"

// class lrid_t;

// EXTERNC int 	init_efs(u_short, char *);
// int 			init_efssvc(u_short);

// A handle for a SHORE object (EFS version)
struct efs_handle {
	lrid_t		lrid;		// the full oid of the object
	serial_t	reg_file;	// The oid of the file used for registered
							// object on the volume containing the object

	// These are only needed to get around a bug in g++ 2.4.5.  In some
	// circumstances, the compiler is not generating a default copy
	// constructor.
	efs_handle() : lrid(), reg_file() {}
	efs_handle(const efs_handle &h) : lrid(h.lrid), reg_file(h.reg_file) {}

#ifdef DEBUG
	void check() const{
		DBG(<<" lrid is at " 
			<< ::hex((unsigned int)&this->lrid)
			<< " this is at " << ::hex((unsigned int)this));

		DBG(<< "efs_handle.lrid == " << lrid);
		if((lrid.serial.data._low & 0x1)==0) {
			DBG(<<"failed check");
			dassert(0);
		} else if(lrid.lvid.low == 0) {
			DBG(<<"failed check");
			dassert(0);
		} else {
			DBG(<<"passed check");
		}
	}
#define CHECK(h) DBG(<< __LINE__ << " " << __FILE__); (h).check()
#define CHECKFH(h) DBG(<< __LINE__ << " " << __FILE__); ((const efs_handle *)&h)->check()
#else
#define CHECK(h) 
#define CHECKFH(h) 
#endif

};

inline char *
copy_string(const char *s) 
{
	return strcpy(new char[strlen(s)+1], s);
}

void efs_s2h(const efs_handle &s, nfs_fh &h);

/* define these to be legit values that are not otherwise
 * used -- keep in mind that if we ever return NFCHR or NFBLK,
 * the client side tries to use the major,minor numbers to
 * get to a device and things don't work well
 */
#define NFPOOL NFSOCK
#define NFINDX NFBLK /* not used for now */
#define NFXREF NFFIFO
#define NFNTXT NFCHR

mode_t NF2S_IF(ftype f);
ftype S_IF2NF(mode_t sif);

EXTERNC svas_server * ARGN2vas(void *);

#endif /* EFS_H */
