/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: remote.h,v 1.7 1995/04/24 19:41:14 zwilling Exp $
 */
#ifndef REMOTE_H
#define REMOTE_H

// name of server in name server
const char* const   rctrl_name = "rctrl";

struct rctrl_sm_cmd_t {
    enum { data_sz = 1000 };
    int part;  // message part 1,2,3,etc of the command (-# == last)
#ifdef NOTDEF
    int	argc;  // number of argument strings passed
	       // or length of returned result
    int first_arg; // argument # for first string
#endif /*NOTDEF*/
    int error_code;
    char data[data_sz];
};

union rctrl_msg_content_u {
    int 		port;
    rctrl_sm_cmd_t 	cmd;
};

class rctrl_msg_t {
public:
    enum msg_type {bad_msg = 0, connect_msg, disconnect_msg, command_msg, result_msg, sm_shutdown_msg};
    rctrl_msg_t(msg_type t)		{_type = t; _len = 0;}
    rctrl_msg_t()			{_type = bad_msg; _len = 0;}
    void	set_length(int len) 	{ _len = len; }
    void	set_type(msg_type t)	{ _type = t; }
    void	set_reply_port(int r)	{ _reply_port = r; }
    msg_type 	type() const 		{return _type;}
    int 	length() const		{return _len;}
    int		reply_port()		{return _reply_port;}

    static 	rc_t send_tcl_string(char* str, Node& node, int portId, Buffer& buf);
    static 	rc_t receive_tcl_string(Tcl_DString& strD, Port& port, Buffer& buf, Node*& from_node);

private:
    msg_type 	_type;
    int 	_len;
    int		_reply_port;

public:
    rctrl_msg_content_u content;
};

#endif /* REMOTE_H */
