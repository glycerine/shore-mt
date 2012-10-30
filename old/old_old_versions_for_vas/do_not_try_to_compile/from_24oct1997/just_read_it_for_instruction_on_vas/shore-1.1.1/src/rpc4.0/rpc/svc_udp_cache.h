
/*
 * An entry in the cache
 */
typedef struct cache_node *cache_ptr;
struct cache_node {
	/*
	 * Index into cache is xid, proc, vers, prog and address
	 */
	u_long cache_xid;
	u_long cache_proc;
	u_long cache_vers;
	u_long cache_prog;

	u_long found_count; /* count of # times this entry was found
		// for the purpose of incrementing stats for #s of retransmissions
		*/ 
	struct sockaddr_in cache_addr;
	/*
	 * The cached reply and length
	 */
	char * cache_reply;
	u_long cache_replylen;

#define CS_INPROGRESS 0 /* a thread is working on it-- drop the retrans */
	/* NB: CS_INPROGRESS isn't implemented yet */
#define CS_FIRSTREPLY 1
#define CS_REXREPLY   2  /* a reply to a retrans was sent */
#define CS_NEW      3  

	int    cache_status; /* status of rpc request */
		/* if > 0 it indicates the number of replies sent */
	/*
 	 * Next node on the list, if there is a collision
	 */
	cache_ptr cache_next;	
};
static cache_ptr 	cache_find();

/* return values for cache_get: */
#define			DROPIT 	1
#define			SENDREPLY 	2
#define			DOIT 	0

static int 			cache_get();
static void 		cache_set();
static int 			cache_preset();
cache_ptr  			cache_victim();


/*
 * The entire cache
 */
struct udp_cache {
	u_long uc_size;		/* size of cache */
	cache_ptr *uc_entries;	/* hash table of entries in cache */
	cache_ptr *uc_fifo;	/* fifo list of entries in cache */
	u_long uc_nextvictim;	/* points to next victim in fifo list */
	u_long uc_prog;		/* saved program number */
	u_long uc_vers;		/* saved version number */
	u_long uc_proc;		/* saved procedure number */
	struct sockaddr_in uc_addr; /* saved caller's address */
};
