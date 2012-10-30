#ifdef RPC_HDR
%#define __malloc_h

%#include <externc.h>
%#include <bzero.h>

%	BEGIN_EXTERNCLIST
%#	include <rpc/rpc.h>
%	END_EXTERNCLIST
#endif

#ifdef COMMENT
/* in  common.x 
// 		put things that will be generic typedef for all code.
// in  reply.x 
// 		put rpc reply types
// in  vmsg.x 
// 		put rpcs for vas-vas protocol
// in  cmsg.x 
// 		put rpcs for client-vas protocol
*/
#endif

#include "common.x"
#include "reply.x"
#include "cmsg.x"
#include "vmsg.x"


