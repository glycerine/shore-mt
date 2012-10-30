/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: smfile.cc,v 1.44 1997/06/15 03:13:45 solomon Exp $
 */
#define SM_SOURCE
#define SMFILE_C

#include "w.h"
#include "option.h"
#include "sm_int_4.h"
#include "btcursor.h"
#include "lgrec.h"
#include "device.h"
#include "app_support.h"
#include "sm.h"

#define DBGTHRD(arg) DBG(<<" th."<<me()->id<<" tid."<<xct()->tid()<<" " arg)

/*==============================================================*
 *  Physical ID version of all the storage operations           *
 *==============================================================*/

/*--------------------------------------------------------------*
 *  ss_m::set_store_property()                                  *
 *--------------------------------------------------------------*/
rc_t
ss_m::set_store_property(stid_t stid, store_property_t property)
{
    SM_PROLOGUE_RC(ss_m::set_store_property, in_xct, 0);
    SMSCRIPT(<<"set_store_property " <<stid << " " << property );
    W_DO( _set_store_property( stid, property) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::get_store_property()                                  *
 *--------------------------------------------------------------*/
rc_t
ss_m::get_store_property(stid_t stid, store_property_t& property)
{
    SM_PROLOGUE_RC(ss_m::get_store_property, in_xct, 0);
    RES_SMSCRIPT(<<"get_store_property " <<stid );
    W_DO( _get_store_property( stid, property) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_file()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_file(
    vid_t 			vid, 
    stid_t& 			fid, 
    store_property_t 		property,
    const serial_t& 		serial)
{
    SM_PROLOGUE_RC(ss_m::create_file, in_xct, 0);
    RES_SMSCRIPT(<<"create_file " <<vid << " " << property << " " << serial );
    W_DO(_create_file(vid, fid, property, serial));
    DBGTHRD(<<"create_file returns " << fid);
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::destroy_file()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_file(const stid_t& fid)
{
    SM_PROLOGUE_RC(ss_m::destroy_file, in_xct, 0);
    RES_SMSCRIPT(<<"destroy_file " <<fid);
    W_DO(_destroy_file(fid));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_rec()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_rec(const stid_t& fid, const vec_t& hdr,
		 smsize_t len_hint, const vec_t& data, rid_t& new_rid,
		 const serial_t& serial )
{
    SM_PROLOGUE_RC(ss_m::create_rec, in_xct, 0);
    RES_SMSCRIPT(<<"create_rec " <<fid
	<<" " << hdr 
	<<" " << len_hint
	<<" " << data
	<<" " << serial );
    W_DO(_create_rec(fid, hdr, len_hint, data, new_rid, serial));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::destroy_rec()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_rec(const rid_t& rid)
{
    SM_PROLOGUE_RC(ss_m::destroy_rec, in_xct, 0);
    RES_SMSCRIPT(<<"destroy_rec " <<rid);
    W_DO(_destroy_rec(rid, serial_t::null));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::update_rec()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::update_rec(const rid_t& rid, smsize_t start, const vec_t& data)
{
    SM_PROLOGUE_RC(ss_m::update_rec, in_xct, 0);
    RES_SMSCRIPT(<<"update_rec " <<rid <<" "<<start
	<<" [vec " <<data << "] "
    );
    W_DO(_update_rec(rid, start, data, serial_t::null));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::update_rec_hdr()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::update_rec_hdr(const rid_t& rid, smsize_t start, const vec_t& hdr)
{
    SM_PROLOGUE_RC(ss_m::update_rec_hdr, in_xct, 0);
    RES_SMSCRIPT(<<"update_rec_hdr " <<rid <<" " << hdr );
    W_DO(_update_rec_hdr(rid, start, hdr, serial_t::null));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::append_rec()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::append_rec(const rid_t& rid, const vec_t& data, bool allow_forward)
{
    SM_PROLOGUE_RC(ss_m::append_rec, in_xct, 0);
    RES_SMSCRIPT(<<"append_rec " <<rid <<data << allow_forward);
    W_DO(_append_rec(rid, data, allow_forward, serial_t::null));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::truncate_rec()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::truncate_rec(const rid_t& rid, smsize_t amount)
{
    SM_PROLOGUE_RC(ss_m::truncate_rec, in_xct, 0);
    RES_SMSCRIPT(<<"truncate_rec " <<rid <<" " << amount);
    bool should_forward;
    W_DO(_truncate_rec(rid, amount, should_forward, serial_t::null));
    if (should_forward) {
	// The record is still implemented as large, even though
	// it could fit on a page (though not on this one).
	// It's possible to forward it, but that only seems useful
	// in the context of logical IDs.
    }
    return RCOK;
}

/*==============================================================*
 * Logical ID version of all the storage operations		*
 *==============================================================*/

/*--------------------------------------------------------------*
 *  ss_m::set_store_property()                                  *
 *--------------------------------------------------------------*/
rc_t
ss_m::set_store_property(
    const lvid_t&       lvid,
    const serial_t&     lfid,
    store_property_t	property)
{
    SM_PROLOGUE_RC(ss_m::set_store_property, in_xct, 0);
    SMSCRIPT(<<"set_store_property " <<lvid <<" " << lfid << " " << property);

    lid_t  id(lvid, lfid);
    stid_t stid;

    LID_CACHE_RETRY_DO(id, stid_t, stid, _set_store_property(stid, property) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::get_store_property()                                  *
 *--------------------------------------------------------------*/
rc_t
ss_m::get_store_property(
    const lvid_t&       lvid,
    const serial_t&     lfid,
    store_property_t&	property)
{
    SM_PROLOGUE_RC(ss_m::get_store_property, in_xct, 0);
    RES_SMSCRIPT(<<"get_store_property " <<lvid <<" " << lfid );

    lid_t  id(lvid, lfid);
    stid_t stid;
    LID_CACHE_RETRY_DO(id, stid_t, stid, _get_store_property(stid, property) );
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_file()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_file(const lvid_t& lvid, serial_t& lfid,
		  store_property_t property)
{
    SM_PROLOGUE_RC(ss_m::create_file, in_xct, 0);
    vid_t  vid;  // physical volume ID (returned by generate_new_serial)
    stid_t fid;  // physical file ID

    if (property == t_temporary)  {
	return RC(eNOLOGICALTEMPFILES);
    }

    W_DO(lid->generate_new_serials(lvid, vid, 1, lfid, lid_m::local_ref));

//TODO: begin rollback
DBG( << "TODO: put in rollback mechanism" );
    W_DO(_create_file(vid, fid, property, lfid));
    W_DO(lid->associate(lvid, lfid, fid));

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_file_id() - for Markos' tests                  *
 *--------------------------------------------------------------*/
rc_t
ss_m::create_file_id(const lvid_t& lvid, const serial_t& lfid,
		  extnum_t first_ext,
                  store_property_t property)
{
    SM_PROLOGUE_RC(ss_m::create_file, in_xct, 0);
    vid_t  vid;  // physical volume ID (returned by generate_new_serial)
    stid_t fid;  // physical file ID

    W_DO(lid->lookup(lvid, vid));

//TODO: begin rollback
DBG( << "TODO: put in rollback mechanism" );
    W_DO(_create_file_id(vid, first_ext, fid, property, lfid));
    W_DO(lid->associate(lvid, lfid, fid));

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::destroy_file()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_file(const lvid_t& lvid, const serial_t& lfid)
{
    SM_PROLOGUE_RC(ss_m::destroy_file, in_xct, 0);

    stid_t fid;  // physical file ID
    lid_t  id(lvid, lfid);

    // scan the file and remove all logical IDs
    LID_CACHE_RETRY_DO(id, stid_t, fid, _remove_file_lids(fid, lvid));

//TODO: begin rollback
DBG( << "TODO: put in rollback mechanism" );

    W_DO(_destroy_file(fid));
    W_DO(lid->remove(lvid, lfid));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_rec()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_rec(const lvid_t& lvid, const serial_t& lfid,
		 const vec_t& hdr, smsize_t len_hint,
		 const vec_t& data, serial_t& lrid)
{
    SM_PROLOGUE_RC(ss_m::create_rec, in_xct, 0);
    vid_t  vid;  // physical volume ID (returned by generate_new_serial)

    W_DO(lid->generate_new_serials(lvid, vid, 1, lrid, lid_m::local_ref));

//TODO: begin rollback
DBG( << "TODO: put in rollback mechanism" );
    W_DO(_create_rec_id(lvid, lfid, hdr, len_hint, data, lrid));
//whatabout rolling back
//what if another thread is runnin 

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_rec()						*
 *  for Janet Wiener's tests					*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_rec(const lvid_t& lvid, const serial_t& lfid,
                 const vec_t& hdr, smsize_t len_hint,
                 const vec_t& data, serial_t& lrid, rid_t& rid)
{
    SM_PROLOGUE_RC(ss_m::create_rec, in_xct, 0);
    vid_t  vid;  // physical volume ID (returned by generate_new_serial)

    W_DO(lid->generate_new_serials(lvid, vid, 1, lrid, lid_m::local_ref));

//TODO: begin rollback
DBG( << "TODO: put in rollback mechanism" );
    W_DO(_create_rec_id(lvid, lfid, hdr, len_hint, data, lrid, rid));
//whatabout rolling back
//what if another thread is runnin

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_id()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_id(const lvid_t& lvid, int id_count, serial_t& id_start)
{
    SM_PROLOGUE_RC(ss_m::create_id, in_xct, 0);
    vid_t  vid;  // physical volume ID (returned by generate_new_serial)

    W_DO(lid->generate_new_serials(lvid, vid, id_count,
				     id_start, lid_m::local_ref));

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_rec_id()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_rec_id(const lvid_t& lvid, const serial_t& lfid,
		 const vec_t& hdr, smsize_t len_hint,
		 const vec_t& data, const serial_t& lrid)
{
    SM_PROLOGUE_RC(ss_m::create_rec_id, in_xct, 0);

//TODO: begin rollback
DBG( << "TODO: put in rollback mechanism" );
    W_DO(_create_rec_id(lvid, lfid, hdr, len_hint, data, lrid));
//whatabout rolling back
//what if another thread is runnin 

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::create_rec_id()					*
 *  for Janet Wiener's and Markos' tests                 	*
 *--------------------------------------------------------------*/
rc_t
ss_m::create_rec_id(const lvid_t& lvid, const serial_t& lfid,
                 const vec_t& hdr, smsize_t len_hint,
                 const vec_t& data, const serial_t& lrid, rid_t& rid,
		 bool forward_alloc)
{
    SM_PROLOGUE_RC(ss_m::create_rec_id, in_xct, 0);

//TODO: begin rollback
DBG( << "TODO: put in rollback mechanism" );
    W_DO(_create_rec_id(lvid, lfid, hdr, len_hint, data, lrid, rid, forward_alloc));
//whatabout rolling back
//what if another thread is runnin

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_create_rec_id()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_create_rec_id(const lvid_t& lvid, const serial_t& lfid,
		 const vec_t& hdr, smsize_t len_hint,
		 const vec_t& data, const serial_t& lrid)
{
    rid_t  rid;  // physical record ID
    stid_t fid;  // physical file ID
    lid_t  id(lvid, lfid);

    LID_CACHE_RETRY_DO(id, stid_t, fid, _create_rec(fid, hdr, len_hint, data, rid, lrid));

    W_DO(lid->associate(lvid, lrid, rid));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_create_rec_id()					*
 *  for Janet Wiener's and Markos' tests			*
 *--------------------------------------------------------------*/
rc_t
ss_m::_create_rec_id(const lvid_t& lvid, const serial_t& lfid,
                 const vec_t& hdr, smsize_t len_hint,
                 const vec_t& data, const serial_t& lrid, rid_t& rid,
		 bool forward_alloc)
{
    stid_t fid;  // physical file ID
    lid_t  id(lvid, lfid);

    LID_CACHE_RETRY_DO(id, stid_t, fid, _create_rec(fid, hdr, len_hint, data, rid, lrid, forward_alloc));
    W_DO(lid->associate(lvid, lrid, rid));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::destroy_rec()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::destroy_rec(const lvid_t& lvid, const serial_t& lrid)
{
    SM_PROLOGUE_RC(ss_m::destroy_rec, in_xct, 0);
    rid_t rid;
    lid_t  id(lvid, lrid);

    LID_CACHE_RETRY_DO(id, rid_t, rid, _destroy_rec(rid, lrid));

//TODO: begin rollback
DBG( << "put in rollback mechanism" );
    W_DO(lid->remove(lvid, lrid));

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::update_rec()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::update_rec(const lvid_t& lvid, const serial_t& lrid, smsize_t start,
		 const vec_t& data)
{
    SM_PROLOGUE_RC(ss_m::update_rec, in_xct, 0);
    rid_t  rid;  // physical record ID
    lid_t  id(lvid, lrid);

    LID_CACHE_RETRY_DO(id, rid_t, rid, _update_rec(rid, start, data, lrid));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::update_rec_hdr()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::update_rec_hdr(const lvid_t& lvid, const serial_t& lrid,
		     smsize_t start, const vec_t& hdr)
{
    SM_PROLOGUE_RC(ss_m::update_rec_hdr, in_xct, 0);
    rid_t  rid;  // physical record ID
    lid_t  id(lvid, lrid);

    LID_CACHE_RETRY_DO(id, rid_t, rid, _update_rec_hdr(rid, start, hdr, lrid));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::append_rec()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::append_rec(const lvid_t& lvid, const serial_t& lrid, const vec_t& data)
{
    SM_PROLOGUE_RC(ss_m::append_rec, in_xct, 0);
    rid_t  rid;  // physical record ID
    lid_t  id(lvid, lrid);

    rc_t rc;
    LID_CACHE_RETRY(id, rid_t, rid, rc, _append_rec(rid, data, false, lrid));
    if (rc)  {
	if (rc.err_num() != eRECWONTFIT) return rc.reset();

	rid_t new_rid;
	W_DO(_forward_rec(lvid, lrid, rid, data, new_rid));
    }
    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::truncate_rec()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::truncate_rec(const lvid_t& lvid, const serial_t& lrid, smsize_t amount)
{
    SM_PROLOGUE_RC(ss_m::truncate_rec, in_xct, 0);
    rid_t  rid;  // physical record ID
    lid_t  id(lvid, lrid);
    bool should_forward;

    LID_CACHE_RETRY_DO(id, rid_t, rid, _truncate_rec(rid, amount, should_forward, lrid));
    if (should_forward) {
	//
	// The record is still implemented as large, even though
	// it could fit on a page (though not on the current one).
	// So, we take advantage of logical IDs and forward it.
	vec_t dummy;  // empty vector
	rid_t new_rid;
	W_DO(_forward_rec(lvid, lrid, rid, dummy, new_rid));
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::sort_file()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::sort_file(const lvid_t& lvid, const serial_t& serial,
		const lvid_t& s_lvid, serial_t& s_serial,
		store_property_t property,
		const key_info_t& key_info, int run_size,
		bool unique, bool destructive)
{
    SM_PROLOGUE_RC(ss_m::sort_file, in_xct, 0);
    stid_t in_fid; 
    stid_t out_fid; 
    vid_t  out_vid;
    lid_t  id(lvid, serial);

    LID_CACHE_RETRY_VALIDATE_STID_DO(id, in_fid);

    W_DO(lid->generate_new_serials(s_lvid, out_vid, 1, s_serial,
		lid_m::local_ref));
//TODO: begin rollback
DBG( << "TODO: put in rollback mechanism" );
    W_DO(_sort_file(in_fid, out_vid, out_fid, property, key_info, 
		run_size, unique, destructive, s_serial, s_lvid));
    W_DO(lid->associate(s_lvid, s_serial, out_fid));

    if (destructive) {
    	W_DO(lid->remove(s_lvid, serial));
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::Link_to_remote_id()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::link_to_remote_id(const lvid_t& local_lvid,
			serial_t& local_serial,
			const lvid_t& remote_lvid,
			const serial_t& remote_serial)
{
    SM_PROLOGUE_RC(ss_m::link_to_remote_id, in_xct, 0);
    vid_t vid;
    bool found;

    lid_t id(remote_lvid, remote_serial);
    W_DO(lid->check_duplicate_remote(id, local_lvid, local_serial, found));
    if (!found) {
	W_DO(lid->generate_new_serials(local_lvid, vid, 1, 
					 local_serial, lid_m::remote_ref));
	w_assert3(local_serial.is_remote()); 
	W_DO(lid->associate(local_lvid, local_serial,
			      remote_lvid, remote_serial));
    }
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::serial_to_stid()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::serial_to_stid(const lvid_t& lvid, const serial_t& serial, stid_t& stid)
{
    SM_PROLOGUE_RC(ss_m::serial_to_stid, in_xct, 0);
    lid_t  id(lvid, serial);
    W_DO(lid->lookup(id, stid));
    return RCOK;
}

rc_t
ss_m::lvid_to_vid(const lvid_t& lvid, vid_t& vid)
{
    SM_PROLOGUE_RC(ss_m::lvid_to_vid, can_be_in_xct, 0);
    RES_SMSCRIPT(<<"lvid_to_lvid " <<lvid );
    vid = io->get_vid(lvid);
    if (vid == vid_t::null) return RC(eBADVOL);
    return RCOK;
}

rc_t
ss_m::vid_to_lvid(vid_t vid, lvid_t& lvid)
{
    SM_PROLOGUE_RC(ss_m::lvid_to_vid, can_be_in_xct, 0);
    RES_SMSCRIPT(<<"vid_to_lvid " <<vid );
    lvid = io->get_lvid(vid);
    if (lvid == lvid_t::null) return RC(eBADVOL);
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::serial_to_rid()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::serial_to_rid(const lvid_t& lvid, const serial_t& serial, rid_t& rid)
{
    SM_PROLOGUE_RC(ss_m::serial_to_rid, in_xct, 0);
    RES_SMSCRIPT(<<"serial_to_rid " <<lvid <<" " <<serial);
    lid_t  id(lvid, serial);
    W_DO(lid->lookup(id, rid));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::convert_to_local_id()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::convert_to_local_id(const lvid_t& remote_v,
                          const serial_t& remote_s,
                          lvid_t& local_v, serial_t& local_s)
{
    SM_PROLOGUE_RC(ss_m::convert_to_local_id, in_xct, 0);
    RES_SMSCRIPT(<<"convert_to_local_id " <<remote_v <<" " <<remote_s);
    lid_t  id(remote_v, remote_s);
    W_DO(lid->lookup_local(id));
    local_v = id.lvid;
    local_s = id.serial;
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::lfid_of_lrid()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::lfid_of_lrid(const lvid_t& lvid, const serial_t& lrid,
                 serial_t& lfid)
{
    SM_PROLOGUE_RC(ss_m::lfid_of_lrid, in_xct, 0);
    RES_SMSCRIPT(<<"lfid_of_lrid " <<lvid <<" " <<lrid);
    rid_t 	rid;
    sdesc_t* 	sd;	// info on the store for this rid
    lid_t  id(lvid, lrid);

    W_DO(lid->lookup(id, rid));
    W_DO(dir->access(rid.pid.stid(), sd, IS));

    w_assert1(sd);

    lfid = sd->sinfo().logical_id;
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_set_store_property()                                 *
 *--------------------------------------------------------------*/
rc_t
ss_m::_set_store_property(
    stid_t              stid,
    store_property_t	property)
{
    store_flag_t oldflags = st_bad;
    store_flag_t newflags = _make_store_flag(property);

    /*
     * can't change to a load file
     */
    if (property & st_load_file)  {
	return RC(eBADSTOREFLAGS);
    }

    /*
     * find out the current property
     */

    W_DO( io->get_store_flags(stid, oldflags) );

    if (oldflags == newflags)  {
	return RCOK;
    }

    if (newflags == st_tmp)  {
	return RC(eBADSTOREFLAGS);
    }

    sdesc_t* sd;
    W_DO( dir->access(stid, sd, EX) );  // also locks the store in EX

    if (newflags == st_regular)  {
	/*
	 *  Set the io store flags for both stores and discard
	 *  the pages from the buffer pool.
	 *
	 *  Set the flags BEFORE forcing  the stores 
	 *  to handle this case: there's another thread
	 *  in this xct (the lock doesn't control concurrency
	 *  in this case); the other thread is reading pages in this 
	 *  file.
	 *  A page is read back in immediately after we forced it,
	 *  and its page flags are set when the read is done. 
	 *
	 */
	W_DO( io->set_store_flags(stid, newflags) );
	W_DO( bf->force_store(stid, true) );

	if (sd->large_stid())  {
	    W_DO( io->set_store_flags(sd->large_stid(), newflags) ) ;
	    W_DO( bf->force_store(sd->large_stid(), true) );
	}
    }  else if (newflags == st_insert_file)  {
	// only allow the changing for a regular file, not indices
	if (sd->sinfo().stype != t_file)  {
	    return RC(eBADSTOREFLAGS);
	}

	W_DO( io->set_store_flags(stid, newflags) );

	if (sd->large_stid())  {
	    W_DO( io->set_store_flags(sd->large_stid(), newflags) );
	}
    }  else  {
	W_FATAL(eINTERNAL);
    }

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_get_store_property()                                 *
 *--------------------------------------------------------------*/
rc_t
ss_m::_get_store_property(
    stid_t              stid,
    store_property_t&	property)
{
    store_flag_t flags = st_bad;
    W_DO( io->get_store_flags(stid, flags) );

    if (flags & st_tmp)  {
	property = t_temporary;
    } else if (flags & st_regular) {
	property = t_regular;
    } else if (flags & st_insert_file) {
	property = t_insert_file;
    } else {
	W_FATAL(eINTERNAL);
    }

    return RCOK;
}


/*--------------------------------------------------------------*
 *  ss_m::_create_file()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_create_file(vid_t vid, stid_t& fid,
		   store_property_t property,
		   const serial_t& logical_id)
{
    FUNC(ss_m::_create_file);
    DBG( << "Attempting to create a file on volume " << vid.vol );

    store_flag_t st_flag = _make_store_flag(property);

    DBGTHRD(<<"about to create a store");
    W_DO( io->create_store(vid, 100/*unused*/, st_flag, fid) );

    DBGTHRD(<<"created store " << fid);

    /*
    // create a store for holding large record pages 
    // Don't allocate any extents for it yet (num_exts == 0)
    */
    stid_t lg_stid;
    W_DO( io->create_store(vid, 100/*unused*/, st_flag, lg_stid, 0) );

    DBGTHRD(<<"created 2nd store (for lg recs) " << lg_stid);

    // Note: theoretically, some other thread could destroy
    // 	     the above stores before the following lock request
    // 	     is granted.  The only forseable way for this to
    //	     happen would be due to a bug in a vas causing
    //       it to destroy the wrong store.  We make no attempt
    //       to prevent this.
    W_DO(lm->lock(fid, EX, t_long, WAIT_SPECIFIED_BY_XCT));

    DBGTHRD(<<"locked " << fid);

    lpid_t first;
    W_DO( fi->create(fid, first) );
    DBGTHRD(<<"locked &created " << fid);

    sinfo_s sinfo(fid.store, t_file, 100/*unused*/, 
	   t_bad_ndx_t, t_cc_none/*not used*/, first.page, logical_id, 0, 0);
    sinfo.set_large_store(lg_stid.store);
    stpgid_t stpgid(fid);
    W_DO( dir->insert(stpgid, sinfo) );

    DBGTHRD(<<"inserted " << fid.store);

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_create_file_id()  - for Markos' tests                *
 *--------------------------------------------------------------*/
rc_t
ss_m::_create_file_id(vid_t vid, extnum_t first_ext, stid_t& fid,
                   store_property_t property,
                   const serial_t& logical_id)
{
    FUNC(ss_m::_create_file);
    DBG( << "Attempting to create a file on volume " << vid.vol );

    store_flag_t st_flag = _make_store_flag(property);

    W_DO( io->create_store(vid, 100/*unused*/, st_flag, fid, first_ext) );

    /*
    // create a store for holding large record pages
    // Don't allocate any extents yet; wait until a large object
    // is created
    */
    stid_t lg_stid;
    W_DO( io->create_store(vid, 100/*unused*/, st_flag, lg_stid, 0) );

    W_DO(lm->lock(fid, EX, t_long, WAIT_SPECIFIED_BY_XCT));

    lpid_t first;
    W_DO( fi->create(fid, first) );

    sinfo_s sinfo(fid.store, t_file, 100/*unused*/, 
		 t_bad_ndx_t, t_cc_none/*unused*/,
		 first.page, logical_id, 0, 0);
    sinfo.set_large_store(lg_stid.store);
    stpgid_t stpgid(fid);
    W_DO( dir->insert(stpgid, sinfo) );

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_destroy_file()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_destroy_file(const stid_t& fid)
{
    FUNC(ss_m::_destroy_file);

    sdesc_t* sd;

    DBGTHRD(<<"want to destroy store " << fid);
    W_DO( dir->access(fid, sd, EX) );

    if (sd->sinfo().stype != t_file) return RC(eBADSTORETYPE);

    store_flag_t store_flags;
    W_DO( io->get_store_flags(fid, store_flags) );
   
    DBGTHRD(<<"destroying store " << fid);
    W_DO( io->destroy_store(fid) );

    DBGTHRD(<<"destroying store " << sd->large_stid());
    // destroy the store containing large record pages
    W_DO( io->destroy_store(sd->large_stid()) );

    if (store_flags & st_tmp)  { // for temporary
	// we can discard the buffers to save
	// some disk I/O.
	W_IGNORE( bf->discard_store(fid) );
	W_IGNORE( bf->discard_store(sd->large_stid()) );
    }
    
    DBGTHRD(<<"about to remove directory entry " << fid);
    W_DO( dir->remove(fid) );
    DBGTHRD(<<"removed directory entry " << fid);
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_destroy_n_swap_file()				*
 *      destroy the old file and shift the large object store	*
 * 	to new file.						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_destroy_n_swap_file(const stid_t& old_fid, const stid_t& new_fid)
{

    sdesc_t *sd1, *sd2;

    W_DO( dir->access(old_fid, sd1, EX) );
    if (sd1->sinfo().stype != t_file) return RC(eBADSTORETYPE);

    W_DO( dir->access(new_fid, sd2, EX) );
    if (sd2->sinfo().stype != t_file) return RC(eBADSTORETYPE);

    store_flag_t old_store_flags = st_bad;
    store_flag_t new_store_flags = st_bad;
    W_DO( io->get_store_flags(old_fid, old_store_flags) );
    W_DO( io->get_store_flags(new_fid, new_store_flags) );
   
    // destroy the old
    W_DO( io->destroy_store(old_fid) );

    // destroy the store containing large record pages for new file
    W_DO( io->destroy_store(sd2->large_stid()) );

    if (old_store_flags & st_tmp)  { // for temporary and no-log files, 
	// we can discard the buffers to save
	// some disk I/O.
	W_IGNORE( bf->discard_store(old_fid) );
    }
    
    if (new_store_flags & st_tmp)  { // for temporary and no-log  files, 
	// we can discard the buffers to save
	// some disk I/O.
	W_IGNORE( bf->discard_store(sd2->large_stid()) );
    }

    // do the actual swap in directory
    W_DO( dir->remove_n_swap(old_fid, new_fid) );

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_remove_file_lids()					*
 *        Removes logical IDs for all records in a file.	*
 *--------------------------------------------------------------*/
rc_t
ss_m::_remove_file_lids(const stid_t& fid, const lvid_t& lvid)
{
    FUNC(ss_m::_remove_file_lids);
    /*
     * The basic idea is to scan the file to find the serial number
     * for each record.  The serial numbers are removed from the
     * volumes logical ID index.
     *
     * NOTE: The "scan" code was extracted from the implementation
     *       of class scan_file_i.
     */
    {
	sdesc_t* sd;
	W_DO( dir->access(fid, sd, EX) );
	if (sd->sinfo().stype != t_file) return RC(eBADSTORETYPE);
    }

    rid_t	curr_rid;	// current record ID
    bool	eof = false;
    file_p	page;		// current page being scanned
    record_t*	rec = NULL;

    W_DO(lm->lock(fid, EX, t_long, WAIT_SPECIFIED_BY_XCT));

    W_DO(fi->first_page(fid, curr_rid.pid));
    curr_rid.slot = 0;  // start at the page header object

    while (!eof) {
    	if (!page.is_fixed()) {
	    W_DO( page.fix(curr_rid.pid, LATCH_EX) );
	    w_assert1(page.is_fixed());
	}
	curr_rid.slot = page.next_slot(curr_rid.slot);

	// serializability problem in scan is avoided
	// by acquiring the EX lock right up front, above

        if (curr_rid.slot == 0) {
            // last slot, so go to next page
            page.unfix();
            W_DO (fi->next_page(curr_rid.pid, eof));
        } else {
	    W_DO( page.get_rec(curr_rid.slot, rec) );
	    W_DO(lid->remove(lvid, rec->tag.serial_no));
	    DBG( << "removing record (lvid,serial): " <<
		    lvid << " , " << rec->tag.serial_no );
        }
    }
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_create_rec()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_create_rec(const stid_t& fid, const vec_t& hdr, smsize_t len_hint, 
		 const vec_t& data, rid_t& new_rid,
		 const serial_t& serial, bool forward_alloc)
{
    FUNC(ss_m::_create_rec);
    sdesc_t* sd;
    W_DO( dir->access(fid, sd, IX) );

    DBG( << "create in fid " << fid << " data.size " << data.size());

    W_DO( fi->create_rec(fid, hdr, len_hint, data, serial, *sd, new_rid, forward_alloc) );
    // NOTE: new_rid need not be locked, since file_m::create_rec
    // locks it (actually, it locks the entire page)l
#ifdef DEBUG
    /*
      not a valid test.... e.g. file locked in EX (no lock for pid)
    lock_mode_t m_p;  // mode for page id
    rc = lm->query(new_rid.pid, m_p);
    w_assert3(m_p == EX);
    */
#endif

    //cout << "sm create_rec " << new_rid << " size(hdr, data) " << hdr.size() <<  " " << data.size() << endl;

    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_destroy_rec()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_destroy_rec(const rid_t& rid, const serial_t& verify)
{
    W_DO(lm->lock(rid, EX, t_long, WAIT_SPECIFIED_BY_XCT));
    W_DO(fi->destroy_rec(rid, verify));
    //cout << "sm destroy_rec " << rid << endl;
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_update_rec()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_update_rec(const rid_t& rid, smsize_t start, const vec_t& data, const serial_t& verify)
{
    W_DO(lm->lock(rid, EX, t_long, WAIT_SPECIFIED_BY_XCT));
    W_DO(fi->update_rec(rid, start, data, verify));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_update_rec_hdr()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_update_rec_hdr(const rid_t& rid, smsize_t start, const vec_t& hdr, const serial_t& verify)
{
    W_DO(lm->lock(rid, EX, t_long, WAIT_SPECIFIED_BY_XCT));
    W_DO(fi->splice_hdr(rid, u4i(start), hdr.size(), hdr, verify));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_append_rec()						*
 *--------------------------------------------------------------*/
rc_t
ss_m::_append_rec(const rid_t& rid, const vec_t& data, bool allow_forward, const serial_t& verify)
{
    sdesc_t* sd;
    W_DO( dir->access(rid.stid(), sd, IX) );
    //cout << "sm append_rec " << rid << " size " << data.size() << endl;

    W_DO(lm->lock(rid, EX, t_long, WAIT_SPECIFIED_BY_XCT));
    W_DO(fi->append_rec(rid, data, *sd, allow_forward, verify));
    return RCOK;
}

/*--------------------------------------------------------------*
 *  ss_m::_truncate_rec()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_truncate_rec(const rid_t& rid, smsize_t amount,
		    bool& should_forward, const serial_t& verify)
{
    W_DO(lm->lock(rid, EX, t_long, WAIT_SPECIFIED_BY_XCT));
    W_DO(fi->truncate_rec(rid, amount, should_forward, verify));
    return RCOK;
}

//
// Forward a record with a logical ID.  Data is the data
// for the append that caused the need for forwarding.
//
/*--------------------------------------------------------------*
 *  ss_m::_forward_rec()					*
 *--------------------------------------------------------------*/
rc_t
ss_m::_forward_rec(const lvid_t& lvid, const serial_t& lrid,
		    const rid_t& old_rid, const vec_t& data,
		    rid_t& new_rid)
{
    FUNC(ss_m::_forward_rec);

    DBG(<< "forwarding lvid,lrid:" << lvid << "." << lrid );

    // the record is small but won't fit without forwarding
    // instead, create a new record and change the logical id

    file_p    page;
    W_DO(fi->locate_page(old_rid, page, LATCH_EX));
    record_t* rec;
    W_DO( page.get_rec(old_rid.slot, rec) );
    w_assert3(rec);

    vec_t alldata;
    lgdata_p lgdata;  // in case record is large

    if (rec->is_small()) {
	alldata.put(rec->body(), rec->body_size());
	w_assert3(data.size() > 0);  // must be appending to small rec
	alldata.put(data);  // append data to vector
    } else {
	w_assert3(data.size() == 0);  // must be truncating a large rec

	// get the last page of the record and have the alldata vector
	// point to it
	W_DO( lgdata.fix(rec->last_pid(page), LATCH_SH) );
	alldata.put(lgdata.tuple_addr(0), lgdata.tuple_size(0));
    }

    vec_t hdr(rec->hdr(), rec->hdr_size());
    smsize_t len_hint = 0;  // hint that it is small

    sdesc_t* sd;
    W_DO( dir->access(old_rid.stid(), sd, IX) );

    // NOTE: old_rid has already been locked, 
    // since _forward_rec only
    // called by _append_rec or pin_i::append_rec
#ifdef DEBUG
    lock_mode_t m_r;  // mode for record id
    W_DO( lm->query(old_rid, m_r, xct()->tid(), true) );
    DBG(<< "lock mgr says for old_rid " << old_rid 
		<< ": m_r=" << m_r
	);
    w_assert3(m_r == EX);
#endif
    W_DO( fi->create_rec(old_rid.stid(), hdr, len_hint, 
			   alldata, lrid, *sd, new_rid) );
    W_DO( fi->destroy_rec(old_rid, serial_t::null) );

    // replace the logical ID entry
    W_DO(lid->associate(lvid, lrid, new_rid, true/*replace*/));

    return RCOK;
}

