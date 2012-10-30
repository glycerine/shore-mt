#ifndef _SHORE_CONFIG_H_
#define _SHORE_CONFIG_H_

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/********************************************************************
 Example config.h

 Edit this file to select options and to set configuration information
 for a specific site.

 This file is used both by ../tools/makemake to generate Makefiles and
 as a header file #included by most C and C++ sources.  It should contain
 only C-style comments, and #define or #undef C preprocessor directives.

********************************************************************/

/* The following line will appear in each generated Makefile.  If you do
 * not use RCS or CVS, you may want to edit it to document the configuration
 * version.
%#>>>>>>>>>>>> $Header: /p/shore/shore_cvs/config/config.h,v 1.6 1997/09/14 11:46:20 solomon Exp $
 */

/* Define IncludeDebugCode to force DEBUG to be defined at compile-time so that
 * source code protected by #ifdef DEBUG is included.  This is code that
 * helps with debugging but may make the program run more slowly.
 */
#define IncludeDebugCode

/* Define Debugging to compile with the -g flag or whatever is appropriate
 * for the particular platform.  Undefine it to make the compiled programs
 * smaller, saving disk space.  This option should have no effect on runtime
 * speed or size.
 */
#define Debugging

/* Define Optimize to compile with -O2 or whatever is appropriate for
 * the particular platform.  Defining it should make compilation
 * slower, but runtime faster.  It may also interfer with debugging
 * using a debugger.  On most platforms, -O (i.e., -O1) is used if
 * Optimize is not set, since with GNU cc this mild level of optimization
 * does not interfere with debugging and might even help.
 */
#undef Optimize

/* Define RcDebugging to make functions return a "return code" that
 * actually contains a set of return codes, one from each layer.  It
 * allows more informative error messages, but slows things down.
 */
#define RcDebugging

/* Set MiscCompilerFlags to any additional flags that should be
 * included when compiling C++ or C code.   (These flags are NOT included
 * when linking).  By default, it is empty.
 */
#define MiscCompilerFlags

/* Tools from PureAtria (see below). */
#undef UsePurify
#undef UseQuantify
#undef UsePureCoverage

/* By default, "make install" installs things in Top/installed.
 * Define InstallDir (and make sure the directory exists) if you would
 * like to override this.  Note that InstallDir should be defined to
 * be the name of a directory, as in
 *    #define InstallDir /tmp/shore
 */
#undef InstallDir

/**********************************************************************
 * Locations of software packages used in the building of Shore.  Whenever
 * possible, the definitions assume executables are in the PATH of the user who
 * runs "make" (e.g., #define Gcc gcc).  You can replace these definitions with
 * absolute path names if appropriate (e.g.,
 * #define Gcc /usr/local/gcc-2.7.2/bin/gcc).  When absolute pathname are
 * necessary, the initial configuration describes the locations of these
 * packages at the University of Wisconsin.  You will almost certainly have to
 * modify these definitions for your site.  However, if you are not using a
 * particular feature (see the comment associated with each parameter), you can
 * leave the definition unchanged.
 **********************************************************************/

/***** GNU tools, available from the Free Software Foundation
 ***** http://www.fsf.org
 *****/
/* The Gnu C and C++ compilers.  See the Shore Installation manual to learn
 * what version of gcc is required for this version of Shore.
 * You can use absolute pathnames if you like.
 */
#define Gcc				gcc
#define GPlusPlus		g++

/* Bison -- a parser generator similar to YACC.  Used only in src/sdl */
#define Bison			bison

/* Flex -- a scanner generator similar to LEX.  Used only in src/sdl */
#define Flex			flex
#define FlexLib			/s/flex/lib/libfl.a
#define FlexInclude		/s/flex/include

/* Gdbm -- the GNU version of the the dbm hash-file package.
 * Only needed if USE_VERIFY is defined (it shouldn't be!)
 */
#define GdbmInclude		/s/gdbm/include
#define GdbmLib			/s/gdbm/lib

/* Ctags -- create tags for use in vi or emacs.  Only needed if
 * you intend to make and use tags ("make tags").
 */
#define Ctags			ctags

/***** TCL -- The Tool Command Language, used to give a "console" interface to
 ***** the storage managager.  See http://www.sunlabs.com/research/tcl
 *****/
#define TclInclude		/s/tcl/include
#define TclLib			/s/tcl/lib/libtcl.a

/***** Perl5 -- used for a lot of general configuration and house-cleaning
 ***** functions.  In particular, Perl is used to generate the Makefiles.
 ***** Without a working version of Perl5, you will not be able to do anything.
 ***** The plan is eventually to replace TCL, ksh, etc. with Perl, to
 ***** decrease the number of pre-requisite software packages.
 ***** See http://www.perl.com/perl/
 *****/
#define Perl			perl

/***** Tools from PureAtria http://www.pureatria.com, used by the
 ***** Shore delopers to improve the quality of the product, but not
 ***** required to build the system.
 ***** If you do not define UsePurify, UseQuantify, or UsePureCoverage
 ***** above, you do not to change any of the following.
 *****/
#define Purify			/s/purify/bin
#define Quantify		/s/quantify/bin
#define PureCoverage	/s/purecov/bin

#if defined(UsePurify) || defined(UseQuantify) || defined(UsePureCoverage)
#define UsePure
#endif

#ifdef UsePurify
#define PureFlags \
	-inuse-at-exit -ignore-signals=0x40000000 -first-only -leaks-at-exit \
	-pointer-mask=0xfffffffe -copy-fd-output-to-logfile=1,2 \
	-threads -thread-stack-change=0x3000 -max_threads=64
/* Note that thread-stack-change is 12K and stack size is 64K.
 * This is necessary since purify will not always notice stack
 * changes if the change-size is near the stack size.
 */
#endif

/**********************************************************************
 * End of general configuration options.   The remainder of the       *
 * definititions in this file are to turn on options that are either  *
 * experimental or obsolete.  Do not turn them on unless you know     *
 * what you're doing.  In most cases, the default configuration is    *
 * "undef".  The only current exceptions are                          *
 *    #define SM_PAGESIZE 8192                                        *
 *    #define OBJECT_CC                                               *
 *    #define PIPE_NOTIFY                                             *
 **********************************************************************/

/* CHEAP_RC is supposed to generate a cheaper version of the w_rc_t type.
 * According to Nancy, it is currently broken.
 */
#undef CHEAP_RC

/*
 * The LOCAL_LOG flag causes the log manager to be built
 * so that it performs I/O locally (in the server process)
 * rather than delegating I/O to a diskrw process.
 * This is useful for debugging and also for servers that
 * are intended to support at most one client or thread
 * that's doing logging at any one time.
 *
 * The LOCAL_LOG compile-time option has been replaced with
 * the SM_LOG_LOCAL runtime options.  Its an environment
 * variable.  If it has a non-zero numeric value, a local
 * log will be used.
 */

/*
 * This defines the SM page size in bytes
 */
/* for sources */
#define SM_PAGESIZE 8192

/*
 * If you want 64 bit serial numbers (logical IDs)
 * rather that 32 bit, define this:
 */
/* for sources */
#undef BITS64

/*
 * If parsets are being used, set define these:
 */
/* for Imakefiles */
#undef FORCE_PARSETS
#undef USE_PVM3
/* for sources */
#undef SLAVEPROGRAM
#undef PARSETS

/*
 * To use hatehints and ship pages back to the primary server
 * define this:
 */
/* both sources and Imakefiles */
#undef USE_HATESEND

/*
 * For a multi-user version of OO7 define this:
 */
#undef FORCE_MULTIUSER

/*
 * Define this so that SVAS testing uses the verify library
 */
/* both sources and Imakefiles */
#undef USE_VERIFY

/*
 * Define this if your code uses TRUE and FALSE instead of true
 * and false, and you don't want to convert it yet.
 */
#undef BOOL_COMPAT

/*
 * Define this if you want to include Joe Hellerstein's 
 * experimental "russian doll" trees.  These are similar to
 * R*trees except that they are used for indexing set-valued
 * attributes.
 */
#undef USE_RDTREE

/*
 * Define this if you want to use the server-to-server coordination
 * stuff for two-phase commit.
 */
#undef USE_COORD

/*
 * Define these if you are using a server-to-server configuration
 */
/* both sources and Imakefiles */
#undef MULTI_SERVER

/*
 * Say whether threads are preemptive or not.  This is a hack to
 * reduce the size of lock manager objects by changing the locking
 * hierarchy to take advantage of the non-preemptive threads.
 */
#undef NOT_PREEMPTIVE

/*
 * OCOMM package aside, if you want to use PVM, define this
 */
#undef USE_PVM3

/*
 * The new object-oriented communications system
 */
#undef USE_OCOMM
#undef OCOMM_USE_TCP
#undef OCOMM_USE_PVM
#undef OCOMM_USE_UDP
#undef OCOMM_USE_MYRINET
#undef OCOMM_USE_MPI

/*
 * Define whether object-level concurrency control can be an option.
 * The actual level of locking is determined by the cc_alg variable defined
 * in sm_base.h.
 */
/* for sources */
#define OBJECT_CC

/*
 * Define whether hierarchical callbacks are allowed
 */
/* for sources */
#undef HIER_CB

/*
 *  Should be PIPE_NOTIFY - alternative is to use
 *  signals for diskrw -- server communication (worse performance)
 */
#define PIPE_NOTIFY

/*
 * The storage manager was changed to use an explicit synchronization
 * instead of trying to block and unblock threads to sychronize.
 * If this causes problems, OLD_SM_BLOCK will revert to the old
 * behaviour.
 */
#undef OLD_SM_BLOCK

/*
 * Print the output of various ssh queries.    If it is not defined
 * the shell will execute the queries, but it won't print the resulting
 * data.  This Could be replaced with a tcl variable in the ssh.
 */
#undef SSH_VERBOSE

/*
 * "Error Systems" account for subsystems which have their
 * own large error number space; large enough that it can't be
 * mixed with the shore error space.  Or, there maybe various
 * subsystems which duplicate error ranges.  "Error Systems"
 * allow these to coexist.  With full error information from each system.
 * 
 * However, for backward compatability, there is a really really small
 * hit when generating error codes over what previously occured.
 * This only happens when errors are generated.
 * If you feel that it is taking away performance, you can
 * disable "Error Systems" be defining NO_ERROR_SYSTEMS. The
 * downside of this is that you will no longer know which
 * error the subsystem generated.
 *
 * This will go away when the compatability stuff is no longer needed.
 */
#undef NO_ERROR_SYSTEMS
#endif /*  _SHORE_CONFIG_H_ */
