/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/client/sshutdown.C,v 1.7 1995/07/14 22:37:38 nhall Exp $
 */

#include <copyright.h>
#include <tcl.h>
#include <interp.h>
#define XDR void
#include <externc.h>
#include <shore_vas.h>
#include <process_options.h>
#include <unistd.h>
#include <getopt.h>
#include <debug.h>

#ifdef I860
BEGIN_EXTERNCLIST
#	include <nx.h>
END_EXTERNCLIST
#endif

char *u="";

extern "C" void do_shutdown(
	const char *host, u_short port, bool use_pmap,
	bool	crash, bool wait
	);

option_t 	*host_opt, *port_opt, *pmap_opt;

w_rc_t 
my_setup_options(option_group_t *options) 
{ 
	DBG( << "my_setup_options" );

	w_rc_t e;

	e = options->add_option("svas_remote_port", "1024 < integer < 65535", 
			"2998", "port for communication between SVASs",
			false, 
			option_t::set_value_long, port_opt);

	if(!e) {
		e = options->add_option("svas_remote_pmap", "yes/no", "no",
		"yes means the server registered its service with the portmapper",
			false, 
			option_t::set_value_bool, pmap_opt);
	}

	if(e) {
		cerr << "cannot set up my options" << e << endl;
		exit(1);
	}

	return RCOK;
}

main(int argc, char* argv[])
{
	option_group_t	*options;
	bool	wait=true;
	bool	crash=false;
	bool	saww=false;
	bool	sawc=false;
	bool	sawi=false;

#ifdef OSF1AD
	 long _argc = (long) argc;
	if(nx_initve("",0,"",&_argc,argv) <=0) {
		perror("can't initialize");
		exit(1);
	}
#endif

	//
	// options
	//

	/* shore.client.sshutdown.optionname */

	if(process_options(&options, 
		argc, argv,
		"client", 0, getenv("SHORE_RC"), u, 
		my_setup_options,  0,
		true // do getopt() processing for -h, -v
	)) { 
		cerr << "Cannot process options" << endl;
		exit (1);
	}

#		ifdef DEBUG
		DBG(<<"argc=" << argc);
		for(int j=1; j<argc; j++) {
			DBG(<<"argv["<<j<<"]="<<argv[j]);
		}
#		endif

	{
		int c;
		optind = 1;
		const char *control_string = "wic";

		opterr = 1;

		DBG(<<"argv[optind]=" << argv[optind]);

		c = getopt(argc, argv, control_string);
		DBG(<<"getopt returns " << c);
		for ( 
			// c = getopt(argc, argv, control_string)
			;
				c != -1;
				c = getopt(argc, argv, control_string) ) {

			cerr << "c = " << (int) c <<endl;
			if(c!= -1) switch (c) {
			case 'i': // immediate
				DBG(<<"IMMEDIATE");
				if(saww || sawc) {
					cerr << "-c, -i and -w are mutually exclusive" << endl;
					exit(1);
				}
				wait = false;
				crash = false;
				sawi=true;
				break;

			case 'w':
				DBG(<<"WAIT");
				// TODO
				cerr << "-w is not yet implemented." << endl;
				cerr << "using -i." << endl;
				// TODO: FIX below

				if(sawi || sawc) {
					cerr << "-c, -i and -w are mutually exclusive" << endl;
					exit(1);
				}
				wait = true;
				crash = false;
				saww=true;
				break;

			case 'c':
				DBG(<<"CRASH");
				if(sawi || saww) {
					cerr << "-c, -i and -w are mutually exclusive" << endl;
					exit(1);
				}
				wait = false;
				crash = true;
				sawc=true;
				break;

			case '?':
				DBG(<<"??????");
				cerr << "Unknown command-line argument: \""
						<< (char) c << '"' << endl;
				options->print_usage(1, cerr);
				exit(0);
				break;

			default:
				DBG(<<"DEFAULT");
				cerr << "Usage " << argv[0] 
					<< " [" << control_string << "]" << endl;
				exit(1);
			}
		} // for
	}
	option_t *t;
	w_rc_t e = options->lookup("svas_host", true, t);
	if(e) {
		cerr << e << endl;
		exit(1);
	}
	host_opt = t;
//	dassert(t == host_opt) ;
	if(!host_opt) {
		cerr << argv[0] << " needs value for option host " <<  endl;
		exit(1);
	}

	e = options->lookup("svas_remote_pmap", true, t);
	if(e) {
		cerr << e << endl;
		exit(1);
	}
	dassert(t == pmap_opt) ;
	e = options->lookup("svas_remote_port", true, t);
	if(e) {
		cerr << e << endl;
		exit(1);
	}
	dassert(t == port_opt) ;

	const char 	*host = host_opt->value();
	u_short 	port = strtol(port_opt->value(),0,0);
	bool		use_pmap, bad;
	if(pmap_opt) {
		use_pmap = option_t::str_to_bool(pmap_opt->value(), bad);
		if(bad) use_pmap = false;
	}

	if(crash) {
		cerr << "Crashing " ;
	} else {
		cerr << "Shutting down" ;
	}
	cerr << "server on " << host << ", port " << port ;

	if(use_pmap) {
		cerr	<< " via portmapper";
	}
	if(wait) {
		cerr << " after clients finish.";
	} else {
		cerr << " immediately (aborting clients).";
	}
	cerr << endl;

	assert(!(crash && wait));

	// TODO: fix
	if(!crash) { wait = true; } // wait acts more like
		// immediate; immediate dumps core

	do_shutdown(host, port, use_pmap, crash, wait);
}

