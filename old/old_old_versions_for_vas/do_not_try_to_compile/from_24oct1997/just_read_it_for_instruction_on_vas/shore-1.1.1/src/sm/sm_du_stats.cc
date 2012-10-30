/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm_du_stats.cc,v 1.16 1997/06/15 03:13:59 solomon Exp $
 */
#define SM_DU_STATS_C

#ifdef __GNUG__
#pragma implementation
#endif

#include <memory.h>
#include <stream.h>
#include "w_base.h"
#include "w_list.h"
#include "w_minmax.h"
#include "basics.h"
#include "lid_t.h"
#include "sm_s.h"
#include "sm_base.h"
#include "sm_du_stats.h"
#include <debug.h>


// This function is a convenient debugging breakpoint for
// detecting audit failures.
static void
stats_audit_failed()
{
    DBG( << "stats audit failed");
}

void
file_pg_stats_t::clear()
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(base_stat_t); i++) {
	((base_stat_t*)this)[i] = 0;
    }
}

void
file_pg_stats_t::add(const file_pg_stats_t& stats)
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(hdr_bs); i++) {
	((base_stat_t*)this)[i] += ((base_stat_t*)&stats)[i];
    }
}

w_rc_t
file_pg_stats_t::audit() const
{
    FUNC(file_pg_stats_t::audit);
    if (total_bytes() % smlevel_0::page_sz != 0) {
	DBG(
	    << " file_pg_stats_t::total_bytes= " << total_bytes()
	    << " smlevel_0::page_sz= " << smlevel_0::page_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    if (lg_rec_cnt) {
	if (rec_lg_chunk_bs + rec_lg_indirect_bs == 0) {
	    DBG(
		<< " lg_rec_cnt= " << lg_rec_cnt
		<< " rec_lg_chunk_bs= " << rec_lg_chunk_bs
		<< " rec_lg_indirect_bs= " << rec_lg_indirect_bs
	    );
	    stats_audit_failed();
	    return RC(fcINTERNAL);
	}
    }
    DBG(<<"file_pg_stats_t audit ok");
    return RCOK;
}

base_stat_t
file_pg_stats_t::total_bytes() const
{
    base_stat_t  total = 0;
    total += hdr_bs;
    total += slots_used_bs;
    total += slots_unused_bs;
    total += rec_tag_bs;
    total += rec_hdr_bs;
    total += rec_hdr_align_bs;
    total += rec_body_bs;
    total += rec_body_align_bs;
    total += rec_lg_chunk_bs;
    total += rec_lg_indirect_bs;
    total += free_bs;
    return total;
}


ostream& operator<<(ostream& o, const file_pg_stats_t& s)
{
    /*
    return o
    << "hdr_bs "		<< s.hdr_bs << endl
    << "slots_used_bs "		<< s.slots_used_bs << endl
    << "slots_unused_bs "	<< s.slots_unused_bs << endl
    << "rec_tag_bs "		<< s.rec_tag_bs << endl
    << "rec_hdr_bs "		<< s.rec_hdr_bs << endl
    << "rec_hdr_align_bs "	<< s.rec_hdr_align_bs << endl
    << "rec_body_bs "		<< s.rec_body_bs << endl
    << "rec_body_align_bs "	<< s.rec_body_align_bs << endl

    << "rec_lg_chunk_bs "	<< s.rec_lg_chunk_bs << endl
    << "rec_lg_indirect_bs "	<< s.rec_lg_indirect_bs << endl
    << "free_bs "		<< s.free_bs << endl

    << "small_rec_cnt "		<< s.small_rec_cnt << endl
    << "lg_rec_cnt "		<< s.lg_rec_cnt << endl
    ;
    */
    s.print(o, "");
    return o;
}

void file_pg_stats_t::print(ostream& o, const char *pfx) const
{
    const file_pg_stats_t &s = *this;
    o
    << pfx << "hdr_bs "			<< s.hdr_bs << endl
    << pfx << "slots_used_bs "		<< s.slots_used_bs << endl
    << pfx << "slots_unused_bs "	<< s.slots_unused_bs << endl
    << pfx << "rec_tag_bs "		<< s.rec_tag_bs << endl
    << pfx << "rec_hdr_bs "		<< s.rec_hdr_bs << endl
    << pfx << "rec_hdr_align_bs "	<< s.rec_hdr_align_bs << endl
    << pfx << "rec_body_bs "		<< s.rec_body_bs << endl
    << pfx << "rec_body_align_bs "	<< s.rec_body_align_bs << endl

    << pfx << "rec_lg_chunk_bs "	<< s.rec_lg_chunk_bs << endl
    << pfx << "rec_lg_indirect_bs "	<< s.rec_lg_indirect_bs << endl
    << pfx << "free_bs "		<< s.free_bs << endl

    << pfx << "small_rec_cnt "		<< s.small_rec_cnt << endl
    << pfx << "lg_rec_cnt "		<< s.lg_rec_cnt << endl
    ;
}




void
lgdata_pg_stats_t::clear()
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(hdr_bs); i++) {
	((base_stat_t*)this)[i] = 0;
    }
}

void
lgdata_pg_stats_t::add(const lgdata_pg_stats_t& stats)
{
    w_assert3(sizeof(*this) % sizeof(hdr_bs) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(hdr_bs); i++) {
	((base_stat_t*)this)[i] += ((base_stat_t*)&stats)[i];
    }
}

w_rc_t
lgdata_pg_stats_t::audit() const
{
    FUNC(lgdata_pg_stats_t::audit);
    if (total_bytes() % smlevel_0::page_sz != 0)  {
	DBG(
	    << " lgdata_pg_stats_t::total_bytes= " << total_bytes()
	    << " smlevel_0::page_sz= " << smlevel_0::page_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    DBG(<<"lgdata_pg_stats_t audit ok");
    return RCOK;
}

base_stat_t
lgdata_pg_stats_t::total_bytes() const
{
    return hdr_bs + data_bs + unused_bs;
}


ostream& operator<<(ostream& o, const lgdata_pg_stats_t& s)
{
    /*
    return o
    << "hdr_bs "		<< s.hdr_bs << endl
    << "data_bs "		<< s.data_bs << endl
    << "unused_bs "		<< s.unused_bs << endl
    ;
    */
    s.print(o, "");
    return o;
}

void lgdata_pg_stats_t::print(ostream& o, const char *pfx) const
{
    const lgdata_pg_stats_t &s = *this;
    o
    << pfx << "hdr_bs "		<< s.hdr_bs << endl
    << pfx << "data_bs "		<< s.data_bs << endl
    << pfx << "unused_bs "		<< s.unused_bs << endl
    ;
}

void
lgindex_pg_stats_t::clear()
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(base_stat_t); i++) {
	((base_stat_t*)this)[i] = 0;
    }
}

void
lgindex_pg_stats_t::add(const lgindex_pg_stats_t& stats)
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(used_bs); i++) {
	((base_stat_t*)this)[i] += ((base_stat_t*)&stats)[i];
    }
}

w_rc_t
lgindex_pg_stats_t::audit() const
{
	FUNC(lgindex_pg_stats_t::audit);
    if (total_bytes() % smlevel_0::page_sz != 0) {
	DBG(
	    << " lgindex_pg_stats_t::total_bytes= " << total_bytes()
	    << " smlevel_0::page_sz= " << smlevel_0::page_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    DBG(<<"lgindex_pg_stats_t audit ok");
    return RCOK;
}

base_stat_t
lgindex_pg_stats_t::total_bytes() const
{
    return used_bs + unused_bs;
}

ostream& operator<<(ostream& o, const lgindex_pg_stats_t& s)
{
    /*
    return o
    << "used_bs "		<< s.used_bs << endl
    << "unused_bs "		<< s.unused_bs << endl
    ;
    */
    s.print(o, "");
    return o;
}

void lgindex_pg_stats_t::print(ostream& o, const char *pfx) const
{
    const lgindex_pg_stats_t &s = *this;
    o 
    << pfx << "used_bs "		<< s.used_bs << endl
    << pfx << "unused_bs "		<< s.unused_bs << endl
    ;
}


void
file_stats_t::clear()
{
    file_pg.clear();
    lgdata_pg.clear();
    lgindex_pg.clear();

    file_pg_cnt = 0;
    lgdata_pg_cnt = 0;
    lgindex_pg_cnt = 0;
    unalloc_file_pg_cnt = 0;
    unalloc_large_pg_cnt = 0;
}

void
file_stats_t::add(const file_stats_t& stats)
{
    file_pg.add(stats.file_pg);
    lgdata_pg.add(stats.lgdata_pg);
    lgindex_pg.add(stats.lgindex_pg);

    file_pg_cnt += stats.file_pg_cnt;
    lgdata_pg_cnt += stats.lgdata_pg_cnt;
    lgindex_pg_cnt += stats.lgindex_pg_cnt;
    unalloc_file_pg_cnt += stats.unalloc_file_pg_cnt;
    unalloc_large_pg_cnt += stats.unalloc_large_pg_cnt;
}

w_rc_t
file_stats_t::audit() const
{
    FUNC(file_stats_t::audit);

    W_DO(file_pg.audit());
    W_DO(lgdata_pg.audit());
    W_DO(lgindex_pg.audit());

    base_stat_t total_alloc_pgs = alloc_pg_cnt();

    if (total_alloc_pgs*smlevel_0::page_sz != total_bytes()) {
	DBG(
	    << " total_alloc_pgs= " << total_alloc_pgs
	    << " file_stats_t::total_bytes= " << total_bytes()
	    << " smlevel_0::page_sz= " << smlevel_0::page_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    if ( (total_alloc_pgs + unalloc_file_pg_cnt + unalloc_large_pg_cnt) % smlevel_0::ext_sz != 0) {
	DBG(
	    << " total_alloc_pgs= " << total_alloc_pgs
	    << " unalloc_file_pg_cnt= " << unalloc_file_pg_cnt
	    << " unalloc_large_pg_cnt= " << unalloc_large_pg_cnt
	    << " smlevel_0::ext_sz= " << smlevel_0::ext_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    DBG(<<"file_stats_t audit ok");
    return RCOK;
}

base_stat_t
file_stats_t::total_bytes() const
{
    return file_pg.total_bytes() + lgdata_pg.total_bytes() +
			   lgindex_pg.total_bytes();
}

base_stat_t
file_stats_t::alloc_pg_cnt() const
{
    return file_pg_cnt + lgdata_pg_cnt + lgindex_pg_cnt;
}


ostream& operator<<(ostream& o, const file_stats_t& s)
{
    /*
    return 
    o << s.file_pg << s.lgdata_pg << s.lgindex_pg
    << "file_pg_cnt "		<< s.file_pg_cnt << endl
    << "lgdata_pg_cnt "		<< s.lgdata_pg_cnt << endl
    << "lgindex_pg_cnt "	<< s.lgindex_pg_cnt << endl
    << "unalloc_file_pg_cnt "	<< s.unalloc_file_pg_cnt << endl
    << "unalloc_large_pg_cnt "	<< s.unalloc_large_pg_cnt << endl
    ;
    */
    s.print(o,"");
    return o;
}

void file_stats_t::print(ostream& o, const char *pfx) const
{
    const file_stats_t &s = *this;

    unsigned int pfxlen = strlen(pfx);
    char *pfx1 = new char[strlen(pfx) + 30];
    memcpy(pfx1, pfx, pfxlen);
	memcpy(pfx1+pfxlen, "fipg.", 6);
	s.file_pg.print(o,pfx1); 

	memcpy(pfx1+pfxlen, "lgpg.", 6);
	s.lgdata_pg.print(o,pfx1); 

	memcpy(pfx1+pfxlen, "lgix.", 6);
	s.lgindex_pg.print(o,pfx1); 
    delete[] pfx1;

    o
    << pfx<< "file_pg_cnt "		<< s.file_pg_cnt << endl
    << pfx<< "lgdata_pg_cnt "		<< s.lgdata_pg_cnt << endl
    << pfx<< "lgindex_pg_cnt "	<< s.lgindex_pg_cnt << endl
    << pfx<< "unalloc_file_pg_cnt "	<< s.unalloc_file_pg_cnt << endl
    << pfx<< "unalloc_large_pg_cnt "	<< s.unalloc_large_pg_cnt << endl
    ;
}



void
btree_lf_stats_t::clear()
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(base_stat_t); i++) {
	((base_stat_t*)this)[i] = 0;
    }
}

void
btree_lf_stats_t::add(const btree_lf_stats_t& stats)
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(hdr_bs); i++) {
	((base_stat_t*)this)[i] += ((base_stat_t*)&stats)[i];
    }
}

w_rc_t
btree_lf_stats_t::audit() const
{
    FUNC(btree_lf_stats_t::audit);
    if (total_bytes() % smlevel_0::page_sz != 0) {
	DBG(
	    << " btree_lf_stats_t::total_bytes= " << total_bytes()
	    << " smlevel_0::page_sz= " << smlevel_0::page_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    DBG(<<"btree_lf_stats_t audit ok");
    return RCOK;
}

base_stat_t
btree_lf_stats_t::total_bytes() const
{
    return hdr_bs + key_bs + data_bs + entry_overhead_bs + unused_bs;
}


ostream& operator<<(ostream& o, const btree_lf_stats_t& s)
{
    /*
    return 
    o
    << "hdr_bs "		<< s.hdr_bs << endl
    << "key_bs "		<< s.key_bs << endl
    << "data_bs "		<< s.data_bs << endl
    << "entry_overhead_bs "	<< s.entry_overhead_bs << endl
    << "unused_bs "		<< s.unused_bs << endl
    << "entry_cnt "		<< s.entry_cnt << endl
    << "unique_cnt "		<< s.unique_cnt << endl
    ;
    */
    s.print(o,"");
    return o;
}

void btree_lf_stats_t::print(ostream& o, const char *pfx) const
{
    const btree_lf_stats_t &s = *this;
    o
    << pfx << "hdr_bs "		<< s.hdr_bs << endl
    << pfx << "key_bs "		<< s.key_bs << endl
    << pfx << "data_bs "		<< s.data_bs << endl
    << pfx << "entry_overhead_bs "	<< s.entry_overhead_bs << endl
    << pfx << "unused_bs "		<< s.unused_bs << endl
    << pfx << "entry_cnt "		<< s.entry_cnt << endl
    << pfx << "unique_cnt "		<< s.unique_cnt << endl
    ;
}


void
btree_int_stats_t::clear()
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(base_stat_t); i++) {
	((base_stat_t*)this)[i] = 0;
    }
}

void
btree_int_stats_t::add(const btree_int_stats_t& stats)
{
    w_assert3(sizeof(*this) % sizeof(base_stat_t) == 0);
    for (uint i = 0; i < sizeof(*this)/sizeof(base_stat_t); i++) {
	((base_stat_t*)this)[i] += ((base_stat_t*)&stats)[i];
    }
}

w_rc_t
btree_int_stats_t::audit() const
{
    FUNC(btree_int_stats_t::audit);
    if (total_bytes() % smlevel_0::page_sz != 0) {
	DBG(
	    << " btree_int_stats_t::total_bytes= " << total_bytes()
	    << " smlevel_0::page_sz= " << smlevel_0::page_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    DBG(<<"btree_int_stats_t audit ok");
    return RCOK;
}

base_stat_t
btree_int_stats_t::total_bytes() const
{
    return used_bs + unused_bs;
}


ostream& operator<<(ostream& o, const btree_int_stats_t& s)
{
    /*
    return o
    << "used_bs "		<< s.used_bs << endl
    << "unused_bs "		<< s.unused_bs << endl
    ;
    */
    s.print(o,"");
    return o;
}

void btree_int_stats_t::print(ostream& o, const char *pfx) const
{
    const btree_int_stats_t &s = *this;
    o
    << pfx << "used_bs "		<< s.used_bs << endl
    << pfx << "unused_bs "		<< s.unused_bs << endl
    ;
}


void
btree_stats_t::clear()
{
    leaf_pg.clear();
    int_pg.clear();

    leaf_pg_cnt = 0;
    int_pg_cnt = 0;
    unlink_pg_cnt = 0;
    unalloc_pg_cnt = 0;
    level_cnt = 0;
}

void
btree_stats_t::add(const btree_stats_t& stats)
{
    leaf_pg.add(stats.leaf_pg);
    int_pg.add(stats.int_pg);

    leaf_pg_cnt += stats.leaf_pg_cnt;
    int_pg_cnt += stats.int_pg_cnt;
    unlink_pg_cnt += stats.unlink_pg_cnt;
    unalloc_pg_cnt += stats.unalloc_pg_cnt;
    level_cnt = MAX(level_cnt, stats.level_cnt);
}

w_rc_t
btree_stats_t::audit() const
{
    FUNC(btree_stats_t::audit);
    W_DO(leaf_pg.audit());
    W_DO(int_pg.audit());

    base_stat_t total_alloc_pgs = alloc_pg_cnt();
    if (total_alloc_pgs*smlevel_0::page_sz != total_bytes()) {
	DBG(
	    << " leaf_pg_cnt= " << leaf_pg_cnt
	    << " int_pg_cnt= " << int_pg_cnt
	    << " btree_stats_t::total_bytes= " << total_bytes()
	    << " smlevel_0::page_sz= " << smlevel_0::page_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    if ( (total_alloc_pgs + unlink_pg_cnt + unalloc_pg_cnt) %
	 smlevel_0::ext_sz != 0) {
	DBG(
	    << " leaf_pg_cnt= " << leaf_pg_cnt
	    << " int_pg_cnt= " << int_pg_cnt
	    << " unlink_pg_cnt= " << unlink_pg_cnt
	    << " unalloc_pg_cnt= " << unalloc_pg_cnt
	    << " smlevel_0::ext_sz= " << smlevel_0::ext_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    DBG(<<"btree_stats_t audit ok");
    return RCOK;
}

base_stat_t
btree_stats_t::total_bytes() const
{
    return leaf_pg.total_bytes() + int_pg.total_bytes();
}

base_stat_t
btree_stats_t::alloc_pg_cnt() const
{
    return leaf_pg_cnt + int_pg_cnt;
}


ostream& operator<<(ostream& o, const btree_stats_t& s)
{
    /*
    return o << s.leaf_pg << s.int_pg
    << "leaf_pg_cnt "		<< s.leaf_pg_cnt << endl
    << "int_pg_cnt "		<< s.int_pg_cnt << endl
    << "unlink_pg_cnt "		<< s.unlink_pg_cnt << endl
    << "unalloc_pg_cnt "	<< s.unalloc_pg_cnt << endl
    << "level_cnt "		<< s.level_cnt << endl
    ;
    */
    s.print(o,"");
    return o;
}

void btree_stats_t::print(ostream& o, const char *pfx) const
{
    const btree_stats_t &s = *this;
    unsigned int pfxlen = strlen(pfx);
    char *pfx1 = new char[strlen(pfx) + 30];
    memcpy(pfx1, pfx, pfxlen);
	memcpy(pfx1+pfxlen, "lfpg.", 6);
	s.leaf_pg.print(o,pfx1); 

	memcpy(pfx1+pfxlen, "inpg.", 6);
	s.int_pg.print(o,pfx1); 
    delete[] pfx1;

    o
    << pfx << "leaf_pg_cnt "		<< s.leaf_pg_cnt << endl
    << pfx << "int_pg_cnt "		<< s.int_pg_cnt << endl
    << pfx << "unlink_pg_cnt "		<< s.unlink_pg_cnt << endl
    << pfx << "unalloc_pg_cnt "	<< s.unalloc_pg_cnt << endl
    << pfx << "level_cnt "		<< s.level_cnt << endl
    ;
}


void
rtree_stats_t::clear()
{
    entry_cnt = 0;
    unique_cnt = 0;
    leaf_pg_cnt = 0;
    int_pg_cnt = 0;
    unalloc_pg_cnt = 0;
    fill_percent = 0;
    level_cnt = 0;
}

void
rtree_stats_t::add(const rtree_stats_t& stats)
{
    entry_cnt += stats.entry_cnt;
    unique_cnt += stats.unique_cnt;
    leaf_pg_cnt += stats.leaf_pg_cnt;
    int_pg_cnt += stats.int_pg_cnt;
    unalloc_pg_cnt += stats.unalloc_pg_cnt;
    // this should be a weighted average ... oh well.
    fill_percent = MAX(fill_percent, stats.fill_percent);
    level_cnt = MAX(level_cnt, stats.level_cnt);
}

w_rc_t
rtree_stats_t::audit() const
{
    FUNC(rtree_stats_t::audit);
    if ( (leaf_pg_cnt + int_pg_cnt + unalloc_pg_cnt) % smlevel_0::ext_sz != 0) {
	DBG(
	    << " leaf_pg_cnt= " << leaf_pg_cnt
	    << " int_pg_cnt= " << int_pg_cnt
	    << " unalloc_pg_cnt= " << unalloc_pg_cnt
	    << " smlevel_0::ext_sz= " << smlevel_0::ext_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    DBG(<<"rtree_stats_t audit ok");
    return RCOK;
}

base_stat_t
rtree_stats_t::total_bytes() const
{
    return (leaf_pg_cnt + int_pg_cnt) * smlevel_0::page_sz;
}


ostream& operator<<(ostream& o, const rtree_stats_t& s)
{
/*
    return o
    << "entry_cnt "		<< s.entry_cnt << endl
    << "unique_cnt "		<< s.unique_cnt << endl
    << "leaf_pg_cnt "		<< s.leaf_pg_cnt << endl
    << "int_pg_cnt "		<< s.int_pg_cnt << endl
    << "unalloc_pg_cnt "	<< s.unalloc_pg_cnt << endl
    << "fill_percent "		<< s.fill_percent << endl
    << "level_cnt "		<< s.level_cnt << endl
    ;
*/
    s.print(o, "");
    return o;
}

void rtree_stats_t::print(ostream& o, const char *pfx) const
{
    const rtree_stats_t &s = *this;
    o
    << pfx << "entry_cnt "		<< s.entry_cnt << endl
    << pfx << "unique_cnt "		<< s.unique_cnt << endl
    << pfx << "leaf_pg_cnt "		<< s.leaf_pg_cnt << endl
    << pfx << "int_pg_cnt "		<< s.int_pg_cnt << endl
    << pfx << "unalloc_pg_cnt "	<< s.unalloc_pg_cnt << endl
    << pfx << "fill_percent "		<< s.fill_percent << endl
    << pfx << "level_cnt "		<< s.level_cnt << endl
    ;
}


void
volume_hdr_stats_t::clear()
{
    hdr_ext_cnt = 0;
    alloc_ext_cnt   = 0;
    unalloc_ext_cnt = 0;
    extent_size = 0;
}

void
volume_hdr_stats_t::add(const volume_hdr_stats_t& stats)
{
    hdr_ext_cnt += stats.hdr_ext_cnt;
    alloc_ext_cnt   += stats.alloc_ext_cnt;
    unalloc_ext_cnt += stats.unalloc_ext_cnt;
    extent_size = MAX(extent_size, stats.extent_size);
}

w_rc_t
volume_hdr_stats_t::audit() const
{
    FUNC(volume_hdr_stats_t::audit);
    if (extent_size != smlevel_0::ext_sz) {
	DBG(
	    << " extent_size= " << extent_size
	    << " smlevel_0::ext_sz= " << smlevel_0::ext_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    DBG(<<"volume_hdr_stats_t audit ok");
    return RCOK;
}

base_stat_t
volume_hdr_stats_t::total_bytes() const
{
    return hdr_ext_cnt * smlevel_0::ext_sz * smlevel_0::page_sz;
}

ostream& operator<<(ostream& o, const volume_hdr_stats_t& s)
{
    /*
    return o
    << "hdr_ext_cnt "		<< s.hdr_ext_cnt << endl
    << "alloc_ext_cnt "		<< s.alloc_ext_cnt << endl
    << "unalloc_ext_cnt "	<< s.unalloc_ext_cnt << endl
    << "extent_size "		<< s.extent_size << endl
    ;
    */
    s.print(o, "");
    return o;
}

void volume_hdr_stats_t::print(ostream& o, const char *pfx) const
{
    const volume_hdr_stats_t &s = *this;
    o
    << pfx << "hdr_ext_cnt "		<< s.hdr_ext_cnt << endl
    << pfx << "alloc_ext_cnt "		<< s.alloc_ext_cnt << endl
    << pfx << "unalloc_ext_cnt "	<< s.unalloc_ext_cnt << endl
    << pfx << "extent_size "		<< s.extent_size << endl
    ;
}


void
volume_map_stats_t::clear()
{
    store_directory.clear();
    root_index.clear();
    lid_map.clear();
    lid_remote_map.clear();
}

void
volume_map_stats_t::add(const volume_map_stats_t& stats)
{
    store_directory.add(stats.store_directory);
    root_index.add(stats.root_index);
    lid_map.add(stats.lid_map);
    lid_remote_map.add(stats.lid_remote_map);
}

w_rc_t
volume_map_stats_t::audit() const
{
    FUNC(volume_map_stats_t::audit);
    W_DO(store_directory.audit());
    W_DO(root_index.audit());
    W_DO(lid_map.audit());
    W_DO(lid_remote_map.audit());

    if ( (alloc_pg_cnt() + unalloc_pg_cnt()) %
	 smlevel_0::ext_sz != 0) {
	DBG(
	    << " alloc_pg_cnt= " << alloc_pg_cnt()
	    << " unalloc_pg_cnt= " << unalloc_pg_cnt()
	    << " smlevel_0::ext_sz= " << smlevel_0::ext_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    DBG(<<"volume_hdr_stats_t audit ok");
    return RCOK;
}

base_stat_t
volume_map_stats_t::total_bytes() const
{
    return store_directory.total_bytes() +
           root_index.total_bytes() +
           lid_map.total_bytes() +
           lid_remote_map.total_bytes();
}

base_stat_t
volume_map_stats_t::alloc_pg_cnt() const
{
    return store_directory.alloc_pg_cnt() +
           root_index.alloc_pg_cnt() +
           lid_map.alloc_pg_cnt() +
           lid_remote_map.alloc_pg_cnt();
}

base_stat_t
volume_map_stats_t::unalloc_pg_cnt() const
{
    return store_directory.unalloc_pg_cnt +
           root_index.unalloc_pg_cnt +
           lid_map.unalloc_pg_cnt +
           lid_remote_map.unalloc_pg_cnt+
           store_directory.unlink_pg_cnt +
           root_index.unlink_pg_cnt +
           lid_map.unlink_pg_cnt +
           lid_remote_map.unlink_pg_cnt;
}


ostream& operator<<(ostream& o, const volume_map_stats_t& s)
{
    /*
    return o << s.store_directory << s.root_index << s.lid_map
	     << s.lid_remote_map;
    */
    s.print(o, "");
    return o;
}

void volume_map_stats_t::print(ostream& o, const char *pfx) const
{
    const volume_map_stats_t &s = *this;
    unsigned int pfxlen = strlen(pfx);
    char *pfx1 = new char[strlen(pfx) + 30];
    memcpy(pfx1, pfx, pfxlen);
	memcpy(pfx1+pfxlen, "sdir.", 6);
	s.store_directory.print(o,pfx1); 

	memcpy(pfx1+pfxlen, "rind.", 6);
	s.root_index.print(o,pfx1); 

	memcpy(pfx1+pfxlen, "lidm.", 6);
	s.lid_map.print(o,pfx1); 

	memcpy(pfx1+pfxlen, "remm.", 6);
	s.lid_remote_map.print(o,pfx1); 
    delete[] pfx1;
}


void
small_store_stats_t::clear()
{
    btree_lf.clear();

    btree_cnt = 0;
    unalloc_pg_cnt = 0;
}

void
small_store_stats_t::add(const small_store_stats_t& stats)
{
    btree_lf.add(stats.btree_lf);

    btree_cnt += stats.btree_cnt;
    unalloc_pg_cnt += stats.unalloc_pg_cnt;
}

w_rc_t
small_store_stats_t::audit() const
{
    FUNC(small_store_stats_t::audit);
    W_DO(btree_lf.audit());

    base_stat_t total_alloc_pgs = btree_cnt;
    if (total_alloc_pgs*smlevel_0::page_sz != total_bytes()) {
	DBG(
	    << " btree_cnt= " << btree_cnt
	    << " small_store_stats_t::total_bytes= " << total_bytes()
	    << " smlevel_0::page_sz= " << smlevel_0::page_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    if ( (total_alloc_pgs + unalloc_pg_cnt) %
	 smlevel_0::ext_sz != 0) {
	DBG(
	    << " btree_cnt= " << btree_cnt
	    << " unalloc_pg_cnt= " << unalloc_pg_cnt
	    << " smlevel_0::ext_sz= " << smlevel_0::ext_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    DBG(<<"small_store_stats_t audit ok");
    return RCOK;
}

base_stat_t
small_store_stats_t::total_bytes() const
{
    return btree_lf.total_bytes();
}

base_stat_t
small_store_stats_t::alloc_pg_cnt() const
{
    return btree_cnt;
}


ostream& operator<<(ostream& o, const small_store_stats_t& s)
{
/*
    return o << s.btree_lf
    << "btree_cnt "		<< s.btree_cnt << endl
    << "unalloc_pg_cnt "	<< s.unalloc_pg_cnt << endl
    ;
*/
    s.print(o,"");
    return o;
}

void small_store_stats_t::print(ostream& o, const char *pfx) const
{
    const small_store_stats_t &s = *this;
    unsigned int pfxlen = strlen(pfx);
    char *pfx1 = new char[strlen(pfx) + 30];
    memcpy(pfx1, pfx, pfxlen);
	memcpy(pfx1+pfxlen, "leaf.", 6);
	s.btree_lf.print(o,pfx1); 
    delete[] pfx1;

    o
    << pfx << "btree_cnt "		<< s.btree_cnt << endl
    << pfx << "unalloc_pg_cnt "	<< s.unalloc_pg_cnt << endl
    ;
}





void
sm_du_stats_t::clear()
{
    file.clear();
    btree.clear();
    rtree.clear();
    rdtree.clear();
    volume_hdr.clear();
    volume_map.clear();
    small_store.clear();

    file_cnt = 0;
    btree_cnt = 0;
    rtree_cnt = 0;
    rdtree_cnt = 0;
}

void
sm_du_stats_t::add(const sm_du_stats_t& stats)
{
    file.add(stats.file);
    btree.add(stats.btree);
    rtree.add(stats.rtree);
    rdtree.add(stats.rdtree);
    volume_hdr.add(stats.volume_hdr);
    volume_map.add(stats.volume_map);
    small_store.add(stats.small_store);

    file_cnt += stats.file_cnt;
    btree_cnt += stats.btree_cnt;
    rtree_cnt += stats.rtree_cnt;
    rdtree_cnt += stats.rdtree_cnt;
}

w_rc_t
sm_du_stats_t::audit() const
{
    FUNC(sm_du_stats_t::audit);
    W_DO(file.audit());
    W_DO(btree.audit());
    W_DO(rtree.audit());
    W_DO(rdtree.audit());
    W_DO(volume_hdr.audit());
    W_DO(volume_map.audit());
    W_DO(small_store.audit());

    base_stat_t alloc_pg_cnt =
	(volume_hdr.alloc_ext_cnt + volume_hdr.hdr_ext_cnt) * 
	smlevel_0::ext_sz -
	(file.unalloc_file_pg_cnt + file.unalloc_large_pg_cnt +
	 btree.unalloc_pg_cnt + btree.unlink_pg_cnt +
	 rtree.unalloc_pg_cnt + rdtree.unalloc_pg_cnt+
	 volume_map.unalloc_pg_cnt() + small_store.unalloc_pg_cnt);

    base_stat_t alloc_and_unalloc_cnt =
	    (btree.leaf_pg_cnt + btree.int_pg_cnt +
	    rtree.leaf_pg_cnt + rtree.int_pg_cnt +
	    rdtree.leaf_pg_cnt + rdtree.int_pg_cnt +
	    file.file_pg_cnt + file.lgdata_pg_cnt +
	    file.lgindex_pg_cnt + volume_map.alloc_pg_cnt() +
	    small_store.alloc_pg_cnt())
	    +
	    (file.unalloc_file_pg_cnt + file.unalloc_large_pg_cnt +
	     btree.unalloc_pg_cnt + btree.unlink_pg_cnt +
	     rtree.unalloc_pg_cnt + rdtree.unalloc_pg_cnt + 
	     volume_map.unalloc_pg_cnt() + small_store.unalloc_pg_cnt
	    );
    
    if (alloc_and_unalloc_cnt != 
	volume_hdr.alloc_ext_cnt * smlevel_0::ext_sz ) {
	DBG(
	    << " alloc total pages = " << alloc_and_unalloc_cnt
	    << " ext total pages = " << volume_hdr.alloc_ext_cnt * smlevel_0::ext_sz
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }


    if ( alloc_pg_cnt * smlevel_0::page_sz != total_bytes()) {
	DBG(
	    << " alloc_pg_cnt= " << alloc_pg_cnt
	    << " ditto * page_sz= " << alloc_pg_cnt * smlevel_0::page_sz
	    << " total bytes= " << total_bytes()
	);
	stats_audit_failed();
	return RC(fcINTERNAL);
    }
    DBG(<<"sm_du_stats_t audit ok");
    return RCOK;
}

base_stat_t
sm_du_stats_t::total_bytes() const
{
    return file.total_bytes() + btree.total_bytes() +
	   rtree.total_bytes() + rdtree.total_bytes() +
	   volume_hdr.total_bytes() + volume_map.total_bytes() +
	   small_store.total_bytes();
}

ostream& operator<<(ostream& o, const sm_du_stats_t& s)
{
    /*
    return o << s.file << s.btree << s.rtree << s.rdtree
	     << s.volume_hdr << s.volume_map << s.small_store << endl
    << "file_cnt "	<< s.file_cnt << endl
    << "btree_cnt "	<< s.btree_cnt << endl
    << "rtree_cnt "	<< s.rtree_cnt << endl
    << "rdtree_cnt "	<< s.rdtree_cnt << endl
    ;
    */
    s.print(o,"");
    return o;
}

void sm_du_stats_t::print(ostream& o, const char *pfx) const
{
    const sm_du_stats_t &s = *this;
    unsigned int pfxlen = strlen(pfx);
    char *pfx1 = new char[strlen(pfx) + 30];
    memcpy(pfx1, pfx, pfxlen);
	memcpy(pfx1+pfxlen, "file.", 6);
	s.file.print(o,pfx1); 

	memcpy(pfx1+pfxlen, "btre.", 6);
	s.btree.print(o,pfx1); 

	memcpy(pfx1+pfxlen, "rtre.", 6);
	s.rtree.print(o,pfx1); 

	memcpy(pfx1+pfxlen, "rdtr.", 6);
	s.rdtree.print(o,pfx1); 

	memcpy(pfx1+pfxlen, "volh.", 6);
	s.volume_hdr.print(o,pfx1); 

	memcpy(pfx1+pfxlen, "volm.", 6);
	s.volume_map.print(o,pfx1); 

	memcpy(pfx1+pfxlen, "smst.", 6);
	s.small_store.print(o,pfx1); 
    delete[] pfx1;

    o
    << pfx << "file_cnt "	<< s.file_cnt << endl
    << pfx << "btree_cnt "	<< s.btree_cnt << endl
    << pfx << "rtree_cnt "	<< s.rtree_cnt << endl
    << pfx << "rdtree_cnt "	<< s.rdtree_cnt << endl
    ;
}

