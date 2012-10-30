/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/serial_t_data.h,v 1.5 1995/04/24 19:28:36 zwilling Exp $
 */
#ifndef SERIAL_T_DATA_H
#define SERIAL_T_GUTS_H
#define SERIAL_T_DATA_H

#ifdef __GNUG__
#pragma interface
#endif

struct serial_t_data {
#ifdef BITS64
	uint4 _high;
#else
#	 define _high _low
#endif
	uint4 _low;
};

#endif
