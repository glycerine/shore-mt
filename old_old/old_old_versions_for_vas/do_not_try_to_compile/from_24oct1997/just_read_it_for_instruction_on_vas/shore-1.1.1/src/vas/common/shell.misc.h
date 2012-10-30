/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __SHELL_MISC_H__
#define __SHELL_MISC_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/shell.misc.h,v 1.26 1996/03/04 16:17:47 nhall Exp $
 */
#include <copyright.h>

#ifdef Linux
#include <features.h>
#endif

#undef _CLIENTDATA
#include <interp.h>
#include <unix_stats.h>
extern unix_stats &shell_rusage;

#include <tcl.h>
#include <debug.h>
#include <ctype.h>

/* shell.h is generated */
#include <shell.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include "uname.h"

#ifdef SUNOS41
extern "C"  int strncasecmp(const char *, const char *, int);
extern "C"	int strcasecmp(const char *, const char *);
#endif

#include <unistd.h>
#include <stream.h>
#include <strstream.h>
#include <errors.h>

// kludge:
#define RPCGEN
// #include <sm_df.h>
#undef RPCGEN
#include <vas_internal.h>

#ifdef USE_VERIFY
#include "verify.h"
#endif

// grot - redefined
enum transGoal {g_prepare, g_commit, g_abort
	, g_lazycommit, g_commitchain
	, g_suspend, g_resume 
};

typedef void (*USAGEFUNC)(ostream &out);
typedef enum { tcl_ok, tcl_error} CMDResult; 

extern "C" {

	/* part of the specific shell */
	void notfound(ClientData clientdata);
	int get_objsize( // Tcl_Interp* ip, svas_base *v, 
		char *av, ObjectSize 	*c, ObjectSize	*h, ObjectOffset *tst,
			int *nindexes);
	int print_statistics(ClientData,Tcl_Interp* ip, int mode);
	int clear_statistics(ClientData);

	/* part of the generic shell */
	int cmdreply(ClientData cd, Tcl_Interp *ip, CMDResult q, const char *c) ;
	CMDResult SyntaxError( Tcl_Interp 	*ip, char		*msg, char		*info);
	int check(ClientData,Tcl_Interp* ip, int ac, int n1, int n2, char* s);
	int append_usage(Tcl_Interp *ip, const char *str);
	int append_usage_cmdi(Tcl_Interp *ip, int i);

	int cmd_trans(ClientData, transGoal, Tcl_Interp*, int, char* av[]);

	void Tcl_AppendLoid( Tcl_Interp *ip, int res, const lrid_t &loid);
	void Tcl_AppendLVid( Tcl_Interp *ip, int res, const lvid_t &);
	void Tcl_AppendSerial( Tcl_Interp *ip, int res, const serial_t &);
	int _atoi(const char *c);
	RequestMode	getRequestMode(Tcl_Interp* ip, char *p) ;
	LockMode	getLockMode(Tcl_Interp* ip, char *p) ;
	int get_vid(
		// Tcl_Interp* ip, 
		void *v, const char *av, lvid_t &target);

	int getCompareOp(ClientData, Tcl_Interp* ip, char *av, CompareOp &target);
	int _get_mode(ClientData,
		Tcl_Interp* ip, int ac, char* av[], int imode, mode_t *mode);
}

enum	statistics_modes { 
	s_autoprint	=0x1, 
	s_autoclear	=0x2,
	s_remote	=0x4, 
};


#define SYNTAXERROR(ip,msg,info)\
	return cmdreply(clientdata, ip, SyntaxError(ip,msg,info),_fname_debug_)

#define CMDREPLY(x) return cmdreply(clientdata, ip,x,_fname_debug_)

// NB: args to CHECK are not #args but values for argc
#define CHECK(n1,n2,s) if(check(clientdata,ip,ac,n1,n2,s)==TCL_ERROR)\
	CMDREPLY(tcl_error);

#define IS_NULL_TID(t) (t==0)

#define GET_CMPOP(av, target) \
	if(getCompareOp(clientdata, ip,av,target)!=TCL_OK) CMDREPLY(tcl_error);

#	define GETMODE(a,b,ac) _get_mode(clientdata,ip,ac,av,b,&mode)

/* stuff shared between interp and shell */
extern int	LineNumber;
extern int	verbose;
extern int	expecterrors;
extern int	dirent_overhead;
extern int 	statistics_mode;

#ifdef USE_VERIFY
extern verify_t *v;
#endif

#define 	CMD_OF_VAS 1
#if defined(__GNUG__) && (__GNUC_MINOR__<6)
/* man 3 getopt */
#ifdef SUNOS41
EXTERNC	int getopt(int,char**,char*);
#else
EXTERNC	int getopt(int,char*const argv[],const char*);
#endif
#endif /* GNUC */

extern char *optarg;
extern int optind, opterr;


class dummy_tclout {
private:
    const scratch_buf_size = 10000;
    char *scratch_buf; // big enough for df_print & stats
    ostrstream *__tclout;
public:
    ostrstream &_tclout() { return *__tclout; }
    dummy_tclout() { 
       scratch_buf = new char[scratch_buf_size];
       __tclout = new ostrstream(scratch_buf,  scratch_buf_size);
    }
    ~dummy_tclout() { 
       delete __tclout;
       __tclout = 0;
       delete[] scratch_buf;
       scratch_buf = 0;
    }
}; // global in shell.C
extern class dummy_tclout _dummy;// in shell.C
#define tclout _dummy._tclout()

#endif /*__SHELL_MISC_H__*/
