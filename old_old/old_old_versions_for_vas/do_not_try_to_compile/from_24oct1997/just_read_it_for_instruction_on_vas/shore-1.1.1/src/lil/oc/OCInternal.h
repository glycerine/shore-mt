/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// OCInternal.h
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/OCInternal.h,v 1.8 1995/04/24 19:33:05 zwilling Exp $ */

#ifndef __OCINTERNAL_H__
#define __OCINTERNAL_H__

//
// This include file contains everything needed by the internals of
// the object cache.  Application programs need never include it
// directly.  They should include only ShoreApp.h.
//

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#ifdef __GNUG__
#pragma interface
#endif

#include <iostream.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "debug.h"
#include "e_error.h"
//#include "shore_vas.h"
#include "SH_error.h"

#include "OCTypes.h"
#include "shore_vas.h"

#endif
