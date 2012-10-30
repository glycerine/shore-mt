/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/* command line arguments */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) && __GNUC_MINOR__<6
extern int getopt(int argc, char * const argv[], const char *optstring);
#endif

#ifndef __GNUC__
extern int getopt(int argc, char * const argv[], const char *optstring);
#endif

#ifdef __cplusplus
};
#endif

extern char *optarg;
extern int optind, opterr, optopt;

