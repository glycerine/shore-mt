/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

//
// ShoreApp.h
//

#ifndef __SHOREAPP_H__
#define __SHOREAPP_H__

// This file is the only Shore header file that application programs
// should include.

#include <assert.h>
#include <new.h>
#include "Type.h"
#include "Shore.h"
#include "ShoreConfig.h"
#include "sdl_string.h"
#include "sdl_set.h"
#include "OCRef.h"
#include "Any.h"
#include "Pool.h"
#include "PoolScan.h"
#include "DirScan.h"
#include "reserved_oids.h"
#include "sdl_templates.h"

// define this so that w_list doesn't include w_list.c
#ifndef EXTERNAL_TEMPLATES
#   define EXTERNAL_TEMPLATES
#endif

#include "w_list.h"
#include "option.h"
#include "process_options.h"

class Type;
class Type_ref;

#endif /* __SHOREAPP_H__ */
