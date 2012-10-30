/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __SHORE_VAS_H__
#define __SHORE_VAS_H__
/*
 * $Header: /p/shore/shore_cvs/src/vas/include/shore_vas.h,v 1.5 1995/06/21 18:25:16 zwilling Exp $
 */

// the application's include file 
// FOR APPLICATIONS ONLY

#ifdef Linux
#include <features.h>
#endif

#include <copyright.h>
#include <iostream.h>
#include <debug.h>
#include <svas_base.h>
#include <option.h>
#include <process_options.h>
typedef svas_base shore_vas;

#endif /*__SHORE_VAS_H__*/
