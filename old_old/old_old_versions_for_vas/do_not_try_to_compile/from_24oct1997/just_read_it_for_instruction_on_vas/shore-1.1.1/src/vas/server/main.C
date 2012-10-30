/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/main.C,v 1.67 1997/01/24 16:48:01 nhall Exp $
 */
#include <copyright.h>

#ifdef notdef
#define NOSHELL
#define NOCLIENTS
#define NOEFSD
#define NOMOUNTD
#define NOREMOTE
#endif


#include <vas_internal.h>
#include <server_stubs.h>
#include <default_port.h>
#include <interp.h>
#include <tcl.h>
#include <process_options.h>

BEGIN_EXTERNCLIST
	// tcl stuff for handling commands from stdin
	void 	svc_run(); // in svc_run.C

#ifdef I860
#	include <nx.h>
#endif
END_EXTERNCLIST

static char *u = " [options]\n\
where options are these short (single-character) options: \n\
  h       : help \n\
  v       : print version \n\
\n\
and these long options: \n\
";

static
void usage(ostream& err_stream, const char* prog_name,
         bool long_form, option_group_t *options,
		 const char	*usage_string)
{
    if (!long_form) {
		options->print_usage(false, err_stream);
    } else {
		err_stream << "Usage: " << prog_name << " " << usage_string << endl;
		options->print_usage(true, err_stream);
    }
    return;
}

class startup_smthread_t : public smthread_t
{

public:
	uint4_t err_num;
	startup_smthread_t();
	~startup_smthread_t() {}
	void run();
};

main(
    int 	argc,
    char 	**argv
    // , char **envp
)
{
    FUNC(main);
    option_group_t *options;

    DBG( << "Entering main." );
#ifdef I860
    long _argc = argc;

    if(nx_initve("",0,"",&_argc, argv)<=0) {
	perror("cannot initialize: nx_initve");
	exit(1); // paragon
    }
#endif

    
    const char*  vasrc = ShoreVasLayer.configuration_file_name();
    DBG(<<"argc=" << argc);

#ifdef DEBUG
    { for(int i=1; i<argc; i++) { DBG(<<"argv["<<i<<"]="<<argv[i]); } }
#endif

    /* shore.server.<a.out>.option */
    if(process_options(&options, argc, argv, 
	    "server", 0, vasrc, u, ss_m::setup_options)) {
	    catastrophic("Cannot process options");
    }
#ifdef DEBUG
    DBG(<<"argc=" << argc);
    { for(int i=1; i<argc; i++) { DBG(<<"argv["<<i<<"]="<<argv[i]); } }
#endif


    ss_m	*_sm = NULL;
    /* main thread is no longer a standard sthread */

    startup_smthread_t *startup_thread = new startup_smthread_t();
    if(!startup_thread) {
		W_FATAL(fcOUTOFMEMORY);
    }
    W_COERCE(startup_thread->fork());
    W_COERCE(startup_thread->wait());

    if(startup_thread->err_num) {
       cerr << "exiting with value " << startup_thread->err_num <<endl;
    }
    delete startup_thread;

    delete options;
    DBG( << "EXITING MAIN THREAD");
}


startup_smthread_t::startup_smthread_t()
: smthread_t(t_regular, false, false, "startup_thread", WAIT_FOREVER )
{
}

void
startup_smthread_t::run()
{
	DBG(<<"calling new _ss_m");

	err_num = 0;

	ss_m *_sm;

	_sm = new ss_m();
	DBG(<<"have ssm");

	if(!_sm) {
		catastrophic("Cannot initialize the storage manager (1).");
	}
	DBG(<< "have sm");

	svas_layer *vaslayer;
	vaslayer = new svas_layer(_sm); // interprets options

	DBG(<< "made vas layer ");

	w_rc_t	e;

#ifndef NOMOUNTD
	if(e=vaslayer->configure_mount_service()) {
		catastrophic("Cannot configure Shore NFS service.");
		err_num = e.err_num();
		return;
	}
	DBG(<< "configured mount");
#endif

#ifndef NOEFSD
	if(e=vaslayer->configure_nfs_service( )) {
		catastrophic("Cannot configure Shore NFS service.");
		err_num = e.err_num();
		return;
	}
	DBG(<< "configured nfs");
#endif
#ifndef NOCLIENTS
	if(e=vaslayer->configure_client_service()) {
		catastrophic("Cannot configure Shore RPC service.");
		err_num = e.err_num();
		return;
	}
	DBG(<< "configured client");
#endif
#ifndef NOREMOTE
	if(e=vaslayer->configure_remote_service()) {
		catastrophic("Cannot configure Shore REMOTE service.");
		err_num = e.err_num();
		return;
	}
	DBG(<< "configured remote");
#endif

#ifndef  NOSHELL
	bool	istty = true;
	// bool	istty = isatty(0);
	if(istty) {
		if(ShoreVasLayer.make_tcl_shell ) {
			if(e=vaslayer->configure_tclshell_service()) {
				catastrophic("Cannot configure Tcl Shell service.");
				err_num = e.err_num();
				return;
			}
			DBG(<< "configured tcl shell");
		} else {
			DBG(<<"option says don't make a shell");
		}
	} else  {
		// don't print anything
		DBG(<< " not a tty so no shell");
	}

#else
	cerr << "Server configured without shell.   Detaching from terminal ..."
		<< endl;

#endif

#ifdef DEBUG
	// test_errors();
#endif

	DBG(<< "starting services");


	if(e=vaslayer->start_services()) {
		catastrophic("Cannot start shore vaslayer services.");
	}

	(void) svc_run(); // if you want to run the service, you have
						// to hand over main to this.
						// or rewrite svc_run()

	DBG(<<"returned from svc_run()");

	delete vaslayer; // cleans up all services

	DBG( << "deleting _sm" );
	delete _sm; 

} /* main */
