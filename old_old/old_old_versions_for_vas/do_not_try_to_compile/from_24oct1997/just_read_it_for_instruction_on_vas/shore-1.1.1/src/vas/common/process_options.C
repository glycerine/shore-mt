/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/process_options.C,v 1.10 1997/06/13 22:34:13 solomon Exp $
 */

#define __PROCESS_OPTIONS_C__
#include <copyright.h>
#ifndef W_OS2
#define W_UNIX
#endif
#include <config.h>
#include <stdlib.h>
#include <iostream.h>
#include <externc.h>
#include <debug.h>
#include <string.h>
#include <svas_error_def.h>
#include <os_error_def.h>

#ifdef SOLARIS2
// _POSIX_C_SOURCE needed on Solaris
#define _POSIX_C_SOURCE
#endif /* SOLARIS2 */

#include <limits.h>

#ifdef SOLARIS2
#undef _POSIX_C_SOURCE
#endif /* SOLARIS2 */

#include <svas_base.h>
#include <option.h>
#include <process_options.h>
#include <getopt.h>

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static
void usage(ostream& err_stream, const char* prog_name,
         bool long_form, option_group_t *options,
		 const char	*usage_string)
{
    if (!long_form) {
		options->print_usage(FALSE, err_stream);
    } else {
		err_stream << "Usage: " << prog_name << " " << usage_string << endl;
		err_stream << "Options:" << endl;
		options->print_usage(TRUE, err_stream);
    }
    return;
}

w_rc_t
process_options(
	option_group_t **res,
	int		   &argc,	// gets changd
	char  		*argv[], // changed

	// type is "shore"
	const char *progclass, // type.progclass.program.option
	const char *aout, 		// = 0, overrides argv[0]
	// options are many
	const char *rcfilename, // = 0
	const char *ustring,	// = ""
	setup_options_func func1, // = 0
	setup_options_func func2, // =0
	bool		dohv  // = true
)
{
	if(res == 0 ) { return RC(SVAS_BadParam1); }
	if(argv==-0 ) { return RC(SVAS_BadParam3); }
	*res = 0;

	if(progclass==0) { return RC(SVAS_BadParam4); }

	if(aout==0) { 
		aout = argv[0];
	}

	if(ustring==0) { return RC(SVAS_BadParam7); }

	option_group_t *options = new option_group_t(3);
	if(!options) {
		return RC(fcOUTOFMEMORY);
	}

	// Class Shore:: deletes options.
	w_rc_t	rc;
	*res = options;
	const char *prog_name;

	// set prog_name to the file name of the program
	{
		prog_name = strrchr(aout, '/');
		if (prog_name == NULL) {
				prog_name = aout;
		} else {
				prog_name += 1; /* skip the '/' */
				if (prog_name[0] == '\0')  {
						prog_name = aout;
				}
		}
		W_COERCE(options->add_class_level("shore"));
		W_COERCE(options->add_class_level(progclass));
		W_COERCE(options->add_class_level(prog_name));
	}
	DBG(<<"options name is " << options->class_name() );

	W_COERCE(svas_base::setup_options(options));

	// func1 is presumably oc_layer::setup_options()
	if(func1) {
		DBG(<<"invoking user-defined setoptions-1");
		W_COERCE((*func1)(options));
	}
	if(func2) {
		DBG(<<"invoking user-defined setoptions-2");
		W_COERCE((*func2)(options));
	}

	if(rcfilename) {
		/* Scan the default configuration files: 
		 * .rcfilename,
		 * $HOME/.rcfilename, 
		 * in that order.
		 * OS errors are ignored since it is not an error
		 * for this file to not be found.
		 */
		/* if file name is absolute, don't mess with $HOME */
		bool	is_relative = ! (rcfilename[0] == '/' || 
										rcfilename[0] == '~' );

		for(int file_num = 0; file_num < (is_relative?2:1) && !rc.is_error(); file_num++) {
			// scan default option files
			char            opt_file[_POSIX_PATH_MAX+1];
			if((file_num == 1) && is_relative) {
				w_assert3((is_relative) && (file_num == 1));

				if (!getenv("HOME")) {
					cerr << "Error: environment variable $HOME is not set" << endl;
				   rc = RC(SVAS_FAILURE);
					break;
				}
				if (sizeof(opt_file) <= 
					strlen(getenv("HOME")) + strlen("/") + strlen(rcfilename) + 1) {
					cerr << "Error: environment variable $HOME is too long" 
						<< endl;
					rc = RC(SVAS_FAILURE);
					break;
				}
				strcpy(opt_file, getenv("HOME"));
				strcat(opt_file, "/");
				strcat(opt_file, rcfilename);
			} else {
				w_assert3((!is_relative) || (file_num == 0));
				if(is_relative) {
					strcpy(opt_file, "./");
				} else {
					strcpy(opt_file, "");
				}
				strcat(opt_file, rcfilename);
			}
			DBG(<< "Trying to read file " << opt_file);
			option_file_scan_t opt_scan(opt_file, options);
			ostrstream      err_stream;
			rc = opt_scan.scan(TRUE, err_stream);
			err_stream << ends;
			if (rc) {
				// ignore OS error messages
				if (rc.err_num() == fcOS) {
					rc = RCOK;
				} else {
					// this error message is kind of gross but is
					// sufficient for now
					char* errmsg = err_stream.str();
					cerr << "Error in reading option file: " << rcfilename << endl;
					//cerr << "\t" << w_error_t::error_string(rc.err_num()) << endl;
					cerr << "\t" << errmsg << endl;
					delete errmsg;
				}
			} else { 
				// we found and read the file
				break;
			}
		}
	}
	{
        /*
         * If there has been no error so far, we read
         * the command line
         */
        if (!rc) {
			ostrstream      err_stream;

            // parse command line
            //
            // minimum length for recognition by parse_command_line() is
            // 3 (third argument) so that sm_, svas_ are recognized
            // but h, v are not
            //
            DBG(<<"Reading command line");

            rc = options->parse_command_line(argv, argc, 3, &err_stream);

#			ifdef DEBUG
			DBG(<<"argc=" << argc);
			for(int j=1; j<argc; j++) {
				DBG(<<"argv["<<j<<"]="<<argv[j]);
			}
#			endif

            err_stream << ends;
            char* errmsg = err_stream.str();
            if (rc) {
                cerr << "Error on command line " << endl;
                cerr << "\t" << errmsg << endl;
            }
            if (errmsg) delete errmsg;
        } else {
            DBG(<<"skipping reading command line");
        }
	}

	/*
	 * Assuming no error so far, check that all required options
	 * in option_group_t options are set.
	 */
	if (!rc) {
		ostrstream      err_stream;

		// check required options
		rc = options->check_required(&err_stream);
		err_stream << ends;
		char* errmsg = err_stream.str();
		if (rc) {
			cerr << "These required options are not set:" << endl;
			cerr << errmsg << endl;
			usage(cerr, prog_name, false, options, ustring);
		}
		if (errmsg) delete errmsg;
	}

	// Look for -h, -v
	// check command-line options even if error, so that
	// you can handle -h option
	//
	if(dohv) {
		int c;
		int save_opterr = opterr;
		int save_optind = optind;
		int roving_optind;
		bool user_flags, advance;

		opterr = 0; // disable error message
		optind = 1; // in case getopt() was used before

		// getopt returns '?' when it doesn't know
		// what to do with an argument, e.g. it has > 1 character
		// and isn't preceded by some 1-char arg with ":" in the 
		// control string.

		///////////////////////////////////////////////////
		// NB: getopt() allows user to combine options into
		// one word: -xyzhv
		// They're only processed as single-letter options
		// if preceded by a '-'.
		// We'd like to peel off the hv and leave the rest,
		// but that might not be possible
		///////////////////////////////////////////////////

		bool		remove=false;
		const char *control_string = "hv";

		user_flags 	= false;
		advance = false;
		roving_optind = optind;

		c = getopt(argc, argv, control_string);

		while (c != -1) {
			DBG(<<"getopt returned " << (char) c
				<< " optind=" << optind
				<<" optarg= " << (char *)(optarg?optarg:"null") );

			if(roving_optind < optind) {
				advance = true;
			}

			switch (c) {

				case 'v': /* "verbose" -- print values and continue */
					// print short form
					options->print_values(FALSE, cerr);
					remove = true;
					break;

				case 'h': /* "help" -- print usage + values, exit */
					usage(cerr, argv[0], TRUE, options, ustring);

					// print long form
					options->print_values(TRUE, cerr);
					exit(0);
					break;

				case '?':
					// any other letter in an argument beginning
					// with '-'
					user_flags = true;
					break;
			}

			/////////////////////////////////////
			// remove the argument (which has no 
			// associated value)
			// remove: true if we encountered an
			// argument to remove
			// advance: true when we're done processing
			// all the characters in argv[optind-1]
			// user_flags: true if other flags appear
			// in argv[optind-1]; ones that user
			// will have to process
			///////////////////////////////////// 
			if(remove && advance) {
				if(user_flags) {
					char *s = argv[optind-1];
					while(s = strpbrk(s,control_string)) {
						for(int j=0; s[j]!='\0' ; j++)   {
							s[j] = s[j+1];
						}
					}
				} else {
					for (int j = optind; j < argc; j++) {
						argv[j-1] = argv[j];
					}
					argc -= 1;
				}
			}
			roving_optind = optind;
			c = getopt(argc, argv, control_string);
		}
		DBG(<<"getopt returned " << (int) c );
		opterr = save_opterr;
		optind = save_optind;
	}
#	ifdef DEBUG
	DBG(<<"argc=" << argc);
	for(int j=1; j<argc; j++) {
		DBG(<<"argv["<<j<<"]="<<argv[j]);
	}
#	endif
	return rc;
}
