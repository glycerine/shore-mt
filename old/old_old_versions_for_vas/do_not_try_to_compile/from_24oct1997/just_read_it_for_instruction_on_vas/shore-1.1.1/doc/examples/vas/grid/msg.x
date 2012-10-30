/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

%#ifndef GRID_BASICS_H
#include "grid_basics.h"
%#endif /* !GRID_BASICS_H */

/* const MAXNAMELEN = MAX_NAME_LEN;*/
typedef char name_type_t[MAX_NAME_LEN];

typedef char error_msg_t[MAX_ERR_MSG_LEN];

struct error_reply {
    error_msg_t 		error_msg;
};

struct print_grid_reply {
    grid_display_t  	display;
    error_msg_t		error_msg;
};

struct add_item_arg {
    name_type_t		name;
    int			x;
    int			y;
};

struct remove_item_arg {
    name_type_t		name;
};

struct location_arg {
    name_type_t		name;
};

struct location_reply {
    int			x;
    int			y;
    error_msg_t		error_msg;
};

struct spatial_arg {
    int			x_low;
    int			y_low;
    int			x_hi;
    int			y_hi;
};

struct spatial_reply {
    spatial_result_t	result;
    error_msg_t		error_msg;
};

program GRID {
    version GRIDVERS {
	void	ping_rpc(void) 					= 0;
	error_reply	commit_transaction_rpc(void) 		= 101;
	error_reply	abort_transaction_rpc(void) 		= 102;
	error_reply	clear_grid_rpc(void) 			= 103;
	print_grid_reply print_grid_rpc(void) 			= 104;
	error_reply	add_item_rpc(add_item_arg)		= 105;
	error_reply	remove_item_rpc(remove_item_arg)	= 106;
	error_reply	move_item_rpc(add_item_arg)		= 107;
	location_reply  location_of_rpc(location_arg)		= 108;
	spatial_reply	spatial_rpc(spatial_arg)		= 109;
    } = 1;
} = 0x20000100;


#ifdef RPC_HDR

%#define MSG_H


%#ifdef RPC_SVC
%#ifdef __cplusplus

%/*
% * Maximum size of all replys
% * Used to create a sufficiently large a reply buffer in a thread.
% */
% const size_t thread_reply_buf_size = MAX(sizeof(error_reply),
%		          MAX(sizeof(print_grid_reply),
%			  MAX(sizeof(location_reply),
%			  sizeof(spatial_reply))));

%/* server dispatch function */
%extern "C" void grid_1(struct svc_req*, register SVCXPRT*);

%/* Server side of RPCs */
%extern "C" void* 		ping_rpc_1(void*, svc_req*);
%extern "C" error_reply*	commit_transaction_rpc_1(void*, svc_req*);
%extern "C" error_reply*	abort_transaction_rpc_1(void*, svc_req*);
%extern "C" error_reply*	clear_grid_rpc_1(void*, svc_req*);
%extern "C" print_grid_reply*	print_grid_rpc_1(void*, svc_req*);
%extern "C" error_reply*	add_item_rpc_1(add_item_arg*, svc_req*);
%extern "C" error_reply*	remove_item_rpc_1(remove_item_arg*, svc_req*);
%extern "C" error_reply*	move_item_rpc_1(add_item_arg*, svc_req*);
%extern "C" location_reply*	location_of_rpc_1(location_arg*, svc_req*);
%extern "C" spatial_reply*	spatial_rpc_1(spatial_arg*, svc_req*);
%#endif /*__cplusplus*/
%#endif /*RPC_SVC*/

%#ifdef RPC_CLNT
%#ifdef __cplusplus
%extern "C" void* ping_rpc_1(void*, CLIENT*);
%extern "C" error_reply*	commit_transaction_rpc_1(void*, CLIENT*);
%extern "C" error_reply*	abort_transaction_rpc_1(void*, CLIENT*);
%extern "C" error_reply*	clear_grid_rpc_1(void*, CLIENT*);
%extern "C" print_grid_reply*	print_grid_rpc_1(void*, CLIENT*);
%extern "C" error_reply*	add_item_rpc_1(add_item_arg*, CLIENT*);
%extern "C" error_reply*	remove_item_rpc_1(remove_item_arg*, CLIENT*);
%extern "C" error_reply*	move_item_rpc_1(add_item_arg*, CLIENT*);
%extern "C" location_reply*	location_of_rpc_1(location_arg*, CLIENT*);
%extern "C" spatial_reply*	spatial_rpc_1(spatial_arg*, CLIENT*);
%#endif /*__cplusplus*/
%#endif /*RPC_CLNT*/

#endif /*RPC_HDR*/
