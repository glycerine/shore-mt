#ifndef SM_STATS_T_STRUCT_GEN_H
#define SM_STATS_T_STRUCT_GEN_H

/* DO NOT EDIT --- GENERATED from sm_stats.dat by stats.pl
           on Mon Jan  2 14:58:12 2012

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


 w_base_t::base_stat_t bf_one_page_write;
 w_base_t::base_stat_t bf_two_page_write;
 w_base_t::base_stat_t bf_three_page_write;
 w_base_t::base_stat_t bf_four_page_write;
 w_base_t::base_stat_t bf_five_page_write;
 w_base_t::base_stat_t bf_six_page_write;
 w_base_t::base_stat_t bf_seven_page_write;
 w_base_t::base_stat_t bf_eight_page_write;
 w_base_t::base_stat_t bf_more_page_write;
 w_base_t::base_stat_t bf_cleaner_sweeps;
 w_base_t::base_stat_t bf_cleaner_signalled;
 w_base_t::base_stat_t bf_evicted_while_cleaning;
 w_base_t::base_stat_t bf_already_evicted;
 w_base_t::base_stat_t bf_dirty_page_cleaned;
 w_base_t::base_stat_t bf_flushed_OHD_page;
 w_base_t::base_stat_t bf_kick_full;
 w_base_t::base_stat_t bf_kick_replacement;
 w_base_t::base_stat_t bf_kick_threshold;
 w_base_t::base_stat_t bf_sweep_page_hot_skipped;
 w_base_t::base_stat_t bf_discarded_hot;
 w_base_t::base_stat_t bf_log_flush_all;
 w_base_t::base_stat_t bf_log_flush_lsn;
 w_base_t::base_stat_t bf_write_out;
 w_base_t::base_stat_t bf_sleep_await_clean;
 w_base_t::base_stat_t bf_fg_scan_cnt;
 w_base_t::base_stat_t bf_unfix_cleaned;
 w_base_t::base_stat_t rwlock_r_waits;
 w_base_t::base_stat_t rwlock_w_waits;
 w_base_t::base_stat_t need_latch_condl;
 w_base_t::base_stat_t latch_condl_nowaits;
 w_base_t::base_stat_t need_latch_uncondl;
 w_base_t::base_stat_t latch_uncondl_nowaits;
 w_base_t::base_stat_t latch_uncondl_waits;
 w_base_t::base_stat_t btree_latch_wait;
 w_base_t::base_stat_t io_latch_wait;
 w_base_t::base_stat_t bf_look_cnt;
 w_base_t::base_stat_t bf_grab_latch_failed;
 w_base_t::base_stat_t bf_hit_cnt;
 w_base_t::base_stat_t bf_hit_wait;
 w_base_t::base_stat_t bf_hit_wait_any_p;
 w_base_t::base_stat_t bf_hit_wait_btree_p;
 w_base_t::base_stat_t bf_hit_wait_file_p;
 w_base_t::base_stat_t bf_hit_wait_keyed_p;
 w_base_t::base_stat_t bf_hit_wait_lgdata_p;
 w_base_t::base_stat_t bf_hit_wait_lgindex_p;
 w_base_t::base_stat_t bf_hit_wait_rtree_p;
 w_base_t::base_stat_t bf_hit_wait_rtree_base_p;
 w_base_t::base_stat_t bf_hit_wait_extlink_p;
 w_base_t::base_stat_t bf_hit_wait_stnode_p;
 w_base_t::base_stat_t bf_hit_wait_zkeyed_p;
 w_base_t::base_stat_t bf_hit_wait_scan;
 w_base_t::base_stat_t bf_replace_out;
 w_base_t::base_stat_t bf_replaced_dirty;
 w_base_t::base_stat_t bf_replaced_clean;
 w_base_t::base_stat_t bf_replaced_unused;
 w_base_t::base_stat_t bf_awaited_cleaner;
 w_base_t::base_stat_t bf_no_transit_bucket;
 w_base_t::base_stat_t bf_prefetch_requests;
 w_base_t::base_stat_t bf_prefetches;
 w_base_t::base_stat_t bf_upgrade_latch_unconditional;
 w_base_t::base_stat_t bf_upgrade_latch_race;
 w_base_t::base_stat_t bf_upgrade_latch_changed;
 w_base_t::base_stat_t restart_repair_rec_lsn;
 w_base_t::base_stat_t vol_reads;
 w_base_t::base_stat_t vol_writes;
 w_base_t::base_stat_t vol_blks_written;
 w_base_t::base_stat_t vol_alloc_exts;
 w_base_t::base_stat_t vol_free_exts;
 w_base_t::base_stat_t need_vol_lock_r;
 w_base_t::base_stat_t need_vol_lock_w;
 w_base_t::base_stat_t nowait_vol_lock_r;
 w_base_t::base_stat_t nowait_vol_lock_w;
 w_base_t::base_stat_t await_vol_lock_r;
 w_base_t::base_stat_t await_vol_lock_w;
 w_base_t::base_stat_t io_m_lsearch;
 w_base_t::base_stat_t io_m_lsearch_extents;
 w_base_t::base_stat_t vol_cache_primes;
 w_base_t::base_stat_t vol_cache_prime_fix;
 w_base_t::base_stat_t vol_cache_clears;
 w_base_t::base_stat_t vol_last_extent_search;
 w_base_t::base_stat_t vol_last_page_cache_update;
 w_base_t::base_stat_t vol_last_page_cache_invalidate;
 w_base_t::base_stat_t vol_last_page_cache_find;
 w_base_t::base_stat_t vol_last_page_cache_hit;
 w_base_t::base_stat_t vol_resv_cache_insert;
 w_base_t::base_stat_t vol_resv_cache_erase;
 w_base_t::base_stat_t vol_resv_cache_hit;
 w_base_t::base_stat_t vol_resv_cache_fail;
 w_base_t::base_stat_t vol_lock_noalloc;
 w_base_t::base_stat_t log_dup_sync_cnt;
 w_base_t::base_stat_t log_daemon_wait;
 w_base_t::base_stat_t log_daemon_work;
 w_base_t::base_stat_t log_fsync_cnt;
 w_base_t::base_stat_t log_chkpt_cnt;
 w_base_t::base_stat_t log_chkpt_wake;
 w_base_t::base_stat_t log_fetches;
 w_base_t::base_stat_t log_inserts;
 w_base_t::base_stat_t log_full;
 w_base_t::base_stat_t log_full_old_xct;
 w_base_t::base_stat_t log_full_old_page;
 w_base_t::base_stat_t log_full_wait;
 w_base_t::base_stat_t log_full_force;
 w_base_t::base_stat_t log_full_giveup;
 w_base_t::base_stat_t log_file_wrap;
 w_base_t::base_stat_t log_bytes_generated;
 w_base_t::base_stat_t log_bytes_written;
 w_base_t::base_stat_t log_bytes_rewritten;
 w_base_t::base_stat_t log_bytes_generated_rb;
 w_base_t::base_float_t log_bytes_rbfwd_ratio;
 w_base_t::base_stat_t log_flush_wait;
 w_base_t::base_stat_t log_short_flush;
 w_base_t::base_stat_t log_long_flush;
 w_base_t::base_stat_t lock_deadlock_cnt;
 w_base_t::base_stat_t lock_false_deadlock_cnt;
 w_base_t::base_stat_t lock_dld_upd_waitmap_cnt;
 w_base_t::base_stat_t lock_dld_call_cnt;
 w_base_t::base_stat_t lock_dld_first_call_cnt;
 w_base_t::base_stat_t lock_dld_false_victim_cnt;
 w_base_t::base_stat_t lock_dld_victim_self_cnt;
 w_base_t::base_stat_t lock_dld_victim_other_cnt;
 w_base_t::base_stat_t lock_dld_retry;
 w_base_t::base_stat_t lock_dld_deadlock;
 w_base_t::base_stat_t lock_dld_timeout;
 w_base_t::base_stat_t nonunique_fingerprints;
 w_base_t::base_stat_t unique_fingerprints;
 w_base_t::base_stat_t rec_pin_cnt;
 w_base_t::base_stat_t rec_unpin_cnt;
 w_base_t::base_stat_t rec_repin_cvt;
 w_base_t::base_stat_t fm_pagecache_hit;
 w_base_t::base_stat_t fm_page_nolatch;
 w_base_t::base_stat_t fm_page_moved;
 w_base_t::base_stat_t fm_page_invalid;
 w_base_t::base_stat_t fm_page_nolock;
 w_base_t::base_stat_t fm_alloc_page_reject;
 w_base_t::base_stat_t fm_page_full;
 w_base_t::base_stat_t fm_error_not_handled;
 w_base_t::base_stat_t fm_ok;
 w_base_t::base_stat_t fm_histogram_hit;
 w_base_t::base_stat_t fm_search_pages;
 w_base_t::base_stat_t fm_bogus_pbucketmap;
 w_base_t::base_stat_t fm_search_failed;
 w_base_t::base_stat_t fm_search_hit;
 w_base_t::base_stat_t fm_lastpid_cached;
 w_base_t::base_stat_t fm_lastpid_hit;
 w_base_t::base_stat_t fm_alloc_pg;
 w_base_t::base_stat_t fm_ext_touch;
 w_base_t::base_stat_t fm_ext_touch_nop;
 w_base_t::base_stat_t fm_nospace;
 w_base_t::base_stat_t fm_cache;
 w_base_t::base_stat_t fm_compact;
 w_base_t::base_stat_t fm_append;
 w_base_t::base_stat_t fm_appendonly;
 w_base_t::base_stat_t bt_find_cnt;
 w_base_t::base_stat_t bt_insert_cnt;
 w_base_t::base_stat_t bt_remove_cnt;
 w_base_t::base_stat_t bt_traverse_cnt;
 w_base_t::base_stat_t bt_partial_traverse_cnt;
 w_base_t::base_stat_t bt_restart_traverse_cnt;
 w_base_t::base_stat_t bt_posc;
 w_base_t::base_stat_t bt_scan_cnt;
 w_base_t::base_stat_t bt_splits;
 w_base_t::base_stat_t bt_cuts;
 w_base_t::base_stat_t bt_grows;
 w_base_t::base_stat_t bt_shrinks;
 w_base_t::base_stat_t bt_links;
 w_base_t::base_stat_t bt_upgrade_fail_retry;
 w_base_t::base_stat_t bt_clr_smo_traverse;
 w_base_t::base_stat_t bt_pcompress;
 w_base_t::base_stat_t bt_plmax;
 w_base_t::base_stat_t sort_keycmp_cnt;
 w_base_t::base_stat_t sort_lexindx_cnt;
 w_base_t::base_stat_t sort_getinfo_cnt;
 w_base_t::base_stat_t sort_mof_cnt;
 w_base_t::base_stat_t sort_umof_cnt;
 w_base_t::base_stat_t sort_memcpy_cnt;
 w_base_t::base_stat_t sort_memcpy_bytes;
 w_base_t::base_stat_t sort_keycpy_cnt;
 w_base_t::base_stat_t sort_mallocs;
 w_base_t::base_stat_t sort_malloc_bytes;
 w_base_t::base_stat_t sort_malloc_hiwat;
 w_base_t::base_stat_t sort_malloc_max;
 w_base_t::base_stat_t sort_malloc_curr;
 w_base_t::base_stat_t sort_tmpfile_cnt;
 w_base_t::base_stat_t sort_tmpfile_bytes;
 w_base_t::base_stat_t sort_duplicates;
 w_base_t::base_stat_t sort_page_fixes;
 w_base_t::base_stat_t sort_page_fixes_2;
 w_base_t::base_stat_t sort_lg_page_fixes;
 w_base_t::base_stat_t sort_rec_pins;
 w_base_t::base_stat_t sort_files_created;
 w_base_t::base_stat_t sort_recs_created;
 w_base_t::base_stat_t sort_rec_bytes;
 w_base_t::base_stat_t sort_runs;
 w_base_t::base_stat_t sort_run_size;
 w_base_t::base_stat_t sort_phases;
 w_base_t::base_stat_t sort_ntapes;
 w_base_t::base_stat_t any_p_fix_cnt;
 w_base_t::base_stat_t btree_p_fix_cnt;
 w_base_t::base_stat_t file_p_fix_cnt;
 w_base_t::base_stat_t keyed_p_fix_cnt;
 w_base_t::base_stat_t lgdata_p_fix_cnt;
 w_base_t::base_stat_t lgindex_p_fix_cnt;
 w_base_t::base_stat_t rtree_p_fix_cnt;
 w_base_t::base_stat_t rtree_base_p_fix_cnt;
 w_base_t::base_stat_t extlink_p_fix_cnt;
 w_base_t::base_stat_t stnode_p_fix_cnt;
 w_base_t::base_stat_t zkeyed_p_fix_cnt;
 w_base_t::base_stat_t page_fix_cnt;
 w_base_t::base_stat_t bf_fix_cnt;
 w_base_t::base_stat_t bf_refix_cnt;
 w_base_t::base_stat_t bf_unfix_cnt;
 w_base_t::base_stat_t vol_check_owner_fix;
 w_base_t::base_stat_t page_alloc_cnt;
 w_base_t::base_stat_t page_dealloc_cnt;
 w_base_t::base_stat_t ext_lookup_hits;
 w_base_t::base_stat_t ext_lookup_misses;
 w_base_t::base_stat_t alloc_page_in_ext;
 w_base_t::base_stat_t vol_free_page;
 w_base_t::base_stat_t vol_next_page;
 w_base_t::base_stat_t vol_next_page_with_space;
 w_base_t::base_stat_t vol_find_free_exts;
 w_base_t::base_stat_t xct_log_flush;
 w_base_t::base_stat_t begin_xct_cnt;
 w_base_t::base_stat_t commit_xct_cnt;
 w_base_t::base_stat_t abort_xct_cnt;
 w_base_t::base_stat_t log_warn_abort_cnt;
 w_base_t::base_stat_t prepare_xct_cnt;
 w_base_t::base_stat_t rollback_savept_cnt;
 w_base_t::base_stat_t internal_rollback_cnt;
 w_base_t::base_stat_t s_prepared;
 w_base_t::base_stat_t sdesc_cache_hit;
 w_base_t::base_stat_t sdesc_cache_search;
 w_base_t::base_stat_t sdesc_cache_search_cnt;
 w_base_t::base_stat_t sdesc_cache_miss;
 w_base_t::base_stat_t mpl_attach_cnt;
 w_base_t::base_stat_t anchors;
 w_base_t::base_stat_t compensate_in_log;
 w_base_t::base_stat_t compensate_in_xct;
 w_base_t::base_stat_t compensate_records;
 w_base_t::base_stat_t compensate_skipped;
 w_base_t::base_stat_t log_switches;
 w_base_t::base_stat_t get_logbuf;
 w_base_t::base_stat_t await_1thread_xct;
 w_base_t::base_stat_t lock_query_cnt;
 w_base_t::base_stat_t unlock_request_cnt;
 w_base_t::base_stat_t lock_request_cnt;
 w_base_t::base_stat_t lock_acquire_cnt;
 w_base_t::base_stat_t lock_head_t_cnt;
 w_base_t::base_stat_t lock_await_alt_cnt;
 w_base_t::base_stat_t lock_extraneous_req_cnt;
 w_base_t::base_stat_t lock_conversion_cnt;
 w_base_t::base_stat_t lock_cache_hit_cnt;
 w_base_t::base_stat_t lock_request_t_cnt;
 w_base_t::base_stat_t lock_esc_to_page;
 w_base_t::base_stat_t lock_esc_to_store;
 w_base_t::base_stat_t lock_esc_to_volume;
 w_base_t::base_stat_t lk_vol_acq;
 w_base_t::base_stat_t lk_store_acq;
 w_base_t::base_stat_t lk_page_acq;
 w_base_t::base_stat_t lk_kvl_acq;
 w_base_t::base_stat_t lk_rec_acq;
 w_base_t::base_stat_t lk_ext_acq;
 w_base_t::base_stat_t lk_user1_acq;
 w_base_t::base_stat_t lk_user2_acq;
 w_base_t::base_stat_t lk_user3_acq;
 w_base_t::base_stat_t lk_user4_acq;
 w_base_t::base_stat_t lock_wait_cnt;
 w_base_t::base_stat_t lock_block_cnt;
 w_base_t::base_stat_t lk_vol_wait;
 w_base_t::base_stat_t lk_store_wait;
 w_base_t::base_stat_t lk_page_wait;
 w_base_t::base_stat_t lk_kvl_wait;
 w_base_t::base_stat_t lk_rec_wait;
 w_base_t::base_stat_t lk_ext_wait;
 w_base_t::base_stat_t lk_user1_wait;
 w_base_t::base_stat_t lk_user2_wait;
 w_base_t::base_stat_t lk_user3_wait;
 w_base_t::base_stat_t lk_user4_wait;
public: 
friend ostream &
    operator<<(ostream &o,const sm_stats_t &t);
public: 
friend sm_stats_t &
    operator+=(sm_stats_t &s,const sm_stats_t &t);
public: 
friend sm_stats_t &
    operator-=(sm_stats_t &s,const sm_stats_t &t);
static const char    *stat_names[];
static const char    *stat_types;
#define W_sm_stats_t  30 + 2

#endif /* SM_STATS_T_STRUCT_GEN_H */
