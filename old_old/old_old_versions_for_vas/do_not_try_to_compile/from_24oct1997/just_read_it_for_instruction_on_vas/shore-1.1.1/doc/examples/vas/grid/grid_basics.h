/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef GRID_BASICS_H
#define GRID_BASICS_H

#include "ShoreConfig.h"

/* 
 * This defines a bunch of commonly used constants for the grid
 * program.  
 *
 * Note: #define is since this file must be run through rpcgen.
 */

/*
 * Maximun length of an item name
 */
#define MAX_NAME_LEN 21

/*
 * Max error message length
 */
#define MAX_ERR_MSG_LEN 80

/*
 * Maximum size of grid
 */
#define MAX_GRID_X  40
#define MAX_GRID_Y  15


/*
 * Maximum # of items returns from a spatial query
 */
#define MAX_SPATIAL_RESULT 10

/*
 * Items on the grid
 * Note: #ifdef __cplusplus is to avoid sending c++ code
 *       through rpcgen (since it can't handle it)
 */
struct item_t {
#ifdef __cplusplus
                item_t();
                item_t(const char* _name, int _x, int _y);
    void        init(const char* _name, int _x, int _y);
#endif
    /* location on grid */
    int         x;
    int  	y;
    /* name of the item */
    char        name[MAX_NAME_LEN]; 
};

/*
 */

/*
 * Query Result Structures
 */

typedef char grid_display_row_t[MAX_GRID_X];

struct grid_display_t {
    grid_display_row_t  rows[MAX_GRID_Y];
};

/* results for spatial queries */
struct spatial_result_t {
    int         found_cnt;      /* number of items found	*/
    item_t      items[MAX_SPATIAL_RESULT]; /* some of the items found */
};



/* sunos 4.1.3 does not declare these */
#if defined(SUNOS41) && defined(__cplusplus)
extern "C" {
    void bzero(char*, int);
    int	socket(int, int, int);
    int	bind(int, const void *, int);
}
#endif

#endif /* GRID_BASIC_H */

