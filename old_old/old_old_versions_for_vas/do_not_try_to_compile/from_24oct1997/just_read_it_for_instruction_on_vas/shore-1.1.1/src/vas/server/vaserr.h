/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __VASERR_H__
#define __VASERR_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/vaserr.h,v 1.33 1997/01/24 16:48:24 nhall Exp $
 */
#include <copyright.h>
#include "svas_service.h"
#include "rpc_service.h"
#include "svas_layer.h"

extern int errno; // from unix

// conventions:
// _fname_debug_ is string name of function.

#ifndef RPC_HDR


#define SHORELOG(which)  /* ErrLog		*shorelog = which->errlog; */
#define LOG_FPROLOGUE /* shorelog->log(log_info, #fn); */

#ifndef FPROLOGUE
#define FPROLOGUE(fn,which)\
	FUNC(fn) \
	SHORELOG(which); \
	errno 		= 0;\
	rc_t		smerrorrc; \
	VASResult	res_ignore_warning=SVAS_FAILURE;
#define res res_ignore_warning
#endif


#define FSTART  {
#define FFAILURE /*label*/failure
#define FOK } /*label*/bye_ok_ignore_warning

#define _ERR(verr,v,res,ekind)\
	v->set_error_info(verr,res,ekind, smerrorrc,\
		_fname_debug_, __LINE__, __FILE__)

#define ABORT(verr)\
	set_error_info(verr,SVAS_ABORTED,svas_base::ET_VAS, smerrorrc,\
		_fname_debug_, __LINE__, __FILE__)

#define WARN(verr)\
	set_error_info(verr,SVAS_WARNING,svas_base::ET_VAS, smerrorrc,\
		_fname_debug_, __LINE__, __FILE__)

#define VERR(verr)\
	set_error_info(verr,SVAS_FAILURE,svas_base::ET_USER, smerrorrc,\
		_fname_debug_, __LINE__, __FILE__)

#define remotelog ShoreVasLayer.remote_service->log

#define VFPROLOGUE(fn)\
	FPROLOGUE(fn, this);

#define LOGVFPROLOGUE(fn)\
	FPROLOGUE(fn, this); LOG_FPROLOGUE

#define VABORT {\
	DBG("VABORT AT  " << __LINE__ <<" "<< __FILE__); \
	/* err info set in abort2savepoint() */\
	if(abort2savepoint(savept)!=SVAS_OK) {\
		VERR(SVAS_SmFailure);\
	}\
}

// Object funcs are defined in the vas class but
// when created, are given a session ptr "cl"

#define OBJERR(verr,kind)\
	this->owner->set_error_info(verr,SVAS_FAILURE, svas_base::##kind,\
		smerrorrc, _fname_debug_, __LINE__, __FILE__)

#define OFPROLOGUE(fn)\
	FPROLOGUE(fn,this->owner)

#define OSAVE\
	sm_save_point_t	savept; \
	if SMCALL(save_work(savept)){ \
		OBJERR(SVAS_SmFailure, ET_VAS);\
		FAIL;\
	} DBG(<<"savept at " << (void *)&savept << " " << savept);\
	
#define OABORT\
	DBG("OABORT in  " << _fname_debug_ <<" from " << owner->failure_line); \
	if(owner->abort2savepoint(savept)!=SVAS_OK) {\
		OBJERR(SVAS_SmFailure,ET_USER);\
	}

#ifdef DEBUG
#define ENTER_DIR_CONTEXT_IF_NECESSARY \
	operation old_context = assure_context(__FILE__,__LINE__, directory_op);

#define RESTORE_CLIENT_CONTEXT_IF_NECESSARY\
	if(old_context != _context) { \
		(void) assure_context(__FILE__,__LINE__,old_context); \
	} 

#define ENTER_DIR_CONTEXT \
	_DO_(change_context(__FILE__,__LINE__,directory_op));

#define RESTORE_CLIENT_CONTEXT \
	_DO_(change_context(__FILE__,__LINE__,client_op)); 

#else
#define ENTER_DIR_CONTEXT_IF_NECESSARY \
	enum operation old_context = assure_context(directory_op);

#define RESTORE_CLIENT_CONTEXT_IF_NECESSARY\
	if(old_context != _context) { \
		(void) assure_context(old_context); \
	} 
#define ENTER_DIR_CONTEXT \
	_DO_(change_context(directory_op));

#define RESTORE_CLIENT_CONTEXT \
	_DO_(change_context(client_op)); 
#endif

#define TX_ACTIVE\
	dassert((xct() != (xct_t *)0) && (_xact == xct()));
#define CLI_TX_ACTIVE\
	dassert((xct() != (xct_t *)0) && _xact == xct());

#define SET_CLI_SAVEPOINT \
	sm_save_point_t	savept; \
	{ CLI_TX_ACTIVE; \
	if SMCALL(save_work(savept)){ VERR(SVAS_InternalError); assert(0); }\
	DBG(<<"savept at " << (void *)&savept << " " << savept);\
	}


#define LEAVE leave(_fname_debug_); __txreq__=true;

#define TX_REQUIRED  \
	bool __txreq__; \
	if(enter(true)!=SVAS_OK){\
		LEAVE; RETURN SVAS_FAILURE;\
	}else{ SET_CLI_SAVEPOINT; } 


#define TX_NOT_ALLOWED  \
	bool __txreq__; \
	if(enter(false)!=SVAS_OK){ LEAVE; RETURN SVAS_FAILURE; } 

#endif
#endif
