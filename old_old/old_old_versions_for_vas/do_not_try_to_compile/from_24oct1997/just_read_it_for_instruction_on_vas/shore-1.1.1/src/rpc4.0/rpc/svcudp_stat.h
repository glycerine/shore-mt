
#ifndef __SVCUDP_STAT_H__
#define __SVCUDP_STAT_H__
struct svcudp_stat {
	int cache_finds; /* cache_get + cache_set */
	int cache_nofinds;
	int cache_hits;	/* for cache_get only */
	int cache_misses;	/* for cache_get only */
	int cache_sets;
	int cache_presets;
	int cache_gets;
	int drops;
	int done;
	int replies;
	int rereplies;
	int recvfroms;

	/*
	// all these can be flaky because we could
	// receive a retrans after the cache was
	// flushed, if we have lots of activity
	*/
	int retrans;  /* different calls having retransmissions */
	int retrans_total; /* total # pkts that are retransmissions */
	int retrans_max; /* max # retrans detected for any given request */
};
EXTERNC struct svcudp_stat *svcudp_stats();
EXTERNC void  		svcudp_clearstats();


#endif /*__SVCUDP_STAT_H__*/
