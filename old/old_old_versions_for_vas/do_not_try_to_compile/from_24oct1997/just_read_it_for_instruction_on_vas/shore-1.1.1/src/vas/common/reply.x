
/*
	$Header: /p/shore/shore_cvs/src/vas/common/reply.x,v 1.28 1995/11/16 22:07:14 nhall Exp $
*/
struct void_reply {
	ShoreStatus	status;
};

struct voidref_reply {
	ShoreStatus	status;
	u_char	result<>;
};

struct lvid_t_reply {
	ShoreStatus	status;
	lvid_t	result;
};

struct int_reply {
	ShoreStatus	status;
	int		result;
};

struct short_reply {
	ShoreStatus	status;
	short	result;
};

struct statindex_reply {
	ShoreStatus	status;
	struct indexstatinfo	result;
};

struct u_int_reply {
	ShoreStatus	status;
	u_int	result;
};

struct u_short_reply {
	ShoreStatus	status;
	u_short	result;
};

struct bool_reply {
	ShoreStatus	status;
	small_bool_t	result;
};

struct char_reply {
	ShoreStatus	status;
	char	result;
};

struct u_char_reply {
	ShoreStatus	status;
	u_char	result;
};

struct lrid_t_reply {
	ShoreStatus	status;
	lrid_t	result;
};

struct lookup_reply {
	ShoreStatus	status;
	lrid_t	result;
	small_bool_t	found;
};

struct mkanon_reply {
	ShoreStatus	status;
	lrid_t	result;
	lrid_t	pooloid;
};

struct init_reply {
	ShoreStatus	status;
	int 	sockbufsize;
	int 	page_size;
	int 	num_page_bufs;
	int		num_lg_bufs; 
	int		shmid; /* shm id type is int; 0 if over-the-wire */
};

struct POID_reply {
	ShoreStatus	status;
	POID	result;
};

struct Path_reply {
	ShoreStatus	status;
	Path	result;
};

struct Cookie_reply {
	ShoreStatus	status;
	Cookie	result;
};

struct c_mode_t_reply {
	ShoreStatus	status;
	c_mode_t result;
};

struct c_uid_t_reply {
	ShoreStatus	status;
	c_uid_t	result;
};

struct c_gid_t_reply {
	ShoreStatus	status;
	c_gid_t	result;
};

struct IndexId_reply {
	ShoreStatus	status;
	IndexId	result;
};

struct tid_t_reply {
	ShoreStatus 		status;
	tid_t	result;
};

struct Vote_reply {
	ShoreStatus 			status;
	Vote			result;
};

struct statfs1_reply {
	ShoreStatus	status;
	lvid_t	volume;
	FSDATA  fsdata;
};

struct v_quota_reply {
	ShoreStatus	status;
	smksize_t  kbquota;
	smksize_t  kbused;
};

struct diskusage_reply {
	ShoreStatus	status;
	struct  sm_du_stats_t stats;
};

struct device_id {
	c_ino_t ino;
	c_dev_t dev;
};

struct path_dev_pair {
      Path    path;
      device_id dev;
};

struct v_devices_reply {
	ShoreStatus	status;
	/* int		listlen; is in buf struct */
	struct path_dev_pair	buf<>;
	Cookie	cookie;
};

struct pmountinfo {
	Path   fname;
	lvid_t target;
	serial_t dirserial;
};
struct v_listmounts_reply {
	ShoreStatus	status;
	/* int		listlen; is in buf struct */
	struct  pmountinfo	buf<>;
	Cookie	cookie;
};

struct getvol_reply {
	ShoreStatus	status;
	int		nvols; /* # on volume */
	int		nentries; /* how many returned */
	lvid_t  buf<>;
};
struct getmnt_reply {
	ShoreStatus	status;
	int		nentries;
	FSDATA	buf<>;
	Cookie	cookie;
};
struct getdirentries1_reply {
	ShoreStatus	status;
	char	buf<>;
	int		numentries;
	Cookie	cookie;
};
struct nextelem_reply {
	ShoreStatus		status;
	Cookie		cookie;
	opaque_t	key;
	opaque_t	value;
	small_bool_t		more;
};

struct readcommonreply {
	small_bool_t		sent_small_obj_page;
	opaque_t	data;
	/* for sending object */
	int			obj_follows_bytes;
	ObjectSize	more; /* true if there's more to read */
	lrid_t		snapped;
};

struct readobj_reply {
	ShoreStatus		status;
	readcommonreply commonpart;
};

struct rpcSysProps_reply {
	ShoreStatus		status;
	readcommonreply commonpart;
	rpcSysProps	sysprops;
	int			sysp_size;
};

struct nextpoolscan1_reply {
	ShoreStatus		status;
	/* nextpoolscan1 doesn't read the object,
	 * nextpoolscan2 does 
     */
	readcommonreply commonpart;

	small_bool_t		eof;
	Cookie		cookie;
	/* 	if requested sysprops, either 
	 *  small-object-page will be sent
	 *  or sysprops will be filled in.
	 */
	rpcSysProps	sysprops;
	int			sysp_size;
};

typedef nextpoolscan1_reply nextpoolscan2_reply;

/*
 * stat1, nextpoolscan*, and readobj
 * all use this common objmsg reply structure so
 * that *one* function can handle the SHM
 * and such.
 *
 * Each rpc stub can have its own smaller
 * reply structure and extract only the necessary
 * necessary info from a common_objmsg_reply structure.
 * In that case, USE THE FIELD NAMES of the common
 * structure.
 */

union	repu switch (LgReadRequest tag) {
	case AnyReq:
		readobj_reply  _any;

	case ReadReq:
		readobj_reply _read;
		
	case Stat1Req:
		rpcSysProps_reply 	_stat1;

	case NextPoolScan1Req:
		nextpoolscan1_reply _scan1;

	case NextPoolScan2Req:
		nextpoolscan2_reply _scan2;
};

struct common_objmsg_reply {
	ShoreStatus		status;

	repu _u;
};

struct rmlink1_reply {
	ShoreStatus		status;
	lrid_t		obj;
	small_bool_t		must_remove;
};

struct find_reply {
	ShoreStatus		status;
	small_bool_t		found;
	opaque_t	value;
};
struct readsymlink_reply {
	ShoreStatus		status;
	opaque_t	contents;
};
struct nextindexscan_reply {
	ShoreStatus		status;
	Cookie		cookie;
	opaque_t	key;
	opaque_t	value;
	small_bool_t		eof;
};
typedef reuid	getreuid1_reply;
typedef regid	getregid1_reply;

/* batch_reply defined in ../include/vas_types.h */

struct batched_req_reply {
	ShoreStatus	status; /* for error such as malloc failed */
	int		count; /* # legit responses */
	batch_reply	list<>;  /* # requests is in here */
	/* some of the items in the list my be bogus, but
	* the first "count" are ok. 
	*/
};

enum stats_types { __u, __i, __f, __v, __l };
union	stat_values switch (stats_types tag) {
	case __l: 	long  			_l;
	case __v: 	unsigned long	_v;
	case __i: 	int  			_i;
	case __u:	unsigned int 	_u;
	case __f:	float			_f;
};
enum emptiness { isnull, nonnull };
union possibly_null_string switch (emptiness tag) {
case isnull: 	void;
case nonnull:	string_t str;
};
struct stats_module {
	possibly_null_string descr;
	possibly_null_string types;
	unsigned int base;
	int			count;
	int			longest;
	string_t 	msgs<>; 
	stat_values values<>;
};
struct gather_stats_reply {
	ShoreStatus	status;
	unsigned int signature;
	stats_module modules<>;
};



/* can't use rpcgen's union because we don't have
 * constants for the cases, and, besides, we don't
 * want the tag.
 */
#ifdef RPC_HDR
%typedef union	client_replybuf{
%	int_reply		_int_reply;
%	void_reply		_void_reply;
%	voidref_reply	_voidref_reply;
%	lvid_t_reply	    _lvid_t_reply;
%	short_reply		_short_reply;
%	init_reply		_init_reply;
%	u_int_reply		_u_int_reply;
%	u_short_reply	_u_short_reply;
%	bool_reply		_bool_reply;
%	char_reply		_char_reply;
%	u_char_reply	_u_char_reply;
%	find_reply		_find_reply;
%	lrid_t_reply		_lrid_t_reply;
%	mkanon_reply		_mkanon_reply;
%	POID_reply		_POID_reply;
%	Path_reply		_Path_reply;
%	Cookie_reply	_Cookie_reply;
%	c_mode_t_reply	_mode_t_reply;
%	c_uid_t_reply		_uid_t_reply;
%	c_gid_t_reply		_gid_t_reply;
%	IndexId_reply		_IndexId_reply;
%	tid_t_reply	_tid_t_reply;
%	Vote_reply		_Vote_reply;
%	rpcSysProps_reply	_rpcSysProps_reply;
%	getdirentries1_reply	_getdirentries1_reply;
%	getmnt_reply	_getmnt_reply;
%	nextelem_reply	_nextelem_reply;
%	readobj_reply	_readobj_reply;
%	nextindexscan_reply	_nextindexscan_reply;
%	nextpoolscan1_reply	_nextpoolscan1_reply;
%	nextpoolscan2_reply	_nextpoolscan2_reply;
%	readsymlink_reply	_readsymlink_reply;
%	rmlink1_reply	_rmlink1_reply;
%	getregid1_reply	_getregid1_reply;
%	getreuid1_reply	_getreuid1_reply;
%	statfs1_reply	_statfs1_reply;
%	diskusage_reply		_diskusage_reply;
%	statindex_reply	_statindex_reply;
%	getvol_reply	_getvol_reply;
%	lookup_reply	_lookup_reply;
%	batched_req_reply _batched_req_reply;
%	gather_stats_reply 	_gather_stats_reply;
%	v_devices_reply 	_v_devices_reply;
%	v_listmounts_reply 	_v_listmounts_reply;
%	int_reply 	_locktimeout_reply;
%	v_quota_reply 	_v_quota_reply;
%}client_replybuf ; 
#endif
