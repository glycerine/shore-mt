#include "oo7options.h"
#include <Shore.h>

char 	*configfile;
option_t *configfile_opt=0;
option_t *debug_opt=0;
bool	debugMode=0;

w_rc_t 
setup_options(option_group_t *options) 
{
	W_DO(options->add_option("configfile", "string",
		"oo7.config.tiny",
		"name of configuration file",
		false,
		option_t::set_value_charstr, configfile_opt));

	W_DO(options->add_option("debug", "string",
		"no",
		"yes turns on debugging",
		false,
		option_t::set_value_bool, debug_opt));

	return RCOK;
}

w_rc_t
initialize(
	int &argc,
	char **argv,
	const char *usage1
)
{
	option_group_t  *options;
	const char		*rcfile = ".oo7rc";
	const char		*progclass = "oo7";
	w_rc_t	rc;
	rc = Shore::process_options(argc, argv, progclass, 0, rcfile,
			     usage1, setup_options, &options, true);
	if(rc){
		cerr << "Cannot process options:" << rc << endl;
		exit (1);
	}
	if(debug_opt) {
	    if(option_t::str_to_bool(debug_opt->value(),debugMode)) {
		debugMode = 0;
	    } 
	} else {
	    debugMode = 0;
	}

	if (argc < 1) {
	    if(configfile_opt) {
		configfile = (char *)configfile_opt->value();
	    } else {
		fprintf(stderr, "Usage: %s %s\n", argv[0], usage1);
		exit(1);
	    }
	} else {
	    configfile = argv[1];
	}

	// options already set up.
	rc = Shore::init();
	if(rc){
	    cerr << "can't initialize object cache:" << rc << endl;
	}
	return rc;
}
