/*
 * $Header: /p/shore/shore_cvs/src/vas/tests/client.C,v 1.20 1995/09/08 16:36:55 nhall Exp $
 * kludgy test program for shore vas layer
 */
/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include <copyright.h>
#include <tcl.h>
#include <interp.h>
#define XDR void
#include <externc.h>
#include <shore_vas.h>
#include <w.h>
#include <option.h>
#include <process_options.h>
#include <unistd.h>
#include <getopt.h>

#ifdef I860
BEGIN_EXTERNCLIST
#	include <nx.h>
END_EXTERNCLIST
#endif

extern "C" void	svas_set_shellrc(option_t *);

option_t *filename_opt=0;
w_rc_t my_setup_options(option_group_t *options) 
{ 
	w_rc_t e;
	option_t *opt_shellrc=0;

	// shell startup
	e = options->add_option("svas_shellrc", "string",
			"shore.rc",
			"<filename for vas startup file>(soon to be obsolete)",
			false, 
			option_t::set_value_charstr, opt_shellrc);

	if(!e) {
		svas_set_shellrc(opt_shellrc);

		// what to read after shell startup:
		e = options->add_option("inputfile", "string",
				"stdin",
				"stdin or file name",
				false, 
				option_t::set_value_charstr, filename_opt);
	}

	if(e) {
		cerr << "cannot set up my options:" << e << endl;
		exit(1);
	}

	return RCOK;
}

char *u="[-f inputfile] [-svas_shellrc startupfile]";

extern void prompt();

main(int argc, char* argv[])
{
    FILE* 		inf = stdin;
	option_group_t	*options;


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
	if(process_options(&options,
		argc, argv, "client", 0, getenv("SHORE_RC"), u, 
		my_setup_options)) {
		cerr << "Cannot process options" << endl;
		exit (1);
	}
	{
		extern int optind; extern char *optarg; int c;

		while ((c = getopt(argc, argv, "vhf:")) != -1) {
			switch (c) {
			case 'v':
				options->print_values(1, cout);
				break;

			case 'h':
				options->print_usage(1, cerr);
				exit(0);
				break;

			case 'f':
				if ((inf = fopen(optarg, "r")) == 0) {
					perror("fopen");
					inf = NULL;
				}
				break;

			default:
				goto usage;
			}
		} // while
	}
	if(!inf) {
usage:
		cerr << "Usage: " << argv[0] << " [-f input file]" <<  endl;
		exit(1);
	}

	interpreter_t *I = new interpreter_t(inf, 0);
	prompt();
	while( I->consume() );
	delete I;
	delete options;
}
