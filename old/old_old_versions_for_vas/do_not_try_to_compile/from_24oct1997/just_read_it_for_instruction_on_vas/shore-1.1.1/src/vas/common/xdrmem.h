/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __XDRMEM_H__
#define __XDRMEM_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/xdrmem.h,v 1.13 1995/04/24 19:45:19 zwilling Exp $
 */
#include <copyright.h>

#include <externc.h>

enum xdr_kind { 
	x_unknown=0,
	x_serial_t, 
	x_time_t,
	x_entry, 
	x_int,
	x_reg_sysprops, 
	x_reg_sysprops_withtext, 
	x_reg_sysprops_withindex, 
	x_reg_sysprops_withtextandindex, 
	x_anon_sysprops,
	x_anon_sysprops_withtext,
	x_anon_sysprops_withindex,
	x_anon_sysprops_withtextandindex,
	x_common_sysprops,
	x_common_sysprops_withtext,
	x_common_sysprops_withindex,
	x_common_sysprops_withtextandindex,
	x_serial_t_list,
	x_directory_body,
	x_directory_value
	};
typedef enum xdr_kind xdr_kind;

enum xdr_direction { transient2disk, disk2transient, checksum };
typedef enum xdr_direction xdr_direction;

BEGIN_EXTERNCLIST
	/* int			xdr_serial_t(XDR *, void *); */

	/* 0 is bad, 1 is good */
	/*           trans,    disk,    x_serial_t,    transient2disk */
	int mem2disk(const void	*, void *, xdr_kind );
	int memarray2disk(const void	*, void *, xdr_kind, int );
	int disk2mem(void	*, const void *, xdr_kind );
	int disk2memarray(void	*, const void *, xdr_kind, int );
	void cmp_checksum(const void	*, const void *, smsize_t );
END_EXTERNCLIST

#endif
