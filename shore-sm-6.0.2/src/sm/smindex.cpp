/* -*- mode:C++; c-basic-offset:4 -*-
     Shore-MT -- Multi-threaded port of the SHORE storage manager
   
                       Copyright (c) 2007-2009
      Data Intensive Applications and Systems Labaratory (DIAS)
               Ecole Polytechnique Federale de Lausanne
   
                         All Rights Reserved.
   
   Permission to use, copy, modify and distribute this software and
   its documentation is hereby granted, provided that both the
   copyright notice and this permission notice appear in all copies of
   the software, derivative works or modified versions, and any
   portions thereof, and that both notices appear in supporting
   documentation.
   
   This code is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. THE AUTHORS
   DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER
   RESULTING FROM THE USE OF THIS SOFTWARE.
*/

/*<std-header orig-src='shore'>

 $Id: smindex.cpp,v 1.105 2010/11/08 15:07:06 nhall Exp $

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

#define SM_SOURCE
#define SMINDEX_C
#include "sm_int_4.h"
#include "sm_du_stats.h"
#include "sm.h"

/*==============================================================*
 *  Physical ID version of all the index operations                *
 *==============================================================*/

/*********************************************************************
 *
 *  ss_m::create_index(vid, ntype, property, key_desc, stid)
 *  ss_m::create_index(vid, ntype, property, key_desc, cc, stid)
 *
 *********************************************************************/
rc_t
ss_m::create_index(
    vid_t                   vid, 
    ndx_t                   ntype, 
    store_property_t        property,
    const char*             key_desc,
    stid_t&                 stid
    )
{
    return 
    create_index(vid, ntype, property, key_desc, t_cc_kvl, stid);
}

rc_t
ss_m::create_index(
    vid_t                 vid, 
    ndx_t                 ntype, 
    store_property_t      property,
    const char*           key_desc,
    concurrency_t         cc, 
    stid_t&               stid
    )
{
    SM_PROLOGUE_RC(ss_m::create_index, in_xct, read_write, 0);
    if(property == t_temporary) {
                return RC(eBADSTOREFLAGS);
    }
    W_DO(_create_index(vid, ntype, property, key_desc, cc, stid));

    return RCOK;
}

rc_t
ss_m::create_md_index(
    vid_t                 vid, 
    ndx_t                 ntype, 
    store_property_t         property,
    stid_t&                 stid, 
    int2_t                 dim
    )
{
    SM_PROLOGUE_RC(ss_m::create_md_index, in_xct, read_write, 0);
    W_DO(_create_md_index(vid, ntype, property,
                          stid, dim));
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::destroy_index()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_index(const stid_t& iid)
{
    SM_PROLOGUE_RC(ss_m::destroy_index, in_xct, read_write, 0);
    W_DO( _destroy_index(iid) );
    return RCOK;
}

rc_t
ss_m::destroy_md_index(const stid_t& iid)
{
    SM_PROLOGUE_RC(ss_m::destroy_md_index, in_xct, read_write, 0);
    W_DO( _destroy_md_index(iid) );
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::bulkld_index()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::bulkld_index(
    const stid_t&         stid, 
    int                   nsrcs,
    const stid_t*         source,
    sm_du_stats_t&        _stats,
    bool                  sort_duplicates, // = true
    bool                  lexify_keys // = true
    )
{
    SM_PROLOGUE_RC(ss_m::bulkld_index, in_xct, read_write, 0);
    W_DO(_bulkld_index(stid, nsrcs, source, _stats, sort_duplicates, lexify_keys) );
    return RCOK;
}

w_rc_t        ss_m::bulkld_index(
    const  stid_t        &stid,
    const  stid_t        &source,
    sm_du_stats_t        &_stats,
    bool                 sort_duplicates,
    bool                 lexify_keys
    )
{
    return bulkld_index(stid, 1, &source, _stats,
                        sort_duplicates, lexify_keys);
}

rc_t
ss_m::bulkld_md_index(
    const stid_t&         stid, 
    int                   nsrcs,
    const stid_t*         source,
    sm_du_stats_t&        _stats,
    int2_t                hff, 
    int2_t                hef, 
    nbox_t*               universe)
{
    SM_PROLOGUE_RC(ss_m::bulkld_md_index, in_xct, read_write, 0);
    W_DO(_bulkld_md_index(stid, nsrcs, source, _stats, hff, hef, universe));
    return RCOK;
}

w_rc_t        
ss_m::bulkld_md_index(
    const stid_t        &stid,
    const stid_t        &source,
    sm_du_stats_t       &_stats,
    int2_t              hff,
    int2_t              hef,
    nbox_t              *universe
)
{
    return bulkld_md_index(stid, 1, &source, _stats, hff, hef, universe);
}

rc_t
ss_m::bulkld_index(
    const stid_t&         stid, 
    sort_stream_i&         sorted_stream,
    sm_du_stats_t&         _stats)
{
    SM_PROLOGUE_RC(ss_m::bulkld_index, in_xct, read_write, 0);
    W_DO(_bulkld_index(stid, sorted_stream, _stats) );
    DBG(<<"bulkld_index " <<stid<<" returning RCOK");
    return RCOK;
}

rc_t
ss_m::bulkld_md_index(
    const stid_t&         stid, 
    sort_stream_i&        sorted_stream,
    sm_du_stats_t&        _stats,
    int2_t                hff, 
    int2_t                hef, 
    nbox_t*               universe)
{
    SM_PROLOGUE_RC(ss_m::bulkld_md_index, in_xct, read_write, 0);
    W_DO(_bulkld_md_index(stid, sorted_stream, _stats, hff, hef, universe));
    return RCOK;
}
    
/*--------------------------------------------------------------*
 *  ss_m::print_index()                                                *
 *--------------------------------------------------------------*/
rc_t
ss_m::print_index(stid_t stid)
{
    SM_PROLOGUE_RC(ss_m::print_index, in_xct, read_only, 0);
    W_DO(_print_index(stid));
    return RCOK;
}

rc_t
ss_m::print_md_index(stid_t stid, ostream &out)
{
    SM_PROLOGUE_RC(ss_m::print_index, in_xct, read_only, 0);
    W_DO(_print_md_index(stid, out));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_assoc()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::create_assoc(stid_t stid, const vec_t& key, const vec_t& el
#ifdef SM_DORA
                   , const bool bIgnoreLocks
#endif
        )
{
    SM_PROLOGUE_RC(ss_m::create_assoc, in_xct, read_write, 0);
    W_DO(_create_assoc(stid, key, el
#ifdef SM_DORA
                       , bIgnoreLocks
#endif
                       ));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::destroy_assoc()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_assoc(stid_t stid, const vec_t& key, const vec_t& el
#ifdef SM_DORA
                   , const bool bIgnoreLocks
#endif
                    )
{
    SM_PROLOGUE_RC(ss_m::destroy_assoc, in_xct, read_write, 0);
    W_DO(_destroy_assoc(stid, key, el
#ifdef SM_DORA
                        , bIgnoreLocks
#endif
                        ));
    return RCOK;
}



/*--------------------------------------------------------------*
 *  ss_m::destroy_all_assoc()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_all_assoc(stid_t stid, const vec_t& key, int& num)
{
    SM_PROLOGUE_RC(ss_m::destroy_assoc, in_xct, read_write, 0);
    W_DO(_destroy_all_assoc(stid, key, num));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::find_assoc()                                                *
 *--------------------------------------------------------------*/
rc_t
ss_m::find_assoc(stid_t stid, const vec_t& key, 
                 void* el, smsize_t& elen, bool& found
#ifdef SM_DORA
                 , const bool bIgnoreLocks
#endif
              )
{
    SM_PROLOGUE_RC(ss_m::find_assoc, in_xct, read_only, 0);
    W_DO(_find_assoc(stid, key, el, elen, found
#ifdef SM_DORA
                     , bIgnoreLocks
#endif
                     ));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_md_assoc()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::create_md_assoc(stid_t stid, const nbox_t& key, const vec_t& el)
{
    SM_PROLOGUE_RC(ss_m::create_md_assoc, in_xct, read_write, 0);
    W_DO(_create_md_assoc(stid, key, el));
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::find_md_assoc()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::find_md_assoc(stid_t stid, const nbox_t& key,
                    void* el, smsize_t& elen, bool& found)
{
    SM_PROLOGUE_RC(ss_m::find_assoc, in_xct, read_only, 0);
    W_DO(_find_md_assoc(stid, key, el, elen, found));
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::destroy_md_assoc()                                    *
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_md_assoc(stid_t stid, const nbox_t& key, const vec_t& el)
{
    SM_PROLOGUE_RC(ss_m::destroy_md_assoc, in_xct, read_write, 0);
    W_DO(_destroy_md_assoc(stid, key, el));
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::draw_rtree()                                                *
 *--------------------------------------------------------------*/
rc_t
ss_m::draw_rtree(const stid_t& stid, ostream &s)
{
    SM_PROLOGUE_RC(ss_m::draw_rtree, in_xct, read_only, 0);
    W_DO(_draw_rtree(stid, s));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::rtree_stats()                                                *
 *--------------------------------------------------------------*/
rc_t
ss_m::rtree_stats(const stid_t& stid, rtree_stats_t& stat, 
                uint2_t size, uint2_t* ovp, bool audit)
{
    SM_PROLOGUE_RC(ss_m::rtree_stats, in_xct, read_only, 0);
    W_DO(_rtree_stats(stid, stat, size, ovp, audit));
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::_create_index()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::_create_index(
    vid_t                   vid, 
    ndx_t                   ntype, 
    store_property_t        property,
    const char*             key_desc,
    concurrency_t           cc, // = t_cc_kvl
    stid_t&                 stid
    )
{
    FUNC(ss_m::_create_index);

    DBG(<<" vid " << vid);
    uint4_t count = max_keycomp;
    key_type_s kcomp[max_keycomp];
    lpid_t root;

    W_DO( key_type_s::parse_key_type(key_desc, count, kcomp) );
    {
        DBG(<<"vid " << vid);
        W_DO( io->create_store(vid, 100/*unused*/, _make_store_flag(property), stid) );
    DBG(<<" stid " << stid);
    }

    // Note: theoretically, some other thread could destroy
    //       the above store before the following lock request
    //       is granted.  The only forseable way for this to
    //       happen would be due to a bug in a vas causing
    //       it to destroy the wrong store.  We make no attempt
    //       to prevent this.
    W_DO(lm->lock(stid, EX, t_long, WAIT_SPECIFIED_BY_XCT));

    if( (cc != t_cc_none) && (cc != t_cc_file) &&
        (cc != t_cc_kvl) && (cc != t_cc_modkvl) &&
        (cc != t_cc_im)
        ) return RC(eBADCCLEVEL);

    switch (ntype)  {
    case t_btree:
    case t_uni_btree:
        // compress prefixes only if the first part is compressed
        W_DO( bt->create(stid, root, kcomp[0].compressed != 0) );

        break;
    default:
        return RC(eBADNDXTYPE);
    }
    sinfo_s sinfo(stid.store, t_index, 100/*unused*/, 
                  ntype,
                  cc,
                  root.page, 
                  count, kcomp);
    W_DO( dir->insert(stid, sinfo) );

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_create_md_index()                                    *
 *--------------------------------------------------------------*/
rc_t
ss_m::_create_md_index(
    vid_t                 vid, 
    ndx_t                 ntype, 
    store_property_t      property,
    stid_t&               stid, 
    int2_t                dim
    )
{
    W_DO( io->create_store(vid, 100/*unused*/, 
                           _make_store_flag(property), stid) );

    lpid_t root;

    // Note: theoretically, some other thread could destroy
    //       the above store before the following lock request
    //       is granted.  The only forseable way for this to
    //       happen would be due to a bug in a vas causing
    //       it to destroy the wrong store.  We make no attempt
    //       to prevent this.
    W_DO(lm->lock(stid, EX, t_long, WAIT_SPECIFIED_BY_XCT));

    switch (ntype)  {
    case t_rtree:
        W_DO( rt->create(stid, root, dim) );
        break;
    default:
        return RC(eBADNDXTYPE);
    }

    sinfo_s sinfo(stid.store, t_index, 100/*unused*/, 
                    ntype, t_cc_none, // cc not used for md indexes
                  root.page,
                  0, 0);
    W_DO( dir->insert(stid, sinfo) );

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_destroy_index()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::_destroy_index(const stid_t& iid)
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, iid, sd, EX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype)  {
    case t_btree:
    case t_uni_btree:
        W_DO( io->destroy_store(iid) );
        break;
    default:
        return RC(eBADNDXTYPE);
    }
    
    W_DO( dir->remove(iid) );
    return RCOK;
}

rc_t
ss_m::_destroy_md_index(const stid_t& iid)
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, iid, sd, EX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype)  {
    case t_rtree:
        W_DO( io->destroy_store(iid) );
        break;
    default:
        return RC(eBADNDXTYPE);
    }
    
    W_DO( dir->remove(iid) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_bulkld_index()                                        *
 *--------------------------------------------------------------*/

rc_t
ss_m::_bulkld_index(
    const stid_t&         stid,
    int                   nsrcs,
    const stid_t*         source,
    sm_du_stats_t&        _stats,
    bool                  sort_duplicates, //  = true
    bool                  lexify_keys //  = true
    )
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, EX ) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_btree:
    case t_uni_btree:
        DBG(<<"bulk loading root " << sd->root());
        W_DO( bt->bulk_load(sd->root(), 
            nsrcs,
            source,
            sd->sinfo().nkc, sd->sinfo().kc,
            sd->sinfo().ntype == t_uni_btree, 
            (concurrency_t)sd->sinfo().cc,
            _stats.btree,
            sort_duplicates,
            lexify_keys
            ) );
        break;
    default:
        return RC(eBADNDXTYPE);
    }
    {
        store_flag_t st;
        W_DO( io->get_store_flags(stid, st) );
        w_assert3(st != st_bad);
        if(st & (st_tmp|st_insert_file|st_load_file)) {
            DBG(<<"converting stid " << stid <<
                " from " << st << " to st_regular " );
            // After bulk load, it MUST be re-converted
            // to regular to prevent unlogged arbitrary inserts
            // Invalidate the pages so the store flags get reset
            // when the pages are read back in
            W_DO( io->set_store_flags(stid, st_regular) );
        }
    }
    return RCOK;
}

rc_t
ss_m::_bulkld_md_index(
    const stid_t&         stid, 
    int                   nsrcs,
    const stid_t*         source, 
    sm_du_stats_t&        _stats,
    int2_t                hff, 
    int2_t                hef, 
    nbox_t*               universe)
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, EX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_rtree:
        {
            rtld_desc_t desc(universe,hff,hef);
            W_DO( rt->bulk_load(sd->root(), nsrcs, source, desc, _stats.rtree) ); 
        }
        break;
    default:
        return RC(eBADNDXTYPE);
    }
    {
        store_flag_t st;
        W_DO( io->get_store_flags(stid, st) );
        if(st & (st_tmp|st_insert_file|st_load_file)) {
            // After bulk load, it MUST be re-converted
            // to regular to prevent unlogged arbitrary inserts
            // Invalidate the pages so the store flags get reset
            // when the pages are read back in
            W_DO( io->set_store_flags(stid, st_regular) );
        }
    }

    return RCOK;
}

rc_t
ss_m::_bulkld_index(
    const stid_t&         stid, 
    sort_stream_i&         sorted_stream, 
    sm_du_stats_t&         _stats
    )
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, EX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_btree:
    case t_uni_btree:
        W_DO( bt->bulk_load(sd->root(), sorted_stream,
                            sd->sinfo().nkc, sd->sinfo().kc,
                            sd->sinfo().ntype == t_uni_btree, 
                            (concurrency_t)sd->sinfo().cc, _stats.btree) );
        break;
    default:
        return RC(eBADNDXTYPE);
    }

    return RCOK;
}

rc_t
ss_m::_bulkld_md_index(
    const stid_t&         stid, 
    sort_stream_i&         sorted_stream, 
    sm_du_stats_t&        _stats,
    int2_t                 hff, 
    int2_t                 hef, 
    nbox_t*                 universe)
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, EX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_rtree:
        {
            rtld_desc_t desc(universe,hff,hef);
            W_DO( rt->bulk_load(sd->root(), sorted_stream, desc, _stats.rtree) );
        }
        break;
    default:
        return RC(eBADNDXTYPE);
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_print_index()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::_print_index(const stid_t& stid)
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, IS) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    if (sd->sinfo().nkc > 1) {
        //can't handle multi-part keys
        fprintf(stderr, "multi-part keys are not supported");
        return RC(eNOTIMPLEMENTED);
    }
    sortorder::keytype k = sortorder::convert(sd->sinfo().kc);
    switch (sd->sinfo().ntype) {
    case t_btree:
    case t_uni_btree:
        bt->print(sd->root(), k);
        break;
    default:
        return RC(eBADNDXTYPE);
    }

    return RCOK;
}

rc_t
ss_m::_print_md_index(stid_t stid, ostream &out)
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, IS) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_rtree:
        W_DO( rt->print(sd->root(), out) );
        break;
    default:
        return RC(eBADNDXTYPE);
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_create_assoc()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::_create_assoc(
    const stid_t&        stid, 
    const vec_t&         key, 
    const vec_t&         el
#ifdef SM_DORA
    , const bool bIgnoreLocks
#endif
)
{
    // usually we will do kvl locking and already have an IX lock
    // on the index
    lock_mode_t                index_mode = NL;// lock mode needed on index

    // determine if we need to change the settins of cc and index_mode
    concurrency_t cc = t_cc_bad;

#ifdef SM_DORA
    // IP: DORA inserts using the lowest concurrency and lock mode
    if (bIgnoreLocks) {
      cc = t_cc_none;
      index_mode = NL;
    } else {
#endif

    xct_t* xd = xct();
    if (xd)  {
        lock_mode_t lock_mode;
        W_DO( lm->query(stid, lock_mode, xd->tid(), true) );
        // cc is off if file is EX/SH/UD/SIX locked
        if (lock_mode == EX) {
            cc = t_cc_none;
        } else if (lock_mode == IX || lock_mode >= SIX) {
            // no changes needed
        } else {
            index_mode = IX;
        }
    }

#ifdef SM_DORA
    }
#endif

    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, index_mode) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    if (cc == t_cc_bad ) cc = (concurrency_t)sd->sinfo().cc;

    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
        return RC(eBADNDXTYPE);
    case t_btree:
    case t_uni_btree:
        W_DO( bt->insert(sd->root(), 
                     sd->sinfo().nkc, sd->sinfo().kc,
                     sd->sinfo().ntype == t_uni_btree, 
                     cc,
                     key, el, 50) );
        break;
    case t_rtree:
        fprintf(stderr, 
        "rtrees indexes do not support this function");
        return RC(eNOTIMPLEMENTED);
    default:
        W_FATAL_MSG(eINTERNAL, << "bad index type " << sd->sinfo().ntype );
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_destroy_assoc()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::_destroy_assoc(
    const stid_t  &      stid, 
    const vec_t&         key, 
    const vec_t&         el
#ifdef SM_DORA
    , const bool bIgnoreLocks
#endif
    )
{
    concurrency_t cc = t_cc_bad;
    // usually we will to kvl locking and already have an IX lock
    // on the index
    lock_mode_t                index_mode = NL;// lock mode needed on index

#ifdef SM_DORA
    // IP: DORA deletes using the lowest concurrency and lock mode
    if (bIgnoreLocks) {
      cc = t_cc_none;
      index_mode = NL;
    }
    else {
#endif

    // determine if we need to change the settins of cc and index_mode
    xct_t* xd = xct();
    if (xd)  {
        lock_mode_t lock_mode;
        W_DO( lm->query(stid, lock_mode, xd->tid(), true) );
        // cc is off if file is EX/SH/UD/SIX locked
        if (lock_mode == EX) {
            cc = t_cc_none;
        } else if (lock_mode == IX || lock_mode >= SIX) {
            // no changes needed
        } else {
            index_mode = IX;
        }
    }

#ifdef SM_DORA
    }
#endif

    DBG(<<"");

    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, index_mode) );
    DBG(<<"");

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    if (cc == t_cc_bad ) cc = (concurrency_t)sd->sinfo().cc;

    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
        return RC(eBADNDXTYPE);
    case t_btree:
    case t_uni_btree:
        W_DO( bt->remove(sd->root(), 
                 sd->sinfo().nkc, sd->sinfo().kc,
                 sd->sinfo().ntype == t_uni_btree,
                 cc, key, el) );
        break;
    case t_rtree:
        fprintf(stderr, 
        "rtree indexes do not support this function");
        return RC(eNOTIMPLEMENTED);
    default:
        W_FATAL_MSG(eINTERNAL, << "bad index type " << sd->sinfo().ntype );
    }
    DBG(<<"");
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_destroy_all_assoc()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::_destroy_all_assoc(const stid_t& stid, const vec_t& key, 
        int& num
        )
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, IX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    concurrency_t cc = (concurrency_t)sd->sinfo().cc;

    xct_t* xd = xct();
    if (xd)  {
        lock_mode_t lock_mode;
        W_DO( lm->query(stid, lock_mode, xd->tid(), true) );
        // cc is off if file is EX locked
        if (lock_mode == EX) cc = t_cc_none;
    }
    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
        return RC(eBADNDXTYPE);
    case t_btree:
    case t_uni_btree:
        W_DO( bt->remove_key(sd->root(), 
                     sd->sinfo().nkc, sd->sinfo().kc,
                     sd->sinfo().ntype == t_uni_btree,
                     cc, key, num) );
        break;
    case t_rtree:
        fprintf(stderr, 
        "rtree indexes do not support this function");
        return RC(eNOTIMPLEMENTED);
    default:
        W_FATAL_MSG(eINTERNAL, << "bad index type " << sd->sinfo().ntype );
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_find_assoc()                                                *
 *--------------------------------------------------------------*/
rc_t
ss_m::_find_assoc(
    const stid_t&         stid, 
    const vec_t&          key, 
    void*                 el, 
    smsize_t&             elen, 
    bool&                 found
#ifdef SM_DORA
    , const bool bIgnoreLocks
#endif
    )
{
    concurrency_t cc = t_cc_bad;
    // usually we will to kvl locking and already have an IS lock
    // on the index
    lock_mode_t                index_mode = NL;// lock mode needed on index

#ifdef SM_DORA
    // IP: DORA does the dir access and the index lookup 
    //     using the lowest concurrency and lock mode
    if (bIgnoreLocks) {
      cc = t_cc_none;
      index_mode = NL;
    }
    else {
#endif

    // determine if we need to change the settins of cc and index_mode
    xct_t* xd = xct();
    if (xd)  {
        lock_mode_t lock_mode;
        W_DO( lm->query(stid, lock_mode, xd->tid(), true, true) );
        // cc is off if file is EX/SH/UD/SIX locked
        if (lock_mode >= SH) {
            cc = t_cc_none;
        } else if (lock_mode >= IS) {
            // no changes needed
        } else {
            // Index isn't already locked; have to grab IS lock
            // on it below, via access()
            index_mode = IS;
        }
    }

#ifdef SM_DORA
    }
#endif

    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, index_mode) );
    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    if (cc == t_cc_bad ) cc = (concurrency_t)sd->sinfo().cc;

    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
        return RC(eBADNDXTYPE);
    case t_uni_btree:
    case t_btree:
        W_DO( bt->lookup(sd->root(), 
             sd->sinfo().nkc, sd->sinfo().kc,
             sd->sinfo().ntype == t_uni_btree,
             cc,
             key, el, elen, found) );
        break;

    case t_rtree:
        fprintf(stderr, 
        "rtree indexes do not support this function");
        return RC(eNOTIMPLEMENTED);
    default:
        W_FATAL_MSG(eINTERNAL, << "bad index type " << sd->sinfo().ntype );
    }
    
    return RCOK;
}



/*--------------------------------------------------------------*
 *  ss_m::destroy_md_assoc()                                    *
 *--------------------------------------------------------------*/
rc_t
ss_m::_destroy_md_assoc(stid_t stid, const nbox_t& key, const vec_t& el)
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, IX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
      case t_bad_ndx_t:
        return RC(eBADNDXTYPE);
      case t_rtree:
        W_DO( rt->remove(sd->root(), key, el) );
        break;
      case t_btree:
      case t_uni_btree:
        return RC(eBADNDXTYPE);
      default:
        W_FATAL_MSG(eINTERNAL, << "bad index type " << sd->sinfo().ntype );
    }

    return RCOK;
}



/*--------------------------------------------------------------*
 *  ss_m::_create_md_assoc()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::_create_md_assoc(stid_t stid, const nbox_t& key, const vec_t& el)
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, IX) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
      return RC(eBADNDXTYPE);
    case t_rtree:
        W_DO( rt->insert(sd->root(), key, el) );
        break;
    case t_btree:
    case t_uni_btree:
        return RC(eWRONGKEYTYPE);
    default:
        W_FATAL_MSG(eINTERNAL, << "bad index type " << sd->sinfo().ntype );
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_find_md_assoc()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::_find_md_assoc(
    stid_t                 stid, 
    const nbox_t&         key,
    void*                 el, 
    smsize_t&                 elen,
    bool&                 found)
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, IS) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
      return RC(eBADNDXTYPE);
    case t_rtree:
        // exact match
        W_DO( rt->lookup(sd->root(), key, el, elen, found) );
        break;
    case t_btree:
    case t_uni_btree:
        return RC(eWRONGKEYTYPE);

    default:
        W_FATAL_MSG(eINTERNAL, << "bad index type " << sd->sinfo().ntype );
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_draw_rtree()                                                *
 *--------------------------------------------------------------*/
rc_t
ss_m::_draw_rtree(const stid_t& stid, ostream &s)
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, IS) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
        return RC(eBADNDXTYPE);
    case t_rtree:
        W_DO( rt->draw(sd->root(), s) );
        break;
    case t_btree:
    case t_uni_btree:
        fprintf(stderr, 
        "linear-hash, btrees, rd-trees indexes do not support this function");
        return RC(eNOTIMPLEMENTED);

    default:
        W_FATAL_MSG(eINTERNAL, << "bad index type " << sd->sinfo().ntype );
    }

    return RCOK;
}

rc_t
ss_m::_rtree_stats(const stid_t& stid, rtree_stats_t& stat,
                 uint2_t size, uint2_t* ovp, bool audit)
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, IS) );

    if (sd->sinfo().stype != t_index)   return RC(eBADSTORETYPE);
    switch (sd->sinfo().ntype) {
    case t_bad_ndx_t:
        return RC(eBADNDXTYPE);
    case t_rtree:
        W_DO( rt->stats(sd->root(), stat, size, ovp, audit) );
        break;
    case t_btree:
    case t_uni_btree:
        fprintf(stderr, 
        "linear-hash, btrees, rd-trees indexes do not support this function");
        return RC(eNOTIMPLEMENTED);

    default:
        W_FATAL_MSG(eINTERNAL, << "bad index type " << sd->sinfo().ntype );
    }

    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::_get_store_info()                                        *
 *--------------------------------------------------------------*/
rc_t
ss_m::_get_store_info(
    const stid_t&         stid, 
    sm_store_info_t&        info
)
{
    sdesc_t* sd;
    W_DO( dir->access(t_index, stid, sd, NL) );

    const sinfo_s& s= sd->sinfo();

    info.store = s.store;
    info.stype = s.stype;
    info.ntype = s.ntype;
    info.cc    = s.cc;
    info.eff   = s.eff;
    info.large_store   = s.large_store;
    info.root   = s.root;
    info.nkc   = s.nkc;

    switch (sd->sinfo().ntype) {
    case t_btree:
    case t_uni_btree:
        W_DO( key_type_s::get_key_type(info.keydescr, 
                info.keydescrlen,
                sd->sinfo().nkc, sd->sinfo().kc ));
        break;
    default:
        break;
    }
    return RCOK;
}

