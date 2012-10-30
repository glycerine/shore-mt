/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __FUNCPROTO_H__
#define __FUNCPROTO_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/funcproto.h,v 1.24 1997/01/24 16:47:56 nhall Exp $
 */
#include <copyright.h>

#include <msg_stats.h>

#ifdef DEBUG
#define __CHECKS__ vas->checkflags(false);\
	if(vas->status.vasresult!= SVAS_OK){assert(vas->status.vasreason != 0);}\
	else { assert(vas->status.vasreason ==0); }
#else
#define __CHECKS__
#endif

class svas_base;//forward
EXTERNC svas_server *ARGN2vas(void *cl); /* returns ptr to vas structure */

// This is for use in cmsg.C and vmsg.C, which
// don't have a vas structure, being the rpc code.

#	define FSTART(typ,fn)\
	char			*_fname_debug_ = #fn;\
	int				smerror = 0;\
	svas_server		*vas = ARGN2vas(clnt);\
	typ##_reply		*reply = &(vas->get_client_replybuf()->_##typ##_reply);\
	MY_MSG_STATS.inc((int)fn);\
	DUMP(fn##_1);\
	__log__->log(log_info, #fn);\
	__CHECKS__;\
	errno 		= 0;


#define	 REPLY\
	reply->status = vas->status;\
	DBG(<<"tx status=" << (int)(reply->status.txstate) << " " << (int)(vas->status.txstate));\
	DBG(<<"status=" << (reply->status.vasresult) <<", " << (reply->status.vasreason) ); \
	if(reply->status.txstate == Aborting) { reply->status.vasresult=SVAS_ABORTED;} \
	__CHECKS__;\
	return reply

#endif

