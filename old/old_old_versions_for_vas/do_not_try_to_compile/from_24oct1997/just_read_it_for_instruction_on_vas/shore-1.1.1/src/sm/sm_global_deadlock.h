/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm_global_deadlock.h,v 1.12 1997/05/19 19:48:08 nhall Exp $
 */

#ifndef SM_GLOBAL_DEADLOCK_H
#define SM_GLOBAL_DEADLOCK_H

#ifdef __GNUG__
#pragma interface
#endif

#ifndef GLOBAL_DEADLOCK_H
#include <global_deadlock.h>
#endif
#ifndef MAPPINGS_H
#include <mappings.h>
#endif
#include <netinet/in.h>

class BlockedElem  {
    public:
	lock_request_t*		    req;
	bool			    collected;
	w_link_t		    _link;

	NORET			    BlockedElem(lock_request_t* theReq)
		: req(theReq), collected(false)
		{};
	NORET			    ~BlockedElem()
		{
		    if (_link.member_of() != NULL)
			_link.detach();
		};
	static uint4		    link_offset()
		{
		    return offsetof(BlockedElem, _link);
		};
	W_FASTNEW_CLASS_DECL;
};


class WaitForElem  {
    public:
	gtid_t			    waitGtid;
	gtid_t			    forGtid;
	w_link_t		    _link;

	NORET			    WaitForElem(
	    const gtid_t&		theWaitGtid,
	    const gtid_t&		theForGtid)
		:  waitGtid(theWaitGtid), forGtid(theForGtid)
		{};
	NORET                       ~WaitForElem()
		{
		    if (_link.member_of() != NULL)
		    _link.detach();
		};
	static uint4                link_offset()
		{
		    return offsetof(WaitForElem, _link);
		};
	W_FASTNEW_CLASS_DECL;
};


class WaitPtrForPtrElem  {
    public:
	const gtid_t*		    waitGtid;
	const gtid_t*		    forGtid;
	w_link_t		    _link;

	NORET			    WaitPtrForPtrElem(
	    const gtid_t*		theWaitGtid,
	    const gtid_t*		theForGtid)
		:  waitGtid(theWaitGtid), forGtid(theForGtid)
		{};
	NORET                       ~WaitPtrForPtrElem()
		{
		    if (_link.member_of() != NULL)
		    _link.detach();
		};
	static uint4                link_offset()
		{
		    return offsetof(WaitPtrForPtrElem, _link);
		};
	W_FASTNEW_CLASS_DECL;
};


class GtidIndexElem  {
    public:
	uint4			    index;
	w_link_t		    _link;

	NORET			    GtidIndexElem(uint4 i)
					: index(i)
					{};
	NORET			    ~GtidIndexElem()
					{
					    if (_link.member_of() != NULL)
						_link.detach();
					};
	static uint4		    link_offset()
					{
					    return offsetof(GtidIndexElem, _link);
					};
	W_FASTNEW_CLASS_DECL;
};



typedef w_list_t<BlockedElem>		BlockedList;
typedef w_list_i<BlockedElem>		BlockedListIter;
typedef w_list_t<WaitForElem>		WaitForList;
typedef w_list_i<WaitForElem>		WaitForListIter;
typedef w_list_t<WaitPtrForPtrElem>	WaitPtrForPtrList;
typedef w_list_i<WaitPtrForPtrElem>	WaitPtrForPtrListIter;
typedef w_list_t<GtidIndexElem>		GtidIndexList;
typedef w_list_i<GtidIndexElem>		GtidIndexListIter;


class BitMapVector  {
    public:
	NORET			    BitMapVector(uint4 numberOfBits = 32);
	NORET			    BitMapVector(const BitMapVector& v);
	BitMapVector&		    operator=(const BitMapVector& v);
	bool			    operator==(const BitMapVector& v);
	bool			    operator!=(const BitMapVector& v)
		{
		    return !(*this == v);
		};
	NORET			    ~BitMapVector();

	void			    ClearAll();
	int4			    FirstSetOnOrAfter(uint4 index) const;
	int4			    FirstClearOnOrAfter(uint4 index = 0) const;
	void			    SetBit(uint4 bitNumber);
	void			    ClearBit(uint4 bitNumber);
	bool			    IsBitSet(uint4 bitNumber) const;
	void			    OrInBitVector(const BitMapVector& v, bool& changed);

	W_FASTNEW_CLASS_DECL;

    private:
	uint4			    size;
	uint4*			    vector;
	enum			    { BitsPerWord = sizeof(uint4) * 8 };

	void			    Resize(uint4 numberOfBits);
};


enum DeadlockMsgType    {
    msgVictimizerEndpoint,
    msgRequestClientId,
    msgAssignClientId,
    msgClientDied,
    msgRequestDeadlockCheck,
    msgRequestWaitFors,
    msgWaitForList,
    msgSelectVictim,
    msgVictimSelected,
    msgKillGtid,
    msgQuit,
    msgClientEndpointDied,
    msgVictimizerEndpointDied,
    msgServerEndpointDied
};


struct DeadlockMsgHeader  {
    uint1			    msgType;
    uint1			    complete;
    uint2			    count;
    uint4			    clientId;

    void 			    hton()
				    {
					clientId = htonl(clientId);
					count = htons(count);
				    }
    void			    ntoh()
				    {
					clientId = ntohl(clientId);
					count = ntohs(count);
				    }
};


struct DeadlockMsg  {
    enum			    { MaxNumGtidsPerMsg = 32 };
    
    DeadlockMsgHeader		    header;
    gtid_t			    gtids[MaxNumGtidsPerMsg];

    void			    hton()
				    {
					for (int i = 0; i < header.count; i++)
					    gtids[i].hton();
					header.hton();
				    };

    void			    ntoh()
    				    {
					header.ntoh();
					for (int i = 0; i < header.count; i++)
					    gtids[i].ntoh();
				    };

    static const char* msgNameStrings[];
};

ostream& operator<<(ostream& o, const DeadlockMsg& msg);


class CentralizedGlobalDeadlockClient;

class DeadlockClientCommunicator
{
    public:
        friend CentralizedGlobalDeadlockClient;
        
	NORET			    DeadlockClientCommunicator(
	    const server_handle_t&	theServerHandle,
	    name_ep_map*		ep_map,
	    CommSystem&			commSystem);
	NORET			    ~DeadlockClientCommunicator();
	bool			    Done() const
					{
					    return done;
					};

    private:
	rc_t			    SendRequestClientId();
	rc_t			    ReceivedAssignClientId(uint4 clientId);
	rc_t			    SendRequestDeadlockCheck();
	rc_t			    SendWaitForList(WaitForList& waitForList);
	rc_t			    ReceivedRequestWaitForList();
	rc_t			    ReceivedKillGtid(const gtid_t& gtid);
	rc_t			    SendQuit();
	rc_t			    SendClientEndpointDied();
	rc_t			    ReceivedServerEndpointDied(Endpoint& theServerEndpoint);
	void			    SetDeadlockClient(CentralizedGlobalDeadlockClient * client);
	
	CentralizedGlobalDeadlockClient*	deadlockClient;

	const server_handle_t	    serverHandle;
	name_ep_map*		    endpointMap;

	Endpoint		    serverEndpoint;
	Endpoint		    myEndpoint;

	uint4			    myId;

	Buffer			    sendBuffer;
	Buffer			    rcvBuffer;
	Buffer			    serverEndpointDiedBuffer;
	DeadlockMsg*		    sendMsg;
	DeadlockMsg*		    rcvMsg;
	CommSystem&		    comm;
	bool			    done;
	bool			    serverEndpointValid;

	rc_t			    SendMsg();
	rc_t			    RcvAndDispatchMsg();
};


class DeadlockVictimizerCallback
{
    public:
	virtual gtid_t& operator()(GtidList& gtidList) = 0;
};


class PickFirstDeadlockVictimizerCallback : public DeadlockVictimizerCallback
{
    public:
	gtid_t& operator()(GtidList& gtidList)
	    {
		w_assert3(!gtidList.is_empty());
		return gtidList.top()->gtid;
	    };
};


class CentralizedGlobalDeadlockServer;
extern PickFirstDeadlockVictimizerCallback selectFirstVictimizer;

class DeadlockServerCommunicator
{
    public:
        friend CentralizedGlobalDeadlockServer;
        
	NORET			    DeadlockServerCommunicator(
	    const server_handle_t&	theServerHandle,
	    name_ep_map*		ep_map,
	    CommSystem&			theCommSystem,
	    DeadlockVictimizerCallback& theCallback = selectFirstVictimizer);
	NORET			    ~DeadlockServerCommunicator();
	bool			    Done() const
					{
					    return done;
					}
	
    private:
	rc_t			    ReceivedVictimizerEndpoint(Endpoint& ep);
	rc_t			    ReceivedRequestClientId(Endpoint& ep);
	rc_t			    SendAssignClientId(uint4 clientId);
	rc_t			    ReceivedRequestDeadlockCheck();
	rc_t			    BroadcastRequestWaitForList();
	rc_t			    SendRequestWaitForList(uint4 clientId);
	rc_t			    ReceivedWaitForList(uint4 clientId, uint2 count, const gtid_t* gtids, bool complete);
	rc_t			    SendSelectVictim(GtidIndexList& deadlockList);
	rc_t			    ReceivedVictimSelected(const gtid_t gtid);
	rc_t			    SendKillGtid(uint4 clientId, const gtid_t gtid);
	rc_t			    SendQuit();
	rc_t			    BroadcastServerEndpointDied();
	rc_t			    ReceivedClientEndpointDied(Endpoint& theClientEndpoint);
	rc_t			    ReceivedVictimizerEndpointDied(Endpoint& theVictimimzerEndpoint);
	void			    SetDeadlockServer(CentralizedGlobalDeadlockServer* server);
	CentralizedGlobalDeadlockServer*	deadlockServer;

	const server_handle_t	    serverHandle;
	name_ep_map*		    endpointMap;

	CommSystem&		    commSystem;
	Endpoint		    myEndpoint;
	Endpoint		    victimizerEndpoint;
	enum			    { InitialNumberOfEndpoints = 20 };
	uint4			    clientEndpointsSize;
	Endpoint*		    clientEndpoints;

	Buffer			    sendBuffer;
	Buffer			    rcvBuffer;
	Buffer			    clientEndpointDiedBuffer;
	Buffer			    victimizerEndpointDiedBuffer;
	DeadlockMsg*		    sendMsg;
	DeadlockMsg*		    rcvMsg;
	BitMapVector		    activeClientIds;
	DeadlockVictimizerCallback& selectVictim;
	bool			    remoteVictimizer;
	bool			    done;
	
	enum MsgDestination	    { toVictimizer, toClient, toServer };
	rc_t			    SendMsg(MsgDestination destination = toClient);
	rc_t			    RcvAndDispatchMsg();
	void			    SetClientEndpoint(uint4 clientId, Endpoint& ep);
	void			    ResizeClientEndpointsArray(uint4 newSize);
};


class DeadlockVictimizerCommunicator
{
    public:
	NORET			    DeadlockVictimizerCommunicator(
	    const server_handle_t&	theServerHandle,
	    name_ep_map*		ep_map,
	    CommSystem&			commSystem,
	    DeadlockVictimizerCallback& theCallback);
	~DeadlockVictimizerCommunicator();
	rc_t			    SendVictimizerEndpoint();
	rc_t			    ReceivedSelectVictim(uint2 count, gtid_t* gtids, bool complete);
	rc_t			    SendVictimSelected(const gtid_t& gtid);
	rc_t			    ReceivedServerEndpointDied(Endpoint& theServerEndpoint);
    
    private:
	const server_handle_t	    serverHandle;
	name_ep_map*		    endpointMap;

	Endpoint		    serverEndpoint;
	Endpoint		    myEndpoint;

	Buffer			    sendBuffer;
	Buffer			    rcvBuffer;
	Buffer			    serverEndpointDiedBuffer;
	DeadlockMsg*		    sendMsg;
	DeadlockMsg*		    rcvMsg;
	GtidList		    gtidList;
	DeadlockVictimizerCallback& selectVictim;
	bool			    done;
	bool			    serverEndpointValid;

	rc_t			    SendMsg();
	rc_t			    RcvAndDispatchMsg();
};


class CentralizedGlobalDeadlockClient : public GlobalDeadlockClient  {
    private:
        class ReceiverThread : public smthread_t
        {
	    public:
		NORET ReceiverThread(CentralizedGlobalDeadlockClient* client);
		NORET ~ReceiverThread();
		rc_t Start();
		void run();
		void retire();

	    private:
		CentralizedGlobalDeadlockClient* deadlockClient;
        };

    public:
	NORET			    CentralizedGlobalDeadlockClient(
	    int4			initialTimeout,
	    int4			subsequentTimeout,
	    DeadlockClientCommunicator* clientCommunicator);
	NORET			    ~CentralizedGlobalDeadlockClient();
	rc_t			    AssignClientId();
        rc_t    		    GlobalXctLockWait(
	    lock_request_t*		req,         
	    const char *		blockname);
        rc_t    		    UnblockGlobalXct(
	    const gtid_t&		gtid);
        rc_t    		    SendWaitForGraph();
        rc_t			    NewServerEndpoint();

	bool			    Done();
	rc_t			    RcvAndDispatchMsg();
	rc_t			    SendQuit();
	rc_t			    SendRequestClientId();

    private:
	int4			    initialTimeout;
	int4			    subsequentTimeout;
	DeadlockClientCommunicator* communicator;
	ReceiverThread		    thread;

	smutex_t		    blockedListMutex;
	BlockedList		    blockedList;
	bool			    isClientIdAssigned;

	rc_t			    AddToWaitForList(
	    WaitForList&		waitForList,
	    const lock_request_t*	req,
	    const gtid_t&		gtid);


	/* disallow copying and assignment */
	NORET					    CentralizedGlobalDeadlockClient(
	    const CentralizedGlobalDeadlockClient&);
	CentralizedGlobalDeadlockClient& 	    operator=(
	    const CentralizedGlobalDeadlockClient&);
};


class DeadlockGraph  {
    public:
	NORET			    DeadlockGraph(uint4 initialNumberOfXcts = 32);
	NORET			    ~DeadlockGraph();
	void			    ClearGraph();
	void			    AddEdge(uint4 waitGtidIndex, uint4 forGtidIndex);
	void			    ComputeTransitiveClosure(GtidIndexList& cycleParticipantsList);
	void			    KillGtidIndex(uint4 gtidIndex);
	bool			    QueryWaitsFor(uint4 waitGtidIndex, uint4 forGtidIndex);

    private:
	uint4			    maxGtidIndex;
	uint4			    maxUsedGtidIndex;
	BitMapVector**		    original;
	BitMapVector**		    closure;
	enum			    { BitsPerWord = sizeof(uint4) * 8 };

	void			    Resize(uint4 newSize);
};


class GtidTableElem  {
    public:
	NORET			    GtidTableElem(gtid_t g, uint4 i)
					: gtid(g), id(i)
					{};
	gtid_t			    gtid;
	uint4			    id;
	BitMapVector		    nodeIds;
	w_link_t		    _link;

	W_FASTNEW_CLASS_DECL;
};

typedef w_hash_t<GtidTableElem, const gtid_t> GtidTable;
typedef w_hash_i<GtidTableElem, const gtid_t> GtidTableIter;

w_base_t::uint4_t hash(const gtid_t &g);


class CentralizedGlobalDeadlockServer  {
    private:
	friend DeadlockServerCommunicator;
        class ReceiverThread : public smthread_t
        {
	    public:
		NORET ReceiverThread(CentralizedGlobalDeadlockServer* server);
		NORET ~ReceiverThread();
		rc_t Start();
                void run();
                void retire();
	    private:
		CentralizedGlobalDeadlockServer* deadlockServer;
        };

    public:
	NORET			    CentralizedGlobalDeadlockServer(DeadlockServerCommunicator* serverCommunicator);
	NORET			    ~CentralizedGlobalDeadlockServer();

	bool			    Done();
	rc_t			    RcvAndDispatchMsg();
	rc_t			    SendQuit();

    private:
	rc_t			    AddClient(uint4 id);
	rc_t			    RemoveClient(uint4 id);
	rc_t			    KillGtid(uint4 clientId, const gtid_t& gtid);
	rc_t			    CheckDeadlockRequested();
	rc_t			    BroadcastRequestWaitForList();
	rc_t			    SendRequestWaitForList(uint4 id);
	rc_t			    AddWaitFors(uint4 clientId, WaitPtrForPtrList& waitForList, bool complete);
	rc_t			    SelectVictim(GtidIndexList& deadlockedList);
	rc_t			    VictimSelected(const gtid_t& gtid);
	rc_t			    CheckDeadlock();
	rc_t			    ResetServer();
	GtidTableElem*		    GetGtidTableElem(const gtid_t& gtid);
	const gtid_t&		    GtidIndexToGtid(uint4 index);

	DeadlockGraph		    deadlockGraph;
	DeadlockServerCommunicator* communicator;
	ReceiverThread		    thread;
	enum State		    {IdleState, CollectingState, SelectingVictimState};
	State			    state;
	bool			    checkRequested;
	BitMapVector		    activeIds;
	BitMapVector		    collectedIds;
	uint4			    numGtids;
	GtidTable		    gtidTable;
	gtid_t**                    idToGtid;
	uint4			    idToGtidSize;
};



#endif
