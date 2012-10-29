#ifndef BF_HTAB_STATS_T_MSG_GEN_H
#define BF_HTAB_STATS_T_MSG_GEN_H

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
/*  -- do not edit anything above this line --   </std-header>*/


/* BFHT_bf_htab_insertions */ "Hash table insertions",
/* BFHT_bf_htab_slow_inserts */ "Hash table inserts requiring multiple moves ",
/* BFHT_bf_htab_slots_tried */ "Hash table slots tried for insertions",
/* BFHT_bf_htab_ensures    */ "Calls to ensure_space to make room for insert",
/* BFHT_bf_htab_cuckolds   */ "Items moved to make room for insert",
/* BFHT_bf_htab_lookups    */ "Hash table lookups",
/* BFHT_bf_htab_harsh_lookups */ "Hash table lookups locking all hash-target buckets",
/* BFHT_bf_htab_lookups_failed */ "Hash table lookups did not find page",
/* BFHT_bf_htab_probes     */ "Hash table probes in lookup",
/* BFHT_bf_htab_harsh_probes */ "Hash table probes in lookup_harsh",
/* BFHT_bf_htab_probe_empty */ "Hash table probes of empty slots in lookup or lookup_harsh",
/* BFHT_bf_htab_hash_collisions */ "Hash function collisions",
/* BFHT_bf_htab_removes    */ "Hash table removes",
/* BFHT_bf_htab_limit_exceeds */ "Insert failed due to exceeding compile-time depth limit",
/* BFHT_bf_htab_max_limit  */ "Maximum depth of ensure_space calls on insert ",
/* BFHT_bf_htab_insert_avg_tries */ "Hash table avg tries per insertion",
/* BFHT_bf_htab_lookup_avg_probes */ "Hash table avg probes per lookup",
/* BFHT_bf_htab_bucket_size */ "Hash table bucket size in bytes",
/* BFHT_bf_htab_table_size */ "Hash table size in bytes",
/* BFHT_bf_htab_entries    */ "Hash table number of entries (bpool buffers)",
/* BFHT_bf_htab_buckets    */ "Hash table number of buckets (indexes)",
/* BFHT_bf_htab_slot_count */ "Hash table number of slots per bucket  ",

#endif /* BF_HTAB_STATS_T_MSG_GEN_H */
