/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

// macros for error handling
#include <assert.h>
#include <errno.h>
#include "bzero.h"
#include "debug.h"

#ifdef DEBUG
#define AUDITIN if(ShoreVasLayer.do_audit) audit("enter");
#define AUDITOUT if(ShoreVasLayer.do_audit) audit("leave");
#else
#define AUDITIN 
#define AUDITOUT 
#endif

#define CLEARSTATUS\
	/*bzero(&status, sizeof(ShoreStatus));*/ \
	status.vasresult=0; status.vasreason=0;\
	status.smresult =0; status.smreason=0;\
	status.osreason=0;\
	AUDITIN

#ifdef VRETURN
#undef VRETURN
#endif
#ifdef DEBUG
#   define VRETURN \
	if(_debug.flag_on(_fname_debug_,__FILE__)) {\
		_debug.clog << __LINE__ << " " << __FILE__ << ": " \
		<< "return from " << _fname_debug_ << flushl; }\
	AUDITOUT \
	return
#else
#   define VRETURN return
#endif


#ifdef notdef
extern int ::errno; // from unix
#endif

// conventions:
// each function has these local variables:
// _fname_debug_ = string name of function
// a	 = argument type for next level call
// reply = reply type ptr - for return from call
// status is part of "this"

#ifdef PURIFY
#define CLEARA memset(&a,'\0',sizeof(a));
#else
#define CLEARA 
#endif

// FSTART:
// fn is string name of function.
// atyp is the argument type
// rtyp is the reply type
//	char		*_fname_debug_ = _string(fn) ;\
//
#define ADJUST_STAT(atyp,x) \
	msg_stats.inc((int)atyp,x);

#	define FSTART(fn,atyp,rtyp)\
	FUNC(fn) \
	atyp##_arg	a; CLEARA\
	rtyp##_reply *reply=NULL;\
	xdrproc_t _freeproc = (xdrproc_t) xdr_##rtyp##_reply;\
	msg_stats.inc((int)atyp);\
	CLEARSTATUS\
	::errno 		= 0;

#define VERR(e)\
		status.vasresult = SVAS_FAILURE;\
		status.osreason = ::errno;\
		status.vasreason = e;\
		perr(_fname_debug_, __LINE__, __FILE__,ET_USER);

#ifdef DEBUG
#define __CHECK__ if(reply->status.vasresult!=SVAS_OK) {\
	dassert(reply->status.vasreason != 0); }
#else
#define __CHECK__
#endif

#define SVCALL(x,rtyp)\
	++_stats.server_calls;\
	status.vasresult = SVAS_OK;\
	rmsg_stats.inc((int)x);\
	reply =\
	(rtyp##_reply *)\
		x##_1( &a, (CLIENT *)(this->cl)); \
	if( reply==NULL ) { \
		DBG(<<"null reply");\
		VERR(SVAS_RpcFailure);\
		return (status.vasresult = SVAS_FAILURE);\
	} else { \
		__CHECK__;\
	}

#define MALLOC_CHECK(xxx) if(((xxx)==0) && \
		(status.vasresult = SVAS_FAILURE) && \
		(status.vasreason = SVAS_MallocFailure))goto failure;


/* NB: blank and ## are critical in the following
// use of SVCALL
*/
#define PREVENTER \
	if(_batch->is_active()) { \
		VERR(SVAS_BatchingActive); \
		return (status.vasresult = SVAS_FAILURE);\
	}

#ifdef PURIFY
#define MEMSETP if(purify_is_running()) { memset(&b, '\0', sizeof(b)); }
#else
#define MEMSETP 
#endif

/* must malloc here because RPC frees these */
#define BATCH1(yyy,xxx,rtyp)\
	batch_req b; MEMSETP\
	if(_batch->preflush(&b)) { assert(0); } 

#define BATCH2(yyy,xxx,rtyp)\
	if(_batch->is_active()) { \
		b.batch_req_u._##xxx = a;\
		DBG(<<"set tag to "<<yyy);\
		b.tag = yyy;\
		DBG(<<"batching " << #xxx);\
		_batch->queue(b);\
		status.vasresult = SVAS_OK;\
	} else {\
		SVCALL( ##xxx,rtyp);\
	}
	
#define BATCH3(yyy,xxx,rtyp)\
	if(!_batch->is_active()) { \
		_batch->svcalled(); \
	}

#define REPLY1\
	status = reply->status;\
	if(status.vasresult != SVAS_OK) \
		perr(_fname_debug_, __LINE__, __FILE__,ET_USER);\
	if(!CLNT_FREERES((CLIENT *)this->cl, _freeproc, reply)){\
		assert(0); /* internal error */\
	}\
	update_txstate(status.txstate);

#define REPLY\
	REPLY1\
	VRETURN status.vasresult

#define BREPLY\
	if(!_batch->is_active()) { REPLY1; }\
	VRETURN status.vasresult

#define WARNREPLY(warn,reason)\
	if(!_batch->is_active()) { REPLY1; }\
	if((warn)) {status.vasresult = SVAS_WARNING; status.vasreason=(reason);}\
	VRETURN status.vasresult

/* assumes source is null-terminated string */
// new version: strings are declared char * in .x file
#define STRARG(x,typ,source)\
	x = source;

