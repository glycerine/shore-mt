/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#define GRID_BASICS_C

#include "ShoreConfig.h"
#include "string.h"
#include "grid_basics.h"

item_t::item_t()
    : x(0), y(0)
{
    memset(name, 0, MAX_NAME_LEN);
}

item_t::item_t(const char* _name, int _x, int _y)
    : x(_x), y(_y)
{
    strncpy(name, _name, MAX_NAME_LEN);
    name[MAX_NAME_LEN] = 0;  // make sure string ends in zero
}


