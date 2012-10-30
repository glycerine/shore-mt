/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/sm/ssh/remote.cc,v 1.7 1997/06/15 10:32:03 solomon Exp $
 */

#define REMOTE_C

#include <stdlib.h>
// #include <stdio.h>
#include <iostream.h>
#include <strstream.h>
#include <unistd.h>
#include <tcl.h>
#include <string.h>
#include "sthread.h"
#include "scomm.h"
#include "service.h"
#include "remote.h"
#include "debug.h"

//
// It is assume that buf points to an initialized rctrl_msg_t
// and that the return port ID is in the buf.
//
rc_t rctrl_msg_t::send_tcl_string(char* str, Node& node,
			     int port_id, Buffer& buf)
{
    FUNC(rctrl_msg_t::send_tcl_string);

    int		total = strlen(str); 	// total to send
    int     	i = 0;  		// bytes sent so far
    		// max bytes we can send
    const int	max_send = rctrl_sm_cmd_t::data_sz-1;
    int     	part = 0;               // msg is part X of whole string
    int     	send_cnt;		// amount sent in current msg
    rctrl_msg_t& msg = *(rctrl_msg_t*) buf.start();
    msg.content.cmd.data[rctrl_sm_cmd_t::data_sz-1] = '\0';

    do {
        part++;
        send_cnt = (max_send < total-i) ? max_send : total-i;   
        strncpy(msg.content.cmd.data, str+i, send_cnt);
        assert(msg.content.cmd.data[rctrl_sm_cmd_t::data_sz-1] == '\0');
        i += send_cnt;
	msg.content.cmd.data[send_cnt] = 0;

        if (i == total) {
            // sending last one
            part = 0-part;
        }
        msg.content.cmd.part = part; 

        // send the message
        W_DO(node.send(buf, port_id));
        DBG( << "sent message part: " << part );

    } while (i < total) ;
    assert(total == 0 ||
	   part == 0 - ((total / max_send) + ((total % max_send) ? 1 : 0)));

    return RCOK;
}

//
// It is assumed that buf points to a rctrl_msg_t.  buf will
// contain the last message received.
//
rc_t rctrl_msg_t::receive_tcl_string(Tcl_DString& strD, Port& port,
				Buffer& buf, Node*& from_node)
{
    FUNC(rctrl_msg_t::receive_tcl_string);

    int     	part = 0;               // msg is part X of whole string
    rctrl_msg_t& msg = *(rctrl_msg_t*) buf.start();
    
    do {
        part++;

        W_DO(port.receive(buf, &from_node));
        assert(msg.content.cmd.data[rctrl_sm_cmd_t::data_sz-1] == '\0');

        DBG( << "received reply to command message part " << part);

        Tcl_DStringAppend(&strD, msg.content.cmd.data, -1);
        if (msg.content.cmd.part < 0) {
            assert(part == - msg.content.cmd.part);
        } else {
            assert(part == msg.content.cmd.part);
        }
    } while (msg.content.cmd.part > 0);

    return RCOK;
}
