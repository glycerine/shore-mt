/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __PROCESS_OPTIONS_H__
#define __PROCESS_OPTIONS_H__

#include <w.h>
#include <option.h>

typedef w_rc_t (*setup_options_func)(option_group_t *);

// not the simplest, but simpler than doing it yourself :
extern w_rc_t 
process_options(
	option_group_t **res, // option group is returned in *<this-arg>
						// this arg may not be 0

	int		   &argc,	// gets changed
	char 		*argv[], // changed -- may not be 0

					// type.class.program.optionname

					// program type is "shore"

	const char *, 	// program class - e.g., "oo7"
					// (may not be 0)
					//
	const char *aout=0,	// program - if non-0, overrides argv[0]

					//
					// each layer adds its options
					//
	const char *rcfile=0,   // run command file name
					// if null, the function doesn't look for a file
					//
	const char *ustring="",// usage string-- is printed after program name
					// in event of error
					// may not be 0 but may be a null string ("")
	setup_options_func f=0, 
					// for OC layer -- may be 0
					// if 0, no function is called
	setup_options_func f=0,
					// for application layer -- may be 0
					// if 0, no function is called
	bool	dohv=true
					// if true, after parsing file and command
					// line for options, it uses getopt() to
					// look for h, -h, v, and -v.  h means
					// print values and quit.
					// v means print values and continue.
					// All other arguments generate error.
					//
					// if this arg is false, none of this
					// processing is done; caller does all
					// its own getopt()-style processing.
);
#endif /*__PROCESS_OPTIONS_H__*/
