/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/sm/ssh/client.cc,v 1.14 1997/06/15 10:32:01 solomon Exp $
 */
#include <stdlib.h>
// #include <stdio.h>
#include <iostream.h>
#include <strstream.h>
#include <stddef.h>
#include <unistd.h>
#include <tcl.h>
#include <string.h>
#include "sthread.h"
#ifdef USE_SCOMM
#    include "scomm.h"
#    include "service.h"
#endif USE_SCOMM
#include "sm_app.h"
#include "remote.h"
#include "debug.h"

#if defined(__GNUG__) && defined(Sparc)
//    extern "C" int getopt(int argc, char** argv, char* optstring);
    extern char *optarg;
    extern int optind, opterr;
#endif

static void process_stdin(Tcl_Interp* ip);
static int  server_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[]);
static int  ovt_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[]);
static int  t_exit(ClientData, Tcl_Interp* ip, int ac, char* av[]);
static int  t_echo(ClientData, Tcl_Interp* ip, int ac, char* av[]);

int 		verbose = false;
Tcl_Interp*	global_ip = 0;
Port    	*rctrl_port = NULL;
Node*		server_node;
int		server_port_id;

void usage(ostream& out, const char* prog)
{
    cerr << "usage: " << prog
	 << "-s server_name [-l] [-v] [-f script_file]" << endl;
    cerr << "\n\t-s: server name to connect to" << endl;
    cerr << "\n\t-l: use logical oid" << endl;
    cerr << "\n\t-v: verbose printing" << endl;
}

main(int argc, char* argv[])
{
    FUNC(main);
    bool 	use_logical_id = false;
    vid_t	volume;
    char*	vol_name = NULL;
    char*	server_host = NULL;
    int		result;

    int option;
    char* f_arg = 0;
    while ((option = getopt(argc, argv, "lvf:s:")) != -1) {
	switch (option) {
	case 'f':
	    f_arg = optarg;
	    break;
	case 'l':
	    cerr << "-l option not supported" << endl;
	    return 1;
	    use_logical_id = true;
	    break;
	case 'v':
	    verbose = true;
	    break;
	case 's':
	    server_host = optarg;
	    break;
	default:
	    usage(cerr, argv[0]);
	    return 1;
	}
    }

    if (!server_host) {
	cerr << "Error: be sure to give a -s parameter" << endl;
	usage(cerr, argv[0]);
	return 1;
    }

    /*
     * Start communication facility
     */
    Node::Init();
    Node::start();

    /*
     * Contact name server to find the server.
     */
    NameService ns;
    Address	server_addr;
    rc_t	rc;		 //return code
    rctrl_port = Port::get();

    /*
     * message buffers
     */
    rctrl_msg_t* conn_msg;
    Buffer msg_buf(sizeof(*conn_msg));
    conn_msg = (rctrl_msg_t*) msg_buf.start();

    char* server_name = new char[strlen(server_host) + strlen(".")
				 + strlen(rctrl_name) + 1];
    strcpy(server_name, server_host);
    strcat(server_name, ".");
    strcat(server_name, rctrl_name);

    rc = ns.lookup(server_name, &server_addr, &server_port_id);
    if (rc) {
	cerr << argv[0] << ": '" << server_name
	     << "' not in name server" << endl;
	return 1;
    }
    DBG( << "found server" << server_name );

    rc = Node::establish(server_addr, &server_node);
    if (rc) {
	cerr << argv[0] << ": establish with '" << server_name
	     << "' fails" << endl;
	return 1;
    }
    DBG( << "established with server");

    // listen for death of server
    server_node->notify(rctrl_port);

    // send connect message to server
    conn_msg->set_type(rctrl_msg_t::connect_msg);
    conn_msg->set_reply_port(rctrl_port->id());
    rc = server_node->send(msg_buf, server_port_id);
    if (rc) {
	cerr << "error in sending connect message" << endl;
	return 1;
    }
    DBG( << "sent connect message" );

    // receive session port
    int 	session_port;
    Node* 	from_node;
    rc = rctrl_port->receive(msg_buf, &from_node);
    if (rc) {
	cerr << "error in receiving reply to connect message" << endl;
	return 1;
    }
    DBG( << "received reply to connect message" );

    // new port to contact server on
    server_port_id = conn_msg->content.port;

    /*
     * Start up Tcl
     */
    global_ip = Tcl_CreateInterp();
    {
	char* start_up  = getenv("SM_LIBDIR");
	if (!start_up)  {
	    cerr << __FILE__ << ':' << __LINE__ << ':'
		 << " SM_LIBDIR environment variable not set" << endl;

	    W_FATAL(fcNOTFOUND);
	}
	char buf[256];
	ostrstream s(buf, sizeof(buf));
	s << start_up << "/ssh.tcl" <<      '\0';
	if (Tcl_EvalFile(global_ip, buf) != TCL_OK)  {
	    cerr << __FILE__ << ':' << __LINE__ << ':'
		 << " error in \"" << buf << "\" script" << endl;
	    cerr << global_ip->result << endl;
	    W_FATAL(fcINTERNAL);
	}

	Tcl_SetVar(global_ip, "verbose_flag",
		   verbose ? "1" : "0", TCL_GLOBAL_ONLY);

	char* args = Tcl_Merge(argc, argv);
	Tcl_SetVar(global_ip, "argv", args, TCL_GLOBAL_ONLY);
	free(args);

	Tcl_CreateCommand(global_ip, "sm", server_dispatch, 0, 0);
	Tcl_CreateCommand(global_ip, "sendserver", server_dispatch, 0, 0);
	Tcl_CreateCommand(global_ip, "ovt", ovt_dispatch, 0, 0);
	Tcl_CreateCommand(global_ip, "exit", t_exit, 0, 0);
	Tcl_CreateCommand(global_ip, "echo", t_echo, 0, 0);
    }

    /*
     * Set use logical id flag based on server's setting
     * Note that this interpreter is running as a client.
     */
    if (Tcl_Eval(global_ip, "set Use_logical_id [sendserver set Use_logical_id]") != TCL_OK) {
	cerr << global_ip->result << endl;
    }
    Tcl_SetVar(global_ip, "is_client", "1", TCL_GLOBAL_ONLY);
    
    /*
     * Process the Tcl file or stdin
     */
    if (f_arg) {
	// input from file, so just "source" it

	char* av[2];
	av[0] = "source";
	av[1] = f_arg;
	char* cmd = Tcl_Merge(2, av);
	int result = Tcl_Eval(global_ip, cmd);
	if (result != TCL_OK)  {
	    cerr << global_ip->result << endl;
	}
	free(cmd);

    } else {
	// input is from stdin

	process_stdin(global_ip);
    }

}

static void process_stdin(Tcl_Interp* ip)
{
    FUNC(process_stdin);
    char line[1000];
    int partial = 0;
    Tcl_DString buf;
    Tcl_DStringInit(&buf);
    int tty = isatty(0);

    while (1) {

	cin.clear();
	if (tty) {
	    char* prompt = Tcl_GetVar(ip, (partial ? "tcl_prompt2" :
					   "tcl_prompt1"), TCL_GLOBAL_ONLY);
	    if (! prompt) {
		if (! partial)  cout << "% " << flush;
	    } else {
		if (Tcl_Eval(ip, prompt) != TCL_OK)  {
		    cerr << ip->result << endl;
		    Tcl_AddErrorInfo(ip,
				     "\n    (script that generates prompt)");
		    if (! partial) cout << "% " << flush;
		} else {
		    fflush(stdout);
		}
	    }

	}
	cin.getline(line, sizeof(line) - 2);
	line[sizeof(line)-2] = '\0';
	strcat(line, "\n");
	if ( !cin) {
	    if (! partial)  {
		break;
	    }
	    line[0] = '\0';
	}
	char* cmd = Tcl_DStringAppend(&buf, line, -1);
	if (line[0] && ! Tcl_CommandComplete(cmd))  {
	    partial = 1;
	    continue;
	}
	partial = 0;
	int result = Tcl_RecordAndEval(ip, cmd, 0);
	Tcl_DStringFree(&buf);
	if (result == TCL_OK)  {
	    if (ip->result[0]) {
		cout << ip->result << endl;
	    }
	} else {
	    cerr << "client.C: Error";
	    if (result != TCL_ERROR) cerr << " " << result;
	    if (ip->result[0]) 
		cerr << ": " << ip->result;
	    cerr << endl;
	}
    }
}

server_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    FUNC(server_dispatch);

    static Buffer send_buf(sizeof(rctrl_msg_t));
    static Buffer rec_buf(sizeof(rctrl_msg_t));
    rctrl_msg_t*  send_msg = (rctrl_msg_t*) send_buf.start();
    rctrl_msg_t*  rec_msg  = (rctrl_msg_t*) rec_buf.start();
    int a;		// current argument being processed;
    rc_t rc;		// return code;
    
    // should always be zero
    send_msg->content.cmd.data[rctrl_sm_cmd_t::data_sz-1] = '\0';

    assert(ac > 1);

    //
    // Determine whether we should send the entire command
    // or just what comes after the first argument.
    //
    int start_arg;
    if (strcmp(av[0], "sm") == 0) {
	start_arg = 0;
    } else if (strcmp(av[0], "sendserver") == 0) {
	start_arg = 1;
    } else if (strcmp(av[0], "ovt") == 0) {
	start_arg = 0;
    } else {
	cerr << "unknown server command: " << av[0] << endl;
	W_FATAL(fcNOTFOUND);
    }
    
    Tcl_DString	cmd;
    Tcl_DStringInit(&cmd);
    char* cmd_string;

    // put the command into a string
    for (a = start_arg; a < ac; a++) {
	cmd_string = Tcl_DStringAppendElement(&cmd, av[a]);
    }

    DBG( << "sending: " << cmd_string);
    send_msg->set_type(rctrl_msg_t::command_msg);
    send_msg->set_reply_port(rctrl_port->id());
    rc = rctrl_msg_t::send_tcl_string(cmd_string, *server_node,
				      server_port_id, send_buf);
    if (rc) {
	cerr << "error in sending command message " << endl;
	W_COERCE(rc);
    }
    DBG( << "sent command message " );

    Tcl_DStringTrunc(&cmd, 0);

    // get response;
    Node* from_node;
    rc = rctrl_msg_t::receive_tcl_string(cmd, *rctrl_port, rec_buf, from_node);
    if (rc) {
	cerr << "error in receiving reply to command message" << endl;
	W_COERCE(rc);
    }
    DBG( << "received reply to command message,");

    DBG( << "received error code " << rec_msg->content.cmd.error_code
	 << " from reply");

    Tcl_DStringResult(ip, &cmd);  // set tcl command result to returned string

    return rec_msg->content.cmd.error_code;

    // This code got too complicated, so it was scrapped
    // at the cost of reduced performance for the above code
#ifdef NOTDEF
    int ac_msg = 1;	// count of arguments in current msg
    int b;  		// current byte in argument being processed
    int alen;		// length of current argument
    int moff = 0;	// current offset in msg buffer 
    int mleft = rctrl_sm_cmd_t::data_sz; // space left in msg buffer
    bool sent_full = false;  // last message sent was full
    
    
    send_msg->content.cmd.first_arg = 0;

    for (a = 0; a < ac; a++, ac_msg++) {
	b = 0;
	alen = strlen(av[a]);

	while (b < alen) {
	    int cpy_len;	// amount to copy to msg buf
	    if ((alen-b)+1 > mleft) {
		//rest of argument cannot fit, send only what's can be
		cpy_len = mleft - 1;
	    } else {
		cpy_len = alen - b;
	    }
	    memcpy(send_msg->content.cmd.data+moff, av[a]+b, cpy_len);
	    send_msg->content.cmd.data[moff+cpy_len] = 0;
	    assert(send_msg->content.cmd.overflow == 0);
   
	    mleft -= cpy_len + 1;
	    b	  += cpy_len;
	    if (mleft == 0) {
		// send msg
		send_rctrl_msg(part, ac_msg, send_msg);
		sent_full = true;
		part++;
		mleft = rctrl_sm_cmd_t::data_sz;
		moff = 0;
		ac_msg = 1;
		if (b == alen) {
		    // sent whole msg, so first arg on next msg is:
		    send_msg->content.cmd.first_arg = a+1;
		} else {
		    // still working on curr arg
		    send_msg->content.cmd.first_arg = a;
		}
	    }
	} // loop to send bytes of an argument
    } // loop to send all arguments

    if (sent_full) {
    	// last message was full, so send an extra one to end it
	send_msg->content.cmd.data[0] = 0;

	// message will just append 0 bytes to last argument
	assert(send_msg->content.cmd.first_arg == ac);
	send_msg->content.cmd.first_arg--;
	send_rctrl_msg(part, ac_msg, send_msg); 
    } else {
	assert(send_msg->content.cmd.first_arg < ac);
	send_rctrl_msg(part, ac_msg, send_msg); 
    }

    // get response;
    do {
	rec_rctrl_msg(rec_msg);
	Tcl_AppendResult(ip, rec_msg->content.data, 0);
    } while (rec_msg->content.argc == rctrl_sm_cmd_t::data_sz);

    return rec_msg->content.error_code;
#endif /*NOTDEF*/
}

ovt_dispatch(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    Tcl_AppendResult(ip, "ovt_dispatch is not implemented", 0);
    return TCL_ERROR;
}

t_exit(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    Tcl_AppendResult(ip, "t_exit is not implemented", 0);
    return TCL_ERROR;
}

static t_echo(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
    for (int i = 1; i < ac; i++) {
        cout << ((i > 1) ? " " : "") << av[i];
        Tcl_AppendResult(ip, (i > 1) ? " " : "", av[i], 0);
    }
    cout << endl;

    return TCL_OK;
}

