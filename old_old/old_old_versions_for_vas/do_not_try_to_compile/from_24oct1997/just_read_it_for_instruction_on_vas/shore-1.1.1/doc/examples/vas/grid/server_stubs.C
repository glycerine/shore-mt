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
/* 
 * NB: THIS IS IMPORTANT: we include "ShoreConfig.h" because
 * we *NEED* the system-dependent definition of a jmp_buf
 * in order to see that the thread data structures that
 * that we build here are consistent with those in the library.
 * If we don't get the right #defines for the configuration, we
 * run the risk of building .o files here with the wrong idea
 * about the size of sthread_t (the root of the class hierarchy
 * for our threads).
 */

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

/********************************************************************
   RPC Stubs that call command_server_t methods for processing
 *******************************************************************/

// there is no command_server_t method for ping
void *
ping_rpc_1(void* , svc_req* )
{
    return (void *)client_t::me()->reply_buf;
}

error_reply *
commit_transaction_rpc_1(void* , svc_req* )
{
    client_t* Me = client_t::me();

    error_reply& reply = *(error_reply*)Me->reply_buf;

    cmd_err_t err = Me->command_server->commit_transaction();
    if (err) {
	strncpy(reply.error_msg, err, MAX_ERR_MSG_LEN);
	reply.error_msg[MAX_ERR_MSG_LEN-1] = '\0';
    } else {
	// no error message
	reply.error_msg[0] = '\0';
    }
    return &reply;
}

error_reply *
abort_transaction_rpc_1(void* , svc_req* )
{
    client_t* Me = client_t::me();

    error_reply& reply = *(error_reply*)Me->reply_buf;

    cmd_err_t err = Me->command_server->abort_transaction();
    if (err) {
	strncpy(reply.error_msg, err, MAX_ERR_MSG_LEN);
	reply.error_msg[MAX_ERR_MSG_LEN-1] = '\0';
    } else {
	// no error message
	reply.error_msg[0] = '\0';
    }
    return &reply;
}


error_reply *
clear_grid_rpc_1(void* , svc_req* )
{
    client_t* Me = client_t::me();
    error_reply& reply = *(error_reply*)Me->reply_buf;

    cmd_err_t err = Me->command_server->clear_grid();

    if (err) {
	strncpy(reply.error_msg, err, MAX_ERR_MSG_LEN);
	reply.error_msg[MAX_ERR_MSG_LEN-1] = '\0';
    } else {
	// no error message
	reply.error_msg[0] = '\0';
    }
    return &reply;
}


print_grid_reply *
print_grid_rpc_1(void* , svc_req* )
{
    client_t* Me = client_t::me();

    print_grid_reply& reply = *(print_grid_reply*)Me->reply_buf;

    cmd_err_t err = Me->command_server->print_grid(reply.display);
    if (err) {
	strncpy(reply.error_msg, err, MAX_ERR_MSG_LEN);
	reply.error_msg[MAX_ERR_MSG_LEN-1] = '\0';
    } else {
	// no error message
	reply.error_msg[0] = '\0';
    }
    return &reply;
}


error_reply *
add_item_rpc_1(add_item_arg* argp, svc_req* )
{
    client_t* Me = client_t::me();

    error_reply& reply = *(error_reply*)Me->reply_buf;

    cmd_err_t err = Me->command_server->add_item(argp->name, argp->x, argp->y);
    if (err) {
	strncpy(reply.error_msg, err, MAX_ERR_MSG_LEN);
	reply.error_msg[MAX_ERR_MSG_LEN-1] = '\0';
    } else {
	// no error message
	reply.error_msg[0] = '\0';
    }
    return &reply;
}


error_reply *
remove_item_rpc_1(remove_item_arg* argp, svc_req* )
{
    client_t* Me = client_t::me();

    error_reply& reply = *(error_reply*)Me->reply_buf;

    cmd_err_t err = Me->command_server->remove_item(argp->name);
    if (err) {
	strncpy(reply.error_msg, err, MAX_ERR_MSG_LEN);
	reply.error_msg[MAX_ERR_MSG_LEN-1] = '\0';
    } else {
	// no error message
	reply.error_msg[0] = '\0';
    }
    return &reply;
}


error_reply *
move_item_rpc_1(add_item_arg* argp, svc_req* )
{
    client_t* Me = client_t::me();

    error_reply& reply = *(error_reply*)Me->reply_buf;

    cmd_err_t err = Me->command_server->move_item(argp->name, argp->x, argp->y);
    if (err) {
	strncpy(reply.error_msg, err, MAX_ERR_MSG_LEN);
	reply.error_msg[MAX_ERR_MSG_LEN-1] = '\0';
    } else {
	// no error message
	reply.error_msg[0] = '\0';
    }
    return &reply;
}


location_reply *
location_of_rpc_1(location_arg* argp, svc_req* )
{
    client_t* Me = client_t::me();

    location_reply& reply = *(location_reply*)Me->reply_buf;

    cmd_err_t err = Me->command_server->location_of(argp->name, reply.x, reply.y);
    if (err) {
	strncpy(reply.error_msg, err, MAX_ERR_MSG_LEN);
	reply.error_msg[MAX_ERR_MSG_LEN-1] = '\0';
    } else {
	// no error message
	reply.error_msg[0] = '\0';
    }
    return &reply;
}


spatial_reply *
spatial_rpc_1(spatial_arg* argp, svc_req* )
{
    client_t* Me = client_t::me();

    spatial_reply& reply = *(spatial_reply*)Me->reply_buf;

    // the spatial_arg is really an array of integers in the format
    // for an nbox_t
    nbox_t box(2, (int*)(&argp->x_low));

    cmd_err_t err = Me->command_server->spatial_query(box, reply.result);
    if (err) {
	strncpy(reply.error_msg, err, MAX_ERR_MSG_LEN);
	reply.error_msg[MAX_ERR_MSG_LEN-1] = '\0';
    } else {
	// no error message
	reply.error_msg[0] = '\0';
    }
    return &reply;
}

