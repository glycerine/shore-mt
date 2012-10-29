#ifndef BF_HTAB_STATS_T_STRUCT_GEN_H
#define BF_HTAB_STATS_T_STRUCT_GEN_H

/* DO NOT EDIT --- GENERATED from bf_htab_stats.dat by stats.pl
           on Fri Dec 17 15:55:05 2010

<std-header orig-src='shore' genfile='true'>

SHORE -- Scalable Heterogeneous Object REpository

Copyright (c) 1994-99 Computer Sciences Department, University of
                      Wisconsin -- Madison
All Rights Reserved.

Permission to use, copy, modify and distribute this software and its
documentation is hereby granted, provided that both the copyright
notice and this permission notice appear in all copies of the
software, derivative works or modified versions, and any portions
thereof, and that both notices appear in supporting documentation.

THE AUTHORS AND THE COMPUTER SCIENCES DEPARTMENT OF THE UNIVERSITY
OF WISCONSIN - MADISON ALLOW FREE USE OF THIS SOFTWARE IN ITS
"AS IS" CONDITION, AND THEY DISCLAIM ANY LIABILITY OF ANY KIND
FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.

This software was developed with support by the Advanced Research
Project Agency, ARPA order number 018 (formerly 8230), monitored by
the U.S. Army Research Laboratory under contract DAAB07-91-C-Q518.
Further funding for this work was provided by DARPA through
Rome Research Laboratory Contract No. F30602-97-2-0247.

*/

#include "w_defines.h"
/*  -- do not edit anything above this line --   </std-header>*/


 w_base_t::base_stat_t bf_htab_insertions;
 w_base_t::base_stat_t bf_htab_slow_inserts;
 w_base_t::base_stat_t bf_htab_slots_tried;
 w_base_t::base_stat_t bf_htab_ensures;
 w_base_t::base_stat_t bf_htab_cuckolds;
 w_base_t::base_stat_t bf_htab_lookups;
 w_base_t::base_stat_t bf_htab_harsh_lookups;
 w_base_t::base_stat_t bf_htab_lookups_failed;
 w_base_t::base_stat_t bf_htab_probes;
 w_base_t::base_stat_t bf_htab_harsh_probes;
 w_base_t::base_stat_t bf_htab_probe_empty;
 w_base_t::base_stat_t bf_htab_hash_collisions;
 w_base_t::base_stat_t bf_htab_removes;
 w_base_t::base_stat_t bf_htab_limit_exceeds;
 w_base_t::base_stat_t bf_htab_max_limit;
 w_base_t::base_float_t bf_htab_insert_avg_tries;
 w_base_t::base_float_t bf_htab_lookup_avg_probes;
 w_base_t::base_stat_t bf_htab_bucket_size;
 w_base_t::base_stat_t bf_htab_table_size;
 w_base_t::base_stat_t bf_htab_entries;
 w_base_t::base_stat_t bf_htab_buckets;
 w_base_t::base_stat_t bf_htab_slot_count;
public: 
friend ostream &
    operator<<(ostream &o,const bf_htab_stats_t &t);
public: 
friend bf_htab_stats_t &
    operator+=(bf_htab_stats_t &s,const bf_htab_stats_t &t);
public: 
friend bf_htab_stats_t &
    operator-=(bf_htab_stats_t &s,const bf_htab_stats_t &t);
static const char    *stat_names[];
static const char    *stat_types;
#define W_bf_htab_stats_t  25 + 2

#endif /* BF_HTAB_STATS_T_STRUCT_GEN_H */
