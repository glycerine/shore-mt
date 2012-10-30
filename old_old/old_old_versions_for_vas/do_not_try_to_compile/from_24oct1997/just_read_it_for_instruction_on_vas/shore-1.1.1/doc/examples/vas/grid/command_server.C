/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * This file implements the server side of the RPCs
 *
 * It is organized as a mix of RPC stub and corresponding
 * command_server_t method for the stub.
 */

#include "ShoreConfig.h"
#include <stream.h>
#include <string.h>
#include <rpc/rpc.h>
// include stuff needed for SM applications (clients)
#include "sm_vas.h"
#include "grid_basics.h"
#define RPC_SVC  /* so rpc prototypes are included */
#include "msg.h"
#include "grid.h"
#include "command.h"
#include "command_server.h"
#include "rpc_thread.h"

lvid_t command_server_t::lvid = lvid_t::null;
const char* command_server_t::grid_name = "Grid_Name";

/*
 * This is an error handling macro that calls "method".
 * If "method" returns an error, a message is written to the
 * command_server_t err_strstream and the string message is returned.
 */
#define SSMDO(method)					\
{							\
    w_rc_t __e = method;				\
    if (__e) { 						\
	cerr << __e << endl;				\
	err_strstream.seekp(0, ios::beg);		\
	err_strstream << "Error from SSM: " 		\
		      << __e->error_string(__e.err_num()) << ends;	\
	return err_strstream.str();			\
    } 							\
}							

/********************************************************************
  Command_server_t Methods
 *******************************************************************/

command_server_t::command_server_t()
    : err_strstream(_err_space, sizeof(_err_space))
{
    W_COERCE(ss_m::begin_xct());

    rc_t rc = init();
    if (rc) {
	cerr << "Error: could not start command server due to:" << endl;
	cerr << "    " << rc << endl;
	W_COERCE(ss_m::abort_xct());
	exit(1);
    }

    W_COERCE(ss_m::commit_xct());  // commit the board initialization

    // start the first transaction for this client
    W_COERCE(ss_m::begin_xct());
}

command_server_t::~command_server_t()
{
    // connection is shutting down, so
    // abort the currently running transaction
    W_COERCE(ss_m::abort_xct());
}


/*
 * This functions initialized a command_server_t, including
 * creating the grid database structures if they do not yet exist.
 * 
 * First, it sees if there is a grid on the volume by
 * looking up the special string, Grid_Name, in
 * the volume root index.
 *
 * If no grid already exists, we create one.  And store info
 * about it in the root index.
 */
rc_t
command_server_t::init()
{
    serial_t root_iid;	// root index ID
    W_DO(ss_m::vol_root_index(lvid, root_iid));
    grid_t::grid_info_t info;
    smsize_t    info_len = sizeof(info);
    bool	found;
    W_DO(ss_m::find_assoc(lvid, root_iid,
			      vec_t(grid_name, strlen(grid_name)),
			      &info, info_len, found));
    if (found) {
	assert(info_len == sizeof(info));
	cout << "Using already existing grid" << endl;
    } else {
	cout << "Creating a new Grid" << endl;

	// create the item file
	W_DO(ss_m::create_file(lvid, info.item_file, ss_m::t_regular));

	// create the btree index on item name
	// the "b*1000" indicates the key type is a variable
	// length byte string with maximum length of 1000
	W_DO(ss_m::create_index(lvid, ss_m::t_uni_btree, ss_m::t_regular,
				"b*1000", 0, info.name_index));

	// create the R*tree index on item location
	W_DO(ss_m::create_md_index(lvid, ss_m::t_rtree, ss_m::t_regular, info.spatial_index));

	// store the grid info in the root index
	W_DO(ss_m::create_assoc(lvid, root_iid,
				vec_t(grid_name, strlen(grid_name)),
 				vec_t(&info, sizeof(info))));
    }

    grid.init(lvid, info);    
    return RCOK;
}


/********************************************************************
  The following command_server_t methods correspond to RPCs
 *******************************************************************/

cmd_err_t
command_server_t::commit_transaction()
{
    SSMDO(ss_m::commit_xct());
    SSMDO(ss_m::begin_xct());
    return 0; /* success */
}

cmd_err_t
command_server_t::abort_transaction()
{
    SSMDO(ss_m::abort_xct());
    SSMDO(ss_m::begin_xct());
    return 0; /* success */
}

cmd_err_t
command_server_t::clear_grid()
{
    SSMDO(grid.clear());
    return 0; /* success */
}

cmd_err_t
command_server_t::print_grid(grid_display_t& rows)
{
    SSMDO(grid.generate_display(rows));
    return 0; /* success */
}

cmd_err_t
command_server_t::add_item(const char* name, int x, int y)
{
    SSMDO(grid.add_item(name, x, y));
    return 0; /* success */
}

cmd_err_t
command_server_t::remove_item(const char* name)
{
    bool found;
    SSMDO(grid.remove_item(name, found));
    if (!found) {
	err_strstream.seekp(0, ios::beg);
	err_strstream << "Error: item was not found" << endl;
	return err_strstream.str();
    }
    return 0; /* success */
}


cmd_err_t
command_server_t::move_item(const char* name, int x, int y)
{
    bool found;
    SSMDO(grid.move_item(name, x, y, found));
    if (!found) {
	err_strstream.seekp(0, ios::beg);
	err_strstream << "Error: item was not found" << endl;
	return err_strstream.str();
    }
    return 0; /* success */
}


cmd_err_t
command_server_t::location_of(const char* name, int& x, int& y)
{
    bool found;
    SSMDO(grid.location_of(name, x, y, found));
    if (!found) {
	err_strstream.seekp(0, ios::beg);
	err_strstream << "Error: item was not found" << endl;
	return err_strstream.str();
    }
    return 0; /* success */
}


cmd_err_t
command_server_t::spatial_query(const nbox_t& box, spatial_result_t& result)
{
    SSMDO(grid.spatial_query(box, result));
    return 0; /* success */
}

