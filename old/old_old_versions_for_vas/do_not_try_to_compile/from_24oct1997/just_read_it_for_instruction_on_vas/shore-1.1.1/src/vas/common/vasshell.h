/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __VASSHELL_H__
#define __VASSHELL_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/vasshell.h,v 1.5 1995/07/14 22:38:37 nhall Exp $
 */
#include <copyright.h>
#include <shell.misc.h>

#define Vas ((svas_base *)(((interpreter_t *)clientdata)->_vas))

extern "C" {

	/* part of the shell */
	void AppendVasError(Tcl_Interp *ip, VASResult res, VASResult reason) ;
	CMDResult vasreply(ClientData, Tcl_Interp *ip, VASResult res, VASResult reason);
	int print_statistics(ClientData,Tcl_Interp* ip, int mode);

	int str2oid(ClientData,
		Tcl_Interp* ip, 
		svas_base *v, const char *av, lrid_t &target,
		bool, bool, bool,
		bool *);

	int str2iid(ClientData,
		Tcl_Interp* ip, 
		svas_base *v, 
		const char *av, 
		IndexId &target);

	// for gdb:
	void expect(
		const char *command,
		const char *result,
		const char *wanted
	); 
}
#define PATH_OR_OID_2_OID_NOFOLLOW(ip, av, target, isReg, errifnotfound) \
	(str2oid(clientdata, ip, Vas, av,target,true,false, errifnotfound, isReg)==TCL_OK) 

#define PATH_OR_OID_2_OID(ip, av, target, isReg, errifnotfound) \
	(str2oid(clientdata, ip, Vas, av,target,true,true, errifnotfound, isReg)==TCL_OK) 


#define OID_2_OID(ip, av, target) \
	(str2oid(clientdata, ip, Vas, av,target,false,false, true, 0)==TCL_OK)

#define IS_OID(ip, av, target) \
	(str2oid(clientdata, ip, Vas, av,target,false,false, false, 0)==TCL_OK) 

#define INDEXID(ip, av, target) \
	(str2iid(clientdata, ip, Vas, av,target)==TCL_OK) 


#define GET_VID(ip, av, target) \
	if(get_vid( /*ip, */ Vas, av,target)!=TCL_OK) SYNTAXERROR(ip,"in get_vid","");


#	define CHECKCONNECTED \
		if(Vas) {\
			if(!Vas->connected()) {\
				Tcl_AppendResult(ip, "Server gone... cleaning up.", 0);\
				delete Vas;\
				((interpreter_t *)clientdata)->_vas = 0; \
				CMDREPLY(tcl_error);\
			}\
		} else {\
			Tcl_AppendResult(ip, "Not connected to vas", 0);\
			CMDREPLY(tcl_error);\
		} yield();
	
#define CMDFUNC(x)\
	FUNC(x);\
	VASResult res=SVAS_OK; int reason=0;


#define NESTEDVASERR(x,r)\
	return cmdreply(clientdata, ip, \
	vasreply(clientdata, ip, x,r),_fname_debug_)

#define VASERR(x)\
	NESTEDVASERR(x,Vas->status.vasreason);

// NB: don't let the caller put a semicolon after this!

#define CALL(x) res = Vas->x; reason = Vas->status.vasreason;  \

#define BEGIN_NESTED_TRANSACTION \
{\
	ShoreStatus saved_status;\
	bool __nested=false; tid_t __t;\
	res = Vas->trans(&__t); DBG(<<"tid="<<__t);\
	if(res == SVAS_FAILURE) {\
		__nested = true; \
		if((res = Vas->beginTrans(2,&__t))!=SVAS_OK) { VASERR(res); }\
		DBG(<<"trans now="<<__t);\
	}

// NB: don't let the caller put a semicolon after this!

#define END_NESTED_TRANSACTION \
	saved_status = Vas->status; \
	if(__nested) { DBG(<<"committing trans="<<__t);\
		res = Vas->commitTrans(__t);\
	}\
	(void)  Vas->trans(&__t);\
	DBG(<<"tid="<<__t);\
	reason = saved_status.vasreason; \
	res = saved_status.vasresult;\
}

#define GET_SIZE(av, c,h,tst,n) \
	DBG( << *c << ":" << *h << ":" << *tst)\
	if(get_objsize( /*ip, Vas, */ av,c,h,tst,n)!=TCL_OK){\
		SYNTAXERROR(ip,"in get_objsize","");\
	} 

#ifdef USE_VERIFY
#define CHECK_TRANS_STATE {\
	tid_t __t;\
	res = Vas->trans(&__t); DBG(<<"tid="<<__t);\
	if(res == SVAS_FAILURE) v->abort();\
}
#else
#define CHECK_TRANS_STATE 
#endif

enum lsflags {
	ls_none = 0, 
	ls_notimpl=0x8000000, // not implemented
	ls_T=ls_notimpl, 	//verbose time format
	ls_r=ls_notimpl,	// reverse order of sort
	ls_f=ls_notimpl,	// force to interp as a dir
	ls_1=ls_notimpl,	// 1-column output
	ls_C=ls_notimpl,	// multi-column output
	ls_q=ls_notimpl,	// print non-graphic chars as '?'

	ls_l=0x1, 	// long format
	ls_g=0x2, 	// print gid
	ls_F=0x4, 	// fancy print of // = @ *, etc
	ls_n=0x8,	// use oid not name
	ls_a=0x20,	// all entries
	ls_s=0x40,	// size in KB
	ls_d=0x80,	// if directory don't do contents
	ls_L=ls_notimpl,	// follow symbolic link or xref
	ls_u=0x200,	// use atime instead of mtime
	ls_c=0x400,	// use ctime instead of mtime
	ls_R=0x800,	// recursive
	ls_z=0x100,	// recursive 1-level (local only)(can't turn on w/ argument)
	ls_i=0x1000, 
	ls_ii=0x2000, 
	ls_iii=0x4000, 
};
typedef enum lsflags lsflags;

#ifndef notdef
#define USER_DEF_TYPE ReservedOid::_UserDefined
#else
	lrid_t		garbage = ReservedOid::_UserDefined;

#define USER_DEF_TYPE garbage
#endif

extern int	dirent_overhead;

#endif /*__SHELL_MISC_H__*/
