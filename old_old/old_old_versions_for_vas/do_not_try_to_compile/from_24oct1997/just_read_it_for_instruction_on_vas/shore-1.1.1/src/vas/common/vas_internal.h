/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __VAS_INTERNAL_H__
#define __VAS_INTERNAL_H__
/*
// To be included by all files that need vas class definitions.
// Files that need rpcgen-generated type definitions
// simply include <msg.h> first.
//
*/

#include <copyright.h>
#include <assert.h>
#include <stdio.h>
#include <debug.h>
#include <externc.h>
//#include <bzero.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
/*
#ifdef MAX
#undef MAX
#endif
#ifdef MIN
#undef MIN
#endif
*/
#include <sys/param.h>
#include <stddef.h>
#include <errno.h>
#include <errors.h>
#include <errlog.h>

#include <svas_error_def.h>
#include <os_error_def.h>

#include "inet_stuff.h"

#ifdef __GNUC__
#define TIME_T long
#else
#define TIME_T time_t
#endif /* __GNUC__ */
#include <time.h>

#ifdef __cplusplus
#	include <new.h>

#	ifdef RPC_HDR
		/*
		// forward decls for things to which 
		// the svas classes have pointers
		*/
		class xct_t; 
		class scan_index_i;
		class scan_file_i;
#	endif /* RPC_HDR */

	/* svas_base includes everything it needs */
#	include "svas_base.h"

#	ifndef RPC_HDR
		/* Any code that includes <msg.h> cannot
		 * also use reserved serial #s or oids because
		 * the latter requires a full c++ class definition
		 * of serial #s
		 */
#		include <reserved_oids.h>
#	endif /* RPC_HDR */

#	ifdef SERVER_ONLY
#		if !defined(SM_VAS_H) 
#			ifdef RPC_HDR
				/* include minimum necessary to get legit def'n of 
				 * the vas classes, but to avoid conflicts with basic
				 * types
				 */
#				include "smstats.h"
#			else
#				include "sm_vas.h"
#			endif
#		endif
#		ifndef __SYSPROPS_H__
#			include "sysprops.h"
#		endif
#		ifndef __SVAS_LAYER_H__
#			include "svas_layer.h"
#		endif
#		ifndef __SVAS_SERVER_H__
#			include "svas_server.h"
#		endif
#		if !defined(__CLIENT_H__)&&!defined(RPC_HDR)
#			include "client.h"
#		endif
#	endif
#	if defined(CLIENT_ONLY)
#		include "svas_client.h"
#	endif
#endif

BEGIN_EXTERNCLIST

#   if !defined(_RPC_TYPES_INCLUDED)&&!defined(_rpc_rpc_h)
#		include <rpc/rpc.h>
#   endif

		void	exit(int);
		bool lrid_t_is_null(const lrid_t &x); 
		bool lvid_t_is_null(const lvid_t &x); 
		bool serial_t_is_null(const serial_t &x); 
END_EXTERNCLIST

#endif
