/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm_global_deadlock.cc,v 1.18 1997/06/15 03:14:18 solomon Exp $
 */

#define SM_SOURCE
#define SM_GLOBAL_DEADLOCK_C


#define DBGTHRD(arg) DBG(<<" th."<<me()->id << " " arg)

#ifdef __GNUG__
#pragma implementation "global_deadlock.h"
#pragma implementation "sm_global_deadlock.h"
#endif

#include <st_error.h>
#include <sm_int_1.h>
#include <sm_global_deadlock.h>
#include <lock_s.h>


template class w_list_t<BlockedElem>;
template class w_list_i<BlockedElem>;
template class w_list_t<WaitForElem>;
template class w_list_i<WaitForElem>;
template class w_list_t<WaitPtrForPtrElem>;
template class w_list_t<GtidIndexElem>;
template class w_list_i<GtidIndexElem>;
template class w_hash_t<GtidTableElem, const gtid_t>;
template class w_hash_i<GtidTableElem, const gtid_t>;
template class w_list_t<GtidTableElem>;
template class w_list_i<GtidTableElem>;

W_FASTNEW_STATIC_DECL(BlockedElem, 64);
W_FASTNEW_STATIC_DECL(WaitForElem , 64);
W_FASTNEW_STATIC_DECL(WaitPtrForPtrElem , 64);
W_FASTNEW_STATIC_DECL(GtidIndexElem , 64);
W_FASTNEW_STATIC_DECL(BitMapVector, 64);
W_FASTNEW_STATIC_DECL(GtidTableElem , 64);


/***************************************************************************
 *                                                                         *
 * BitMapVector class                                                      *
 *                                                                         *
 ***************************************************************************/

BitMapVector::BitMapVector(uint4 numberOfBits)
:   size(0),
    vector(0)
{
    size = (numberOfBits + BitsPerWord - 1) / BitsPerWord;
    vector = new uint4[size];
    ClearAll();

    w_assert3(numberOfBits <= size * BitsPerWord);
    w_assert3(numberOfBits > (size - 1) * BitsPerWord);
}


BitMapVector::BitMapVector(const BitMapVector& v)
{
    size = v.size;
    vector = new uint4[size];
    for (uint4 i = 0; i < size; i++)
	vector[i] = v.vector[i];
}


void BitMapVector::Resize(uint4 numberOfWords)
{
    w_assert3(numberOfWords > size);

    uint4*	newVector = new uint4[numberOfWords];

    uint4 i=0;
    for (i = 0; i < size; i++)
	newVector[i] = vector[i];
    
    for (i = size; i < numberOfWords; i++)
	newVector[i] = 0;
    
    delete [] vector;
    vector = newVector;
    size = numberOfWords;
}


BitMapVector& BitMapVector::operator=(const BitMapVector& v)
{
    if (this != &v)  {
	if (v.size > size)
	    Resize(v.size);

	uint4 i=0;
	for (i = 0; i < v.size; i++)
	    vector[i] = v.vector[i];
	
	for (i = v.size; i < size; i++)
	    vector[i] = 0;
    }
    return *this;
}


bool BitMapVector::operator==(const BitMapVector& v)
{
	for (uint4 i = 0; i < size; i++)
		if (vector[i] != v.vector[i])
			return false;
	
	return true;
}


BitMapVector::~BitMapVector()
{
    delete [] vector;
}


void BitMapVector::ClearAll()
{
    for (uint4 i = 0; i < size; i++)
	vector[i] = 0;
}


int4 BitMapVector::FirstSetOnOrAfter(uint4 index) const
{
    uint4	wordIndex = index / BitsPerWord;
    uint4	mask = (1 << (index % BitsPerWord));

    while (wordIndex < size)  {
	if (vector[wordIndex] & mask)
	    return index;
	
	index++;
	mask <<= 1;
	if (mask == 0)  {
	    wordIndex++;
	    mask = 1;
	}
    }

    return -1;
}


int4 BitMapVector::FirstClearOnOrAfter(uint4 index) const
{
    uint4	wordIndex = index / BitsPerWord;
    uint4	mask = (1 << (index % BitsPerWord));

    while (wordIndex < size)  {
	if (!(vector[wordIndex] & mask))
	    return index;
	
	index++;
	mask <<= 1;
	if (mask == 0)  {
	    wordIndex++;
	    mask = 1;
	}
    }

    return index;
}


void BitMapVector::SetBit(uint4 index)
{
    if (index / BitsPerWord >= size)  {
	Resize((index + BitsPerWord) / BitsPerWord);
	w_assert3(index / BitsPerWord < size);
    }

    vector[index / BitsPerWord] |= (1 << (index % BitsPerWord));
}


void BitMapVector::ClearBit(uint4 index)
{
    if (index / BitsPerWord < size)  {
	vector[index / BitsPerWord] &= ~(1 << (index % BitsPerWord));
    }
}


bool BitMapVector::IsBitSet(uint4 index) const
{
    if (index / BitsPerWord >= size)
	return false;
    else
	return vector[index / BitsPerWord] & (1 << (index % BitsPerWord));
}


void BitMapVector::OrInBitVector(const BitMapVector& v, bool& changed)
{
    if (size < v.size)
	Resize(v.size);
    
    for (uint4 i = 0; i < v.size; i++)  {
	changed |= (~vector[i] & v.vector[i]);
	vector[i] |= v.vector[i];
    }
}


/***************************************************************************
 *                                                                         *
 * DeadlockMsg class output operator                                       *
 *                                                                         *
 ***************************************************************************/

const char* DeadlockMsg::msgNameStrings[] = {
	"msgVictimizerEndpoint",
	"msgRequestClientId",
	"msgAssignClientId",
	"msgClientDied",
	"msgRequestDeadlockCheck",
	"msgRequestWaitFors",
	"msgWaitForList",
	"msgSelectVictim",
	"msgVictimSelected",
	"msgKillGtid",
	"msgQuit",
	"msgClientEndpointDied",
	"msgVictimizerEndpointDied",
	"msgServerEndpointDied"
};


ostream& operator<<(ostream& o, const DeadlockMsg& msg)
{
    o <<     "  msgType:  " << DeadlockMsg::msgNameStrings[msg.header.msgType] << endl
      << "      clientId: " << msg.header.clientId << endl
      << "      complete: " << (bool)msg.header.complete << endl
      << "      count:    " << msg.header.count << endl
      << "      gtids:    [";

    for (uint4 i = 0; i < msg.header.count; i++)  {
	o << ' ' << msg.gtids[i];
    }

    o << " ]" << endl;

    return o;
}


/***************************************************************************
 *                                                                         *
 * notify_always                                                           *
 *                                                                         *
 ***************************************************************************/

inline rc_t notify_always(Endpoint& endpoint, Endpoint& target, Buffer& msgBuffer)
{
    rc_t rc = endpoint.notify(target, msgBuffer);
    if (rc && rc == RC(scDEAD))  {
	EndpointBox box;
	W_DO( box.set(0, endpoint) );
	W_DO( target.send(msgBuffer, box) );
    }  else if (rc)  {
	W_DO(rc);
    }

    return RCOK;
}


/***************************************************************************
 *                                                                         *
 * IgnoreScDEAD                                                            *
 *                                                                         *
 ***************************************************************************/

inline rc_t IgnoreScDEAD(const rc_t& rc)
{
    if (rc && rc == RC(scDEAD))  {
	return RCOK;
    }  else  {
	return rc;
    }
}


/***************************************************************************
 *                                                                         *
 * DeadlockClientCommunicator class                                        *
 *                                                                         *
 ***************************************************************************/

DeadlockClientCommunicator::DeadlockClientCommunicator(
	const server_handle_t& theServerHandle,
	name_ep_map* ep_map,
	CommSystem& commSystem)
:   deadlockClient(0),
    serverHandle(theServerHandle),
    endpointMap(ep_map),
    myId(0xFFFFFFFF),
    sendBuffer(sizeof(DeadlockMsg)),
    rcvBuffer(sizeof(DeadlockMsg)),
    serverEndpointDiedBuffer(sizeof(DeadlockMsgHeader)),
    sendMsg(0),
    rcvMsg(0),
    comm(commSystem),
    done(false),
    serverEndpointValid(false)
{
    DeadlockMsgHeader* serverDiedMsg = (DeadlockMsgHeader*)serverEndpointDiedBuffer.start();
    serverDiedMsg->msgType = msgServerEndpointDied;
    serverDiedMsg->clientId = 0;
    serverDiedMsg->complete = true;
    serverDiedMsg->count = 0;
    serverDiedMsg->hton();

    sendMsg = (DeadlockMsg*)sendBuffer.start();
    memset(sendMsg, 0, sizeof(DeadlockMsg));
    rcvMsg = (DeadlockMsg*)rcvBuffer.start();
    memset(rcvMsg, 0, sizeof(DeadlockMsg));

    W_COERCE( comm.make_endpoint(myEndpoint) );
    W_COERCE( endpointMap->name2endpoint(serverHandle, serverEndpoint) );
    serverEndpointValid = true;

    W_COERCE( notify_always(serverEndpoint, myEndpoint, serverEndpointDiedBuffer) );
}


DeadlockClientCommunicator::~DeadlockClientCommunicator()
{
    if (serverEndpointValid)  {
	W_COERCE( SendClientEndpointDied() );
	W_COERCE( IgnoreScDEAD( serverEndpoint.stop_notify(myEndpoint) ) );
	W_COERCE( serverEndpoint.release() );
    }
    
    W_COERCE( myEndpoint.release() );
}


rc_t DeadlockClientCommunicator::SendRequestClientId()
{
    sendMsg->header.msgType = msgRequestClientId;
    W_DO( SendMsg() );
    
    return RCOK;
}


rc_t DeadlockClientCommunicator::ReceivedAssignClientId(uint4 clientId)
{
    myId = clientId;
    W_DO( deadlockClient->AssignClientId() );
    
    return RCOK;
}


rc_t DeadlockClientCommunicator::SendRequestDeadlockCheck()
{
    sendMsg->header.msgType = msgRequestDeadlockCheck;
    sendMsg->header.count = 0;
    W_DO( SendMsg() );
    
    return RCOK;
}


rc_t DeadlockClientCommunicator::SendWaitForList(WaitForList& waitForList)
{
    sendMsg->header.msgType = msgWaitForList;
    
    sendMsg->header.count = 0;
    if (waitForList.is_empty())  {
        sendMsg->header.complete = true;
        W_DO( SendMsg() );
    }  else  {
    	while (WaitForElem* waitForElem = waitForList.pop())  {
    	    sendMsg->gtids[sendMsg->header.count++] = waitForElem->waitGtid;
    	    sendMsg->gtids[sendMsg->header.count++] = waitForElem->forGtid;
    	    delete waitForElem;
    	    if (sendMsg->header.count >= DeadlockMsg::MaxNumGtidsPerMsg - 1 || waitForList.is_empty())  {
    	        sendMsg->header.complete = waitForList.is_empty();
    	        W_DO( SendMsg() );
    	        sendMsg->header.count = 0;
    	    }
    	}
    }
    
    return RCOK;
}


rc_t DeadlockClientCommunicator::ReceivedRequestWaitForList()
{
    W_DO( deadlockClient->SendWaitForGraph() );
    return RCOK;
}


rc_t DeadlockClientCommunicator::ReceivedKillGtid(const gtid_t& gtid)
{
    W_DO( deadlockClient->UnblockGlobalXct(gtid) );
    return RCOK;
}


rc_t DeadlockClientCommunicator::SendQuit()
{
    Buffer		buffer(sizeof(DeadlockMsg));
    DeadlockMsg*	msg = (DeadlockMsg*)buffer.start();

    msg->header.msgType = msgQuit;
    msg->header.complete = true;
    msg->header.count = 0;
    msg->header.clientId = sendMsg->header.clientId;

    DBGTHRD( << "Deadlock client sending message:" << endl << " C->" << *msg );
    msg->hton();

    EndpointBox		emptyBox;
    W_DO( myEndpoint.send(buffer, emptyBox) );
    return RCOK;
}


rc_t DeadlockClientCommunicator::SendClientEndpointDied()
{
    sendMsg->header.msgType = msgClientEndpointDied;
    sendMsg->header.count = 0;
    W_DO( SendMsg() );

    return RCOK;
}


rc_t DeadlockClientCommunicator::ReceivedServerEndpointDied(Endpoint& theServerEndpoint)
{
    if (serverEndpointValid)  {
	w_assert3(theServerEndpoint == serverEndpoint);

	W_DO( IgnoreScDEAD( serverEndpoint.stop_notify(myEndpoint) ) );
	
	W_DO( serverEndpoint.release() );
	serverEndpointValid = false;
    }
    W_DO( theServerEndpoint.release() );

    return RCOK;
}


rc_t DeadlockClientCommunicator::SendMsg()
{
    if (serverEndpointValid)  {
	sendMsg->header.clientId = myId;
	DBGTHRD( << "Deadlock client sending message:" << endl << " C->" << *sendMsg );
	sendMsg->hton();
	EndpointBox		box;
	if (sendMsg->header.msgType == msgRequestClientId || sendMsg->header.msgType == msgClientEndpointDied)  {
	    W_DO( box.set(0, myEndpoint) );
	}
	W_DO( IgnoreScDEAD( serverEndpoint.send(sendBuffer, box) ) );
    }
    return RCOK;
}


void DeadlockClientCommunicator::SetDeadlockClient(CentralizedGlobalDeadlockClient* client)
{
    delete deadlockClient;
    deadlockClient = client;
}


rc_t DeadlockClientCommunicator::RcvAndDispatchMsg()
{
    EndpointBox		box;
    W_DO( myEndpoint.receive(rcvBuffer, box) );
    rcvMsg->ntoh();
    DBGTHRD( << "Deadlock client received message:" << endl << " ->C" << *rcvMsg );
    switch ((DeadlockMsgType)rcvMsg->header.msgType)  {
	case msgAssignClientId:
	    W_DO( ReceivedAssignClientId(rcvMsg->header.clientId) );
	    break;
	case msgRequestWaitFors:
	    W_DO( ReceivedRequestWaitForList() );
	    break;
	case msgKillGtid:
	    W_DO( ReceivedKillGtid(rcvMsg->gtids[0]) );
	    break;
	case msgQuit:
	    done = true;
	    break;
	case msgServerEndpointDied:
	    {
		Endpoint theServerEndpoint;
		W_DO( box.get(0, theServerEndpoint) );
		W_DO( ReceivedServerEndpointDied(theServerEndpoint) );
	    }
	    break;
	default:
	    w_assert1(0);
    }
    return RCOK;
}


/***************************************************************************
 *                                                                         *
 * DeadlockServerCommunicator class                                        *
 *                                                                         *
 ***************************************************************************/

PickFirstDeadlockVictimizerCallback selectFirstVictimizer;


DeadlockServerCommunicator::DeadlockServerCommunicator(
	const server_handle_t&		theServerHandle,
	name_ep_map*			ep_map,
	CommSystem&			theCommSystem,
	DeadlockVictimizerCallback&	callback
)
:   deadlockServer(0),
    serverHandle(theServerHandle),
    endpointMap(ep_map),
    commSystem(theCommSystem),
    clientEndpointsSize(0),
    clientEndpoints(0),
    sendBuffer(sizeof(DeadlockMsg)),
    rcvBuffer(sizeof(DeadlockMsg)),
    clientEndpointDiedBuffer(sizeof(DeadlockMsgHeader)),
    victimizerEndpointDiedBuffer(sizeof(DeadlockMsgHeader)),
    sendMsg(0),
    rcvMsg(0),
    selectVictim(callback),
    remoteVictimizer(false),
    done(false)
{
    sendMsg = (DeadlockMsg*)sendBuffer.start();
    memset(sendMsg, 0, sizeof(DeadlockMsg));
    rcvMsg = (DeadlockMsg*)rcvBuffer.start();
    memset(rcvMsg, 0, sizeof(DeadlockMsg));

    DeadlockMsgHeader* clientDiedMsg = (DeadlockMsgHeader*)clientEndpointDiedBuffer.start();
    clientDiedMsg->msgType = msgClientEndpointDied;
    clientDiedMsg->clientId = 0;
    clientDiedMsg->complete = true;
    clientDiedMsg->count = 0;
    clientDiedMsg->hton();

    DeadlockMsgHeader* victimizerDiedMsg = (DeadlockMsgHeader*)victimizerEndpointDiedBuffer.start();
    victimizerDiedMsg->msgType = msgClientEndpointDied;
    victimizerDiedMsg->clientId = 0;
    victimizerDiedMsg->complete = true;
    victimizerDiedMsg->count = 0;
    victimizerDiedMsg->hton();

    clientEndpoints = new Endpoint[InitialNumberOfEndpoints];
    clientEndpointsSize = InitialNumberOfEndpoints;
    w_assert3(clientEndpointsSize);

    W_COERCE( endpointMap->name2endpoint(serverHandle, myEndpoint) );
}


DeadlockServerCommunicator::~DeadlockServerCommunicator()
{
    W_COERCE( BroadcastServerEndpointDied() );

    int4 i = 0;
    while ((i = activeClientIds.FirstSetOnOrAfter(i)) != -1)  {
    	W_COERCE( IgnoreScDEAD( clientEndpoints[i].stop_notify(myEndpoint) ) );
	W_COERCE( clientEndpoints[i].release() );
	i++;
    }
    delete [] clientEndpoints;
    
    if (remoteVictimizer)  {
    	W_COERCE( IgnoreScDEAD( victimizerEndpoint.stop_notify(myEndpoint) ) );
    	W_COERCE( victimizerEndpoint.release() );
	remoteVictimizer = false;
    }
    
    W_COERCE( myEndpoint.release() );
}


rc_t DeadlockServerCommunicator::ReceivedVictimizerEndpoint(Endpoint& ep)
{
    victimizerEndpoint = ep;
    W_DO( notify_always(ep, myEndpoint, victimizerEndpointDiedBuffer) );
    remoteVictimizer = true;
    return RCOK;
}


rc_t DeadlockServerCommunicator::ReceivedRequestClientId(Endpoint& ep)
{
    uint4 clientId = activeClientIds.FirstClearOnOrAfter(0);
    activeClientIds.SetBit(clientId);
    W_DO( deadlockServer->AddClient(clientId) );
    SetClientEndpoint(clientId, ep);
    W_DO( SendAssignClientId(clientId) );
    W_DO( notify_always(ep, myEndpoint, clientEndpointDiedBuffer) );
    return RCOK;
}


rc_t DeadlockServerCommunicator::SendAssignClientId(uint4 clientId)
{
    sendMsg->header.msgType = msgAssignClientId;
    sendMsg->header.clientId = clientId;
    W_DO( SendMsg() );
    
    return RCOK;
}


rc_t DeadlockServerCommunicator::ReceivedRequestDeadlockCheck()
{
    W_DO( deadlockServer->CheckDeadlockRequested() );
    return RCOK;
}


rc_t DeadlockServerCommunicator::BroadcastRequestWaitForList()
{
    sendMsg->header.msgType = msgRequestWaitFors;
    
    uint4 i = 0;
    int4 clientId;
    while ((clientId = activeClientIds.FirstSetOnOrAfter(i)) != -1)  {
	sendMsg->header.clientId = clientId;
        W_DO( SendMsg() );
        i++;
    }
    
    return RCOK;
}


rc_t DeadlockServerCommunicator::SendRequestWaitForList(uint4 clientId)
{
    sendMsg->header.msgType = msgRequestWaitFors;
    sendMsg->header.clientId = clientId;
    W_DO( SendMsg() );
    
    return RCOK;
}


rc_t DeadlockServerCommunicator::ReceivedWaitForList(uint4 clientId, uint2 count, const gtid_t* gtids, bool complete)
{
    WaitPtrForPtrList waitPtrForPtrList(WaitPtrForPtrElem::link_offset());
    uint4 i = 0;
    while (i < count)  {
        const gtid_t* waitPtr = &gtids[i++];
        const gtid_t* forPtr = &gtids[i++];
        waitPtrForPtrList.push(new WaitPtrForPtrElem(waitPtr, forPtr));
    }
    W_DO( deadlockServer->AddWaitFors(clientId, waitPtrForPtrList, complete) );
    w_assert3(waitPtrForPtrList.is_empty());
    
    return RCOK;
}


rc_t DeadlockServerCommunicator::SendSelectVictim(GtidIndexList& deadlockList)
{
    w_assert3(!deadlockList.is_empty());

    if (smlevel_0::deadlockEventCallback)  {
	GtidList gtidList(GtidElem::link_offset());
	GtidIndexListIter iter(deadlockList);
	while (GtidIndexElem* gtidIndexElem = iter.next())  {
	    gtidList.push(new GtidElem(deadlockServer->GtidIndexToGtid(gtidIndexElem->index)));
	}
	smlevel_0::deadlockEventCallback->GlobalDeadlockDetected(gtidList);
	while (GtidElem* elem = gtidList.pop())  {
	    delete elem;
	}
    }

    if (remoteVictimizer)  {
	sendMsg->header.msgType = msgSelectVictim;
	while (GtidIndexElem* gtidIndexElem = deadlockList.pop())  {
	    sendMsg->gtids[sendMsg->header.count++] = deadlockServer->GtidIndexToGtid(gtidIndexElem->index);
	    delete gtidIndexElem;
	    if (sendMsg->header.count >= DeadlockMsg::MaxNumGtidsPerMsg || deadlockList.is_empty())  {
		sendMsg->header.complete = deadlockList.is_empty();
		W_DO( SendMsg(toVictimizer) );
		sendMsg->header.count = 0;
	    }
	}
    }  else  {
	GtidList gtidList(GtidElem::link_offset());
	while (GtidIndexElem* gtidIndexElem = deadlockList.pop())  {
	    gtidList.push(new GtidElem(deadlockServer->GtidIndexToGtid(gtidIndexElem->index)));
	    delete gtidIndexElem;
	}
	gtid_t gtid = selectVictim(gtidList);
	if (smlevel_0::deadlockEventCallback)  {
	    smlevel_0::deadlockEventCallback->GlobalDeadlockVictimSelected(gtid);
	}
	W_DO( deadlockServer->VictimSelected(gtid) );
	while (GtidElem* gtidElem = gtidList.pop())  {
	    delete gtidElem;
	}
    }
    w_assert3(deadlockList.is_empty());
    
    return RCOK;
}


rc_t DeadlockServerCommunicator::ReceivedVictimSelected(const gtid_t gtid)
{
    if (smlevel_0::deadlockEventCallback)  {
	smlevel_0::deadlockEventCallback->GlobalDeadlockVictimSelected(gtid);
    }
    W_DO( deadlockServer->VictimSelected(gtid) );
    return RCOK;
}


rc_t DeadlockServerCommunicator::SendKillGtid(uint4 clientId, const gtid_t gtid)
{
    sendMsg->header.msgType = msgKillGtid;
    sendMsg->header.clientId = clientId;
    sendMsg->header.count = 1;
    sendMsg->gtids[0] = gtid;
    W_DO( SendMsg() );
    
    return RCOK;
}


rc_t DeadlockServerCommunicator::SendQuit()
{
    Buffer		buffer(sizeof(DeadlockMsg));
    DeadlockMsg*	msg = (DeadlockMsg*)buffer.start();

    msg->header.msgType = msgQuit;
    msg->header.complete = true;
    msg->header.count = 0;
    msg->header.clientId = 0;

    DBGTHRD( << "Deadlock server sending message:" << endl << " S->" << *msg );
    msg->hton();

    EndpointBox		emptyBox;
    W_DO( myEndpoint.send(buffer, emptyBox) );
    return RCOK;
}


rc_t DeadlockServerCommunicator::BroadcastServerEndpointDied()
{
    sendMsg->header.msgType = msgServerEndpointDied;
    sendMsg->header.count = 0;
    if (remoteVictimizer)  {
	W_DO( SendMsg(toVictimizer) );
    }
    
    int4 clientId = 0;
    while ((clientId = activeClientIds.FirstSetOnOrAfter(clientId)) != -1)  {
	sendMsg->header.clientId = clientId;
        W_DO( SendMsg() );
        clientId++;
    }

    return RCOK;
}


rc_t DeadlockServerCommunicator::ReceivedClientEndpointDied(Endpoint& theClientEndpoint)
{
    int4 clientId = 0;
    while ((clientId = activeClientIds.FirstSetOnOrAfter(clientId)) != -1)  {
	if (theClientEndpoint == clientEndpoints[clientId])  {
	    w_assert3(activeClientIds.IsBitSet(clientId));
	    activeClientIds.ClearBit(clientId);
	    deadlockServer->RemoveClient(clientId);
	    W_DO( IgnoreScDEAD( theClientEndpoint.stop_notify(myEndpoint) ) );
	    W_DO( clientEndpoints[clientId].release() );
	}
	clientId++;
    }
    W_DO( theClientEndpoint.release() );
    return RCOK;
}


rc_t DeadlockServerCommunicator::ReceivedVictimizerEndpointDied(Endpoint& theVictimizerEndpoint)
{
    if (remoteVictimizer)  {
	W_DO( victimizerEndpoint.stop_notify(myEndpoint) );
	W_DO( victimizerEndpoint.release() );
	remoteVictimizer = false;
    }
    W_DO( theVictimizerEndpoint.release() );
    return RCOK;
}


void DeadlockServerCommunicator::SetDeadlockServer(CentralizedGlobalDeadlockServer* server)
{
    delete deadlockServer;
    deadlockServer = server;
}


rc_t DeadlockServerCommunicator::SendMsg(MsgDestination destination)
{
    DBGTHRD( << "Deadlock server sending message:" << endl << " S->" << *sendMsg );
    uint4 clientId = sendMsg->header.clientId;
    sendMsg->hton();
    EndpointBox		box;
    if (sendMsg->header.msgType == msgServerEndpointDied)  {
	W_COERCE( box.set(0, myEndpoint) );
    }
    if (destination == toVictimizer)  {
	W_DO( IgnoreScDEAD( victimizerEndpoint.send(sendBuffer, box) ) );
    }  else if (destination == toClient)  {
	w_assert3(activeClientIds.IsBitSet(clientId));
	W_DO( IgnoreScDEAD( clientEndpoints[clientId].send(sendBuffer, box) ) );
    }

    return RCOK;
}


rc_t DeadlockServerCommunicator::RcvAndDispatchMsg()
{
    EndpointBox		box;
    W_DO( myEndpoint.receive(rcvBuffer, box) );
    rcvMsg->ntoh();
    DBGTHRD( << "Deadlock server received message:" << endl << " ->S" << *rcvMsg );
    switch ((DeadlockMsgType)rcvMsg->header.msgType)  {
	case msgVictimizerEndpoint:
	    {
		Endpoint ep;
		W_DO( box.get(0, ep) );
		W_DO( ReceivedVictimizerEndpoint(ep) );
	    }
	    break;
	case msgRequestClientId:
	    {
		Endpoint ep;
		W_DO( box.get(0, ep) );
		W_DO( ReceivedRequestClientId(ep) );
	    }
	    break;
	case msgRequestDeadlockCheck:
	    W_DO( ReceivedRequestDeadlockCheck() );
	    break;
	case msgWaitForList:
	    W_DO( ReceivedWaitForList(rcvMsg->header.clientId, rcvMsg->header.count, rcvMsg->gtids, rcvMsg->header.complete) );
	    break;
	case msgVictimSelected:
	    W_DO( ReceivedVictimSelected(rcvMsg->gtids[0]) );
	    break;
	case msgQuit:
	    done = true;
	    break;
	case msgClientEndpointDied:
	    {
		Endpoint theServerEndpoint;
		W_DO( box.get(0, theServerEndpoint) );
		W_DO( ReceivedClientEndpointDied(theServerEndpoint) );
	    }
	    break;
	case msgVictimizerEndpointDied:
	    {
		Endpoint theServerEndpoint;
		W_DO( box.get(0, theServerEndpoint) );
		W_DO( ReceivedVictimizerEndpointDied(theServerEndpoint) );
	    }
	    break;
	default:
	    w_assert1(0);
    }
    return RCOK;
}


void DeadlockServerCommunicator::SetClientEndpoint(uint4 clientId, Endpoint& ep)
{
    if (clientId >= clientEndpointsSize)  {
	ResizeClientEndpointsArray(clientEndpointsSize * 2);
    }

    clientEndpoints[clientId] = ep;
}


void DeadlockServerCommunicator::ResizeClientEndpointsArray(uint4 newSize)
{
    w_assert3(newSize > 0);
    Endpoint* newClientEndpoints = new Endpoint[newSize];
    for (uint4 i = 0; i < clientEndpointsSize; i++)  {
	newClientEndpoints[i] = clientEndpoints[i];
    }

    delete [] clientEndpoints;
    clientEndpointsSize = newSize;
    clientEndpoints = newClientEndpoints;
}


/***************************************************************************
 *                                                                         *
 * DeadlockVictimizerCommunicator class                                    *
 *                                                                         *
 ***************************************************************************/

DeadlockVictimizerCommunicator::DeadlockVictimizerCommunicator(
	const server_handle_t&		theServerHandle,
	name_ep_map*			ep_map,
	CommSystem&			commSystem,
	DeadlockVictimizerCallback&	callback)
:   serverHandle(theServerHandle),
    endpointMap(ep_map),
    sendBuffer(sizeof(DeadlockMsg)),
    rcvBuffer(sizeof(DeadlockMsg)),
    serverEndpointDiedBuffer(sizeof(DeadlockMsgHeader)),
    sendMsg(0),
    rcvMsg(0),
    gtidList(GtidElem::link_offset()),
    selectVictim(callback),
    done(false),
    serverEndpointValid(false)
{
    DeadlockMsgHeader* serverDiedMsg = (DeadlockMsgHeader*)serverEndpointDiedBuffer.start();
    serverDiedMsg->msgType = msgServerEndpointDied;
    serverDiedMsg->clientId = 0;
    serverDiedMsg->complete = true;
    serverDiedMsg->count = 0;
    serverDiedMsg->hton();

    sendMsg = (DeadlockMsg*)sendBuffer.start();
    memset(sendMsg, 0, sizeof(DeadlockMsg));
    rcvMsg = (DeadlockMsg*)rcvBuffer.start();
    memset(rcvMsg, 0, sizeof(DeadlockMsg));

    W_COERCE( commSystem.make_endpoint(myEndpoint) );
    W_COERCE( endpointMap->name2endpoint(serverHandle, serverEndpoint) );
    serverEndpointValid = true;

    W_COERCE( notify_always(serverEndpoint, myEndpoint, serverEndpointDiedBuffer) );
}


DeadlockVictimizerCommunicator::~DeadlockVictimizerCommunicator()
{
    W_COERCE( IgnoreScDEAD( serverEndpoint.stop_notify(myEndpoint) ) );
    W_COERCE( serverEndpoint.release() );
    
    W_COERCE( myEndpoint.release() );
    
    while (GtidElem* elem = gtidList.pop())
	delete elem;
}


rc_t DeadlockVictimizerCommunicator::ReceivedSelectVictim(uint2 count, gtid_t* gtids, bool complete)
{
    for (uint4 i = 0; i < count; i++)  {
	gtidList.push(new GtidElem(gtids[i]));
    }

    if (complete)  {
	gtid_t& gtid = selectVictim(gtidList);

	W_DO( SendVictimSelected(gtid) );

	while (GtidElem* gtidElem = gtidList.pop())  {
	    delete gtidElem;
	}
	w_assert3(gtidList.is_empty());
    }

    return RCOK;
}


rc_t DeadlockVictimizerCommunicator::SendVictimizerEndpoint()
{
    sendMsg->header.msgType = msgVictimizerEndpoint;
    W_DO( SendMsg() );
    
    return RCOK;
}


rc_t DeadlockVictimizerCommunicator::SendVictimSelected(const gtid_t& gtid)
{
    sendMsg->header.msgType = msgVictimSelected;
    sendMsg->header.count = 1;
    sendMsg->gtids[0] = gtid;
    W_DO( SendMsg() );

    return RCOK;
}


rc_t DeadlockVictimizerCommunicator::ReceivedServerEndpointDied(Endpoint& theServerEndpoint)
{
    if (serverEndpointValid)  {
	W_DO( serverEndpoint.stop_notify(myEndpoint) );
	W_DO( serverEndpoint.release() );
	serverEndpointValid = false;
    }
    W_DO( theServerEndpoint.release() );
    return RCOK;
}


rc_t DeadlockVictimizerCommunicator::SendMsg()
{
    DBGTHRD( << "Deadlock victimizer sending message:" << endl << " V->" << *sendMsg );
    sendMsg->hton();
    EndpointBox emptybox;
    W_DO( IgnoreScDEAD( serverEndpoint.send(sendBuffer, emptyBox) ) );
    return RCOK;
}


rc_t DeadlockVictimizerCommunicator::RcvAndDispatchMsg()
{
    EndpointBox		box;
    W_DO( myEndpoint.receive(rcvBuffer, box) );
    rcvMsg->ntoh();
    DBGTHRD( << "Deadlock victimizer received message:" << endl << " ->V" << *rcvMsg );
    switch ((DeadlockMsgType)rcvMsg->header.msgType)  {
	case msgSelectVictim:
	    W_DO( ReceivedSelectVictim(rcvMsg->header.count, rcvMsg->gtids, rcvMsg->header.complete) );
	    break;
	case msgQuit:
	    done = true;
	    break;
	case msgServerEndpointDied:
	    {
		Endpoint theServerEndpoint;
		W_DO( box.get(0, theServerEndpoint) );
		W_DO( ReceivedServerEndpointDied(theServerEndpoint) );
	    }
	    break;
	default:
	    w_assert1(0);
    }
    return RCOK;
}


/***************************************************************************
 *                                                                         *
 * CentralizedGlobalDeadlockClient class                                   *
 *                                                                         *
 ***************************************************************************/

CentralizedGlobalDeadlockClient::CentralizedGlobalDeadlockClient(
	int4				initial,
	int4				subsequent,
	DeadlockClientCommunicator*	clientCommunicator
)
:   initialTimeout(initial),
    subsequentTimeout(subsequent),
    communicator(clientCommunicator),
    thread(this),
    blockedListMutex("globalDeadlockClient"),
    blockedList(BlockedElem::link_offset()),
    isClientIdAssigned(false)
{
    w_assert3(communicator);
    communicator->SetDeadlockClient(this);
    W_COERCE( thread.Start() );
}


CentralizedGlobalDeadlockClient::~CentralizedGlobalDeadlockClient()
{
    thread.retire();
    W_COERCE( blockedListMutex.acquire() );

    while (BlockedElem* blockedElem = blockedList.pop())  {
	delete blockedElem;
    }

    blockedListMutex.release();

    delete communicator;
}


rc_t CentralizedGlobalDeadlockClient::AssignClientId()
{
    W_DO( blockedListMutex.acquire() );    
    isClientIdAssigned = true;
    bool is_empty = blockedList.is_empty();
    blockedListMutex.release();
    
    if (!is_empty)  {
        W_DO( communicator->SendRequestDeadlockCheck() );
    }
    
    return RCOK;
}


rc_t CentralizedGlobalDeadlockClient::GlobalXctLockWait(lock_request_t* req, const char * blockname)
{
    int4    timeout = initialTimeout;
    rc_t    rc;

    W_DO( blockedListMutex.acquire() );
    BlockedElem* blockedElem = new BlockedElem(req);
    blockedList.push(blockedElem);
    blockedListMutex.release();

    do  {
	DBGTHRD( << "waiting for global deadlock detection or lock, timeout=" << timeout );
	rc = me()->block(timeout, 0, blockname);
	if (rc.err_num() == stTIMEOUT)  {
	    if (blockedElem->collected || !isClientIdAssigned)  {
		timeout = WAIT_FOREVER;
	    }  else  {
		W_DO( communicator->SendRequestDeadlockCheck() );
		timeout = subsequentTimeout;
	    }
	}
    }  while (rc.err_num() == stTIMEOUT);

    if (rc)  {
	blockedElem->req->status(lock_m::t_aborted);
    }

    W_DO( blockedListMutex.acquire() );
    delete blockedElem;
    blockedListMutex.release();

    return rc;
}


rc_t CentralizedGlobalDeadlockClient::UnblockGlobalXct(const gtid_t& gtid)
{
    W_COERCE( blockedListMutex.acquire() );

    BlockedListIter iter(blockedList);
    while (BlockedElem* blockedElem = iter.next())  {
	if (blockedElem->req->xd->gtid() && *blockedElem->req->xd->gtid() == gtid)  {
	    if (smlevel_0::deadlockEventCallback)  {
		smlevel_0::deadlockEventCallback->KillingGlobalXct(blockedElem->req->xd, blockedElem->req->get_lock_head()->name);
	    }
	    W_DO( blockedElem->req->thread->unblock(RC(smlevel_0::eDEADLOCK)) );
	}
    }

    blockedListMutex.release();

    return RCOK;
}


rc_t CentralizedGlobalDeadlockClient::SendWaitForGraph()
{
    WaitForList waitForList(WaitForElem::link_offset());

    W_DO( blockedListMutex.acquire() );

    BlockedListIter iter(blockedList);
    while (BlockedElem* blockedElem = iter.next())  {
	blockedElem->collected = true;
	W_DO( AddToWaitForList(waitForList, blockedElem->req, *blockedElem->req->xd->gtid()) );
    }

    blockedListMutex.release();

    W_DO( communicator->SendWaitForList(waitForList) );
    w_assert3(waitForList.is_empty());

    return RCOK;
}


rc_t CentralizedGlobalDeadlockClient::NewServerEndpoint()
{
    W_DO( SendRequestClientId() );

    return RCOK;
}


bool CentralizedGlobalDeadlockClient::Done()
{
    return communicator->Done();
}


rc_t CentralizedGlobalDeadlockClient::RcvAndDispatchMsg()
{
    return communicator->RcvAndDispatchMsg();
}


rc_t CentralizedGlobalDeadlockClient::SendQuit()
{
    return communicator->SendQuit();
}


rc_t CentralizedGlobalDeadlockClient::SendRequestClientId()
{
    return communicator->SendRequestClientId();
}


rc_t CentralizedGlobalDeadlockClient::AddToWaitForList(
	WaitForList&		waitForList,
	const lock_request_t*	req,
	const gtid_t&		gtid
)
{
    xct_t* xd = req->xd;
    w_assert3(xd);

    if (xd->lock_info()->wait != req)  {
	// This request is granted, but the thread which was blocked hasn't run yet
	// to take it off the list of global xct's which are blocked on a lock.
	w_assert3(req->status() == lock_m::t_granted);
	return RCOK;
    }

    w_assert3(req->status() == lock_m::t_waiting || req->status() == lock_m::t_converting);

    w_list_i<lock_request_t> iter(req->get_lock_head()->queue);

    if (req->status() == lock_m::t_waiting)  {
        mode_t req_mode = req->mode;
	while (lock_request_t* other = iter.next())  {
	    if (other->xd == xd)
		break;
	    if (other->status() == lock_m::t_aborted)
		continue;
	    if (!lock_base_t::compat[other->mode][req_mode] || other->status() == lock_m::t_waiting
					|| other->status() == lock_m::t_converting)  {
		if (other->xd->is_extern2pc())  {
		    waitForList.push(new WaitForElem(gtid, *other->xd->gtid()));
		}  else if (other->xd->lock_info()->wait)  {
		    AddToWaitForList(waitForList, other->xd->lock_info()->wait, gtid);
		}
	    }
	}
    }  else  {		// req->status() == lock_m::t_converting
        mode_t req_mode = xd->lock_info()->wait->convert_mode;
	while (lock_request_t* other = iter.next())  {
            if (other->xd == xd || other->status() == lock_m::t_aborted)
                continue;
            if (other->status() == lock_m::t_waiting)
                break;
            if (!lock_base_t::compat[other->mode][req_mode])  {
                if (other->xd->is_extern2pc())  {
                    waitForList.push(new WaitForElem(gtid, *other->xd->gtid()));
                }  else if (other->xd->lock_info()->wait)  {
                    AddToWaitForList(waitForList, other->xd->lock_info()->wait, gtid);
                }
	    }
	}
    }

    return RCOK;
}



/***************************************************************************
 *                                                                         *
 * CentralizedGlobalDeadlockClient::ReceiverThread class                   *
 *                                                                         *
 ***************************************************************************/

NORET
CentralizedGlobalDeadlockClient::ReceiverThread::ReceiverThread(CentralizedGlobalDeadlockClient* client)
:   deadlockClient(client),
    smthread_t(t_regular, false, false, "DeadlockClient")
{
    w_assert3(deadlockClient);
}


NORET
CentralizedGlobalDeadlockClient::ReceiverThread::~ReceiverThread()
{
}


rc_t CentralizedGlobalDeadlockClient::ReceiverThread::Start()
{
    W_DO( deadlockClient->SendRequestClientId() );
    W_DO( this->fork() );
    return RCOK;
}


void CentralizedGlobalDeadlockClient::ReceiverThread::run()
{
    while (!deadlockClient->Done())  {
	W_COERCE( deadlockClient->RcvAndDispatchMsg() );
    }
}


void CentralizedGlobalDeadlockClient::ReceiverThread::retire()
{
    W_COERCE( deadlockClient->SendQuit() );
    this->wait();
}


/***************************************************************************
 *                                                                         *
 * DeadlockGraph class                                                     *
 *                                                                         *
 ***************************************************************************/

DeadlockGraph::DeadlockGraph(uint4 initialNumberOfXcts)
:   maxGtidIndex(0),
    maxUsedGtidIndex(0),
    original(0),
    closure(0)
{
    w_assert1(initialNumberOfXcts % BitsPerWord == 0);
    w_assert1(initialNumberOfXcts > 0);

    maxGtidIndex = initialNumberOfXcts;

    BitMapVector** newBitMap = new BitMapVector*[maxGtidIndex];
    uint4 i=0;
    for (i = 0; i < maxGtidIndex; i++)  {
	newBitMap[i] = new BitMapVector(initialNumberOfXcts);
    }
    original = newBitMap;

    newBitMap = new BitMapVector*[maxGtidIndex];
    for (i = 0; i < maxGtidIndex; i++)  {
	newBitMap[i] = new BitMapVector(initialNumberOfXcts);
    }
    closure = newBitMap;
}


DeadlockGraph::~DeadlockGraph()
{
    uint4 i;
    if (original)  {
	for (i = 0; i < maxGtidIndex; i++)
	    delete original[i];
	
	delete [] original;
    }

    if (closure)  {
	for (i = 0; i < maxGtidIndex; i++)
	    delete closure[i];
	
	delete [] closure;
    }
}


void DeadlockGraph::ClearGraph()
{
    uint4 i;
    for (i = 0; i < maxGtidIndex; i++)  {
	original[i]->ClearAll();
	closure[i]->ClearAll();
    }
    maxUsedGtidIndex = 0;
}


void DeadlockGraph::Resize(uint4 newSize)
{
    uint4 i=0;
    if (newSize >= maxGtidIndex)  {
	newSize = (newSize + BitsPerWord) / BitsPerWord * BitsPerWord;

	BitMapVector** newOriginal = new BitMapVector*[newSize];
	BitMapVector** newClosure = new BitMapVector*[newSize];
	for (i = 0; i < maxGtidIndex; i++)  {
	    newOriginal[i] = original[i];
	    newClosure[i] = closure[i];
	}
	for (i = maxGtidIndex; i < newSize; i++)  {
	    newOriginal[i] = new BitMapVector(newSize);
	    newClosure[i] = new BitMapVector(newSize);
	}

	delete [] original;
	delete [] closure;

	original = newOriginal;
	closure = newClosure;
	maxGtidIndex = newSize;
    }  else  {
	w_assert3(0);
    }
}


void DeadlockGraph::AddEdge(uint4 waitGtidIndex, uint4 forGtidIndex)
{
    if (waitGtidIndex >= maxGtidIndex)  {
	Resize(waitGtidIndex);
	w_assert3(waitGtidIndex < maxGtidIndex);
    }

    if (waitGtidIndex > maxUsedGtidIndex)
	maxUsedGtidIndex = waitGtidIndex;
    
    if (forGtidIndex > maxUsedGtidIndex)
	maxUsedGtidIndex = forGtidIndex;

    original[waitGtidIndex]->SetBit(forGtidIndex);
}


void DeadlockGraph::ComputeTransitiveClosure(GtidIndexList& cycleParticipantsList)
{
    uint4 i;
    for (i = 0; i <= maxUsedGtidIndex; i++)
	*closure[i] = *original[i];
    
    bool changed;
    do  {
	changed = false;
	for (uint4 waitIndex = 0; waitIndex <= maxUsedGtidIndex; waitIndex++)  {
	    int4 forIndex = 0;
	    while ((forIndex = closure[waitIndex]->FirstSetOnOrAfter(forIndex)) != -1)  {
		closure[waitIndex]->OrInBitVector(*closure[forIndex], changed);
		forIndex++;
	    }
	}
    }  while (changed);

    for (i = 0; i <= maxUsedGtidIndex; i++)  {
	if (QueryWaitsFor(i, i))
	    cycleParticipantsList.push(new GtidIndexElem(i));
    }
}


void DeadlockGraph::KillGtidIndex(uint4 gtidIndex)
{
    if (gtidIndex < maxGtidIndex)
        original[gtidIndex]->ClearAll();
}


bool DeadlockGraph::QueryWaitsFor(uint4 waitGtidIndex, uint4 forGtidIndex)
{
    if (waitGtidIndex < maxGtidIndex)
        return closure[waitGtidIndex]->IsBitSet(forGtidIndex);
    else
	return false;
}


/***************************************************************************
 *                                                                         *
 * CentralizedGlobalDeadlockServer class                                   *
 *                                                                         *
 ***************************************************************************/

CentralizedGlobalDeadlockServer::CentralizedGlobalDeadlockServer(DeadlockServerCommunicator* serverCommunicator)
:   communicator(serverCommunicator),
    thread(this),
    state(IdleState),
    checkRequested(false),
    numGtids(0),
    gtidTable(127, offsetof(GtidTableElem, gtid), offsetof(GtidTableElem, _link)),
    idToGtid(0),
    idToGtidSize(0)
{
    w_assert3(communicator);
    communicator->SetDeadlockServer(this);
    idToGtidSize = 32;
    idToGtid = new gtid_t*[idToGtidSize];
    w_assert1(idToGtid);
    for (uint i = 0; i < idToGtidSize; i++)
	idToGtid[i] = 0;
    W_COERCE( thread.Start() );
}


CentralizedGlobalDeadlockServer::~CentralizedGlobalDeadlockServer()
{
    thread.retire();
    delete communicator;
    delete [] idToGtid;
    GtidTableIter iter(gtidTable);
    while (GtidTableElem* tableElem = iter.next())  {
	gtidTable.remove(tableElem);
	delete tableElem;
    }
}


bool CentralizedGlobalDeadlockServer::Done()
{
    return communicator->Done();
}


rc_t CentralizedGlobalDeadlockServer::RcvAndDispatchMsg()
{
    return communicator->RcvAndDispatchMsg();
}


rc_t CentralizedGlobalDeadlockServer::SendQuit()
{
    return communicator->SendQuit();
}


rc_t CentralizedGlobalDeadlockServer::AddClient(uint4 id)
{
    w_assert3(!activeIds.IsBitSet(id));
    w_assert3(!collectedIds.IsBitSet(id));
    activeIds.SetBit(id);
    if (state == CollectingState)  {
	W_DO( SendRequestWaitForList(id) );
    }

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::RemoveClient(uint4 id)
{
    w_assert3(activeIds.IsBitSet(id));
    activeIds.ClearBit(id);
    collectedIds.ClearBit(id);

    GtidTableIter iter(gtidTable);
    while (GtidTableElem* tableElem = iter.next())  {
	tableElem->nodeIds.ClearBit(id);
    }
    
    if (state == CollectingState && collectedIds == activeIds)  {
	W_DO( CheckDeadlock() );
    }

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::KillGtid(uint4 clientId, const gtid_t& gtid)
{
    W_DO( communicator->SendKillGtid(clientId, gtid) );

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::CheckDeadlockRequested()
{
    if (state == IdleState)  {
	W_DO( BroadcastRequestWaitForList() );
    }  else  {
	checkRequested = true;
    }

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::BroadcastRequestWaitForList()
{
    w_assert3(state == IdleState);
    state = CollectingState;

    W_DO( communicator->BroadcastRequestWaitForList() );

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::SendRequestWaitForList(uint4 id)
{
    W_DO( communicator->SendRequestWaitForList(id) );
    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::AddWaitFors(uint4 clientId, WaitPtrForPtrList& waitForList, bool complete)
{
    w_assert3(state == CollectingState);
    w_assert3(activeIds.IsBitSet(clientId));
    w_assert3(!collectedIds.IsBitSet(clientId));

    while (WaitPtrForPtrElem* elem = waitForList.pop())  {
	uint4 waitId;
	uint4 forId;

	GtidTableElem* gtidTableElem = GetGtidTableElem(*elem->waitGtid);
	gtidTableElem->nodeIds.SetBit(clientId);
	waitId = gtidTableElem->id;

	gtidTableElem = GetGtidTableElem(*elem->forGtid);
	forId = gtidTableElem->id;

	deadlockGraph.AddEdge(waitId, forId);

	delete elem;
    }

    if (complete)  {
        collectedIds.SetBit(clientId);
        if (collectedIds == activeIds)  {
	    state = SelectingVictimState;
            W_DO( CheckDeadlock() );
	}
    }

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::SelectVictim(GtidIndexList& deadlockedList)
{
    W_DO( communicator->SendSelectVictim(deadlockedList) );
    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::VictimSelected(const gtid_t& gtid)
{
    GtidTableElem* tableElem = gtidTable.lookup(gtid);
    w_assert1(tableElem);

    int4 i = 0;
    while ((i = tableElem->nodeIds.FirstSetOnOrAfter(i)) != -1)  {
	KillGtid(i, gtid);
	i++;
    }

    deadlockGraph.KillGtidIndex(tableElem->id);

    W_DO( CheckDeadlock() );
	
    return RCOK;
}

rc_t CentralizedGlobalDeadlockServer::CheckDeadlock()
{
    w_assert3(state == SelectingVictimState);
    GtidIndexList cycleParticipantList(GtidIndexElem::link_offset());
    deadlockGraph.ComputeTransitiveClosure(cycleParticipantList);

    if (cycleParticipantList.is_empty())  {
        W_DO( ResetServer() );
    }  else  {
        W_DO( SelectVictim(cycleParticipantList) );
    }
    w_assert3(cycleParticipantList.is_empty());

    return RCOK;
}


rc_t CentralizedGlobalDeadlockServer::ResetServer()
{
    w_assert3(state == SelectingVictimState);

    deadlockGraph.ClearGraph();
    collectedIds.ClearAll();
    state = IdleState;

    numGtids = 0;
    GtidTableIter iter(gtidTable);
    while (GtidTableElem* tableElem = iter.next())  {
	gtidTable.remove(tableElem);
	delete tableElem;
    }

    if (checkRequested)  {
	W_DO( BroadcastRequestWaitForList() );
    }

    checkRequested = false;

    return RCOK;
}


GtidTableElem* CentralizedGlobalDeadlockServer::GetGtidTableElem(const gtid_t& gtid)
{
    GtidTableElem* gtidTableElem = gtidTable.lookup(gtid);
    uint4 i=0;

    if (gtidTableElem == 0)  {
	gtidTableElem = new GtidTableElem(gtid, numGtids);
	w_assert1(gtidTableElem);
	gtidTable.push(gtidTableElem);
	w_assert3(gtidTable.lookup(gtid));
    }

    if (numGtids >= idToGtidSize)  {
	uint4 newIdToGtidSize = 2 * idToGtidSize;
	gtid_t** newIdToGtid = new gtid_t*[newIdToGtidSize];
	w_assert1(newIdToGtid);
	for (i = 0; i < idToGtidSize; i++)
	    newIdToGtid[i] = idToGtid[i];
	
	for (i = idToGtidSize; i < newIdToGtidSize; i++)
	    newIdToGtid[i] = 0;
	
	delete [] idToGtid;
	idToGtid = newIdToGtid;
	idToGtidSize = newIdToGtidSize;
    }

    idToGtid[gtidTableElem->id] = &gtidTableElem->gtid;
    numGtids++;
    
    return gtidTableElem;
}


const gtid_t& CentralizedGlobalDeadlockServer::GtidIndexToGtid(uint4 index)
{
    w_assert3(index < idToGtidSize);
    return *idToGtid[index];
}


/***************************************************************************
 *                                                                         *
 * CentralizedGlobalDeadlockServer::ReceiverThread class                   *
 *                                                                         *
 ***************************************************************************/

NORET
CentralizedGlobalDeadlockServer::ReceiverThread::ReceiverThread(CentralizedGlobalDeadlockServer* Server)
:   deadlockServer(Server),
    smthread_t(t_regular, false, false, "DeadlockServer")
{
    w_assert3(deadlockServer);
}


NORET
CentralizedGlobalDeadlockServer::ReceiverThread::~ReceiverThread()
{
}


rc_t CentralizedGlobalDeadlockServer::ReceiverThread::Start()
{
    W_DO( this->fork() );
    return RCOK;
}


void CentralizedGlobalDeadlockServer::ReceiverThread::run()
{
    while (!deadlockServer->Done())  {
	W_COERCE( deadlockServer->RcvAndDispatchMsg() );
    }
}


void CentralizedGlobalDeadlockServer::ReceiverThread::retire()
{
    W_COERCE( deadlockServer->SendQuit() );
    this->wait();
}
