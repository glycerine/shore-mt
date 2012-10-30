/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * This file implements the main() code for the grid client program
 */

#include "ShoreConfig.h"
#include <stream.h>
#include <string.h>
#include <rpc/rpc.h>
// include stuff needed for SM applications (clients)
#include "sm_app.h"
#include "nbox.h"
#include "grid_basics.h"
#define RPC_CLNT  /* so rpc prototypes are included */
#include "msg.h"
#include "grid.h"
#include "command.h"
#include "command_client.h"

#define DO_RPC(rpc, reply) \
    reply = rpc;					\
    if (reply == 0) {					\
	return clnt_sperror(cl, "");			\
    } else if (reply->error_msg[0] != 0) {		\
	return reply->error_msg;			\
    }


command_client_t::command_client_t(CLIENT* client)
	: cl(client)
{
}

cmd_err_t
command_client_t::commit_transaction()
{
    error_reply* reply;
    DO_RPC(commit_transaction_rpc_1(0, cl), reply);
    return 0; /* success */
}

cmd_err_t
command_client_t::abort_transaction()
{
    error_reply* reply;
    DO_RPC(abort_transaction_rpc_1(0, cl), reply);
    return 0; /* success */
}

cmd_err_t
command_client_t::clear_grid()
{
    error_reply* reply;
    DO_RPC(clear_grid_rpc_1(0, cl), reply);
    return 0; /* success */
}

cmd_err_t
command_client_t::print_grid(grid_display_t& display)
{
    print_grid_reply* reply;
    DO_RPC(print_grid_rpc_1(0, cl), reply);
    display = reply->display;
    return 0; /* success */
}

cmd_err_t
command_client_t::add_item(const char* name, int x, int y)
{
    error_reply* reply;
    add_item_arg arg;

    strncpy(arg.name, name, sizeof(arg.name)-1);
    arg.x = x; 
    arg.y = y; 
    
    DO_RPC(add_item_rpc_1(&arg, cl), reply);
    return 0; /* success */
}

cmd_err_t
command_client_t::remove_item(const char* name)
{
    error_reply* reply;
    remove_item_arg arg;

    strncpy(arg.name, name, sizeof(arg.name)-1);
    
    DO_RPC(remove_item_rpc_1(&arg, cl), reply);
    return 0; /* success */
}


cmd_err_t
command_client_t::move_item(const char* name, int x, int y)
{
    error_reply* reply;
    add_item_arg arg;
    arg.x = x; 
    arg.y = y; 

    strncpy(arg.name, name, sizeof(arg.name)-1);
    
    DO_RPC(move_item_rpc_1(&arg, cl), reply);
    return 0; /* success */
}


cmd_err_t
command_client_t::location_of(const char* name, int& x, int& y)
{
    location_reply* reply;
    location_arg arg;

    strncpy(arg.name, name, sizeof(arg.name)-1);
    
    DO_RPC(location_of_rpc_1(&arg, cl), reply);
    x = reply->x;
    y = reply->y;
    return 0; /* success */
}


cmd_err_t
command_client_t::spatial_query(const nbox_t& box, spatial_result_t& result)
{
    spatial_reply* reply;
    spatial_arg arg;

    arg.x_low = box.bound(0);
    arg.y_low = box.bound(1);
    arg.x_hi = box.bound(0+box.dimension());
    arg.y_hi = box.bound(1+box.dimension());

    DO_RPC(spatial_rpc_1(&arg, cl), reply);
    result = reply->result;
    return 0; /* success */
}


