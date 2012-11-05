/*<std-header orig-src='shore'>

 $Id: create_rec.cpp,v 1.3 2010/06/08 22:28:15 nhall Exp $

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

/**\anchor multi_rooted_btrees_example */

// This program is a test of multi-rooted btrees

#include <w_stream.h>
#include <sys/types.h>
#include <cassert>
#include "sm_vas.h"
#include "w_getopt.h"
#include <set>
ss_m* ssm = 0;

// shorten error code type name
typedef w_rc_t rc_t;

// this is implemented in options.cpp
w_rc_t init_config_options(option_group_t& options,
                        const char* prog_type,
                        int& argc, char** argv);


struct file_info_t {
    static const char* key;
    stid_t         fid;
    rid_t       first_rid;
    int         num_rec;
    int         rec_size;
};
const char* file_info_t::key = "SCANFILE";

ostream &
operator << (ostream &o, const file_info_t &info)
{
    o << "key " << info.key
    << " fid " << info.fid
    << " first_rid " << info.first_rid
    << " num_rec " << info.num_rec
    << " rec_size " << info.rec_size ;
    return o;
}


typedef        smlevel_0::smksize_t        smksize_t;


struct el_filler2 : public ss_m::el_filler {
    int j;
    stid_t _fid;
    smsize_t _rec_size;
    rid_t rid;
    rc_t fill_el(vec_t& el, const lpid_t& leaf);
};

rc_t  el_filler2::fill_el(vec_t& el, const lpid_t& leaf) {
    cout << "Creating record " << j << endl;

    char* dummy = new char[_rec_size];
    memset(dummy, '\0', _rec_size);
    vec_t data(dummy, sizeof(int));

    {
	w_ostrstream o(dummy, _rec_size);
	o << j << ends;
	w_assert1(o.c_str() == dummy);
    }
    // header contains record #
    int i = j;
    const vec_t hdr(&i, sizeof(i));

    rc_t rc = ss_m::find_page_and_create_mrbt_rec(_fid, leaf, hdr, sizeof(int), data, rid);
    cout << "rid: " << rid << endl;

    el.put((char*)(&rid), sizeof(rid_t));
    
    return rc;
}

void
usage(option_group_t& options)
{
    cerr << "Usage: create_rec [-h] [-i] [options]" << endl;
    cerr << "       -i initialize device/volume and create file of records" << endl;
    cerr << "Valid options are: " << endl;
    options.print_usage(true, cerr);
}

/* create an smthread based class for all sm-related work */
class smthread_user_t : public smthread_t {
    int        _argc;
    char        **_argv;
    
    const char *_device_name;
    smsize_t    _quota;
    int         _num_rec;
    smsize_t    _rec_size;
    lvid_t      _lvid;  
    rid_t       _start_rid;
    stid_t      _fid;
    bool        _initialize_device;
    option_group_t* _options;
    vid_t       _vid;
    int         _test_no;
public:
    int         retval;
    
    smthread_user_t(int ac, char **av) 
	: smthread_t(t_regular, "smthread_user_t"),
	  _argc(ac), _argv(av), 
	  _device_name(NULL),
	  _quota(0),
	  _num_rec(0),
	  _rec_size(0),
	  _initialize_device(false),
	  _options(NULL),
	  _vid(1),
	  _test_no(0),
	  retval(0) { }
    
    ~smthread_user_t()  { if(_options) delete _options; }
    
    void run();
    
    // helpers for run()
    w_rc_t handle_options();
    w_rc_t find_file_info();
    w_rc_t create_the_file();
    // -- mrbt
    w_rc_t mr_index_test0();
    w_rc_t mr_index_test1();
    w_rc_t mr_index_test2();
    w_rc_t mr_index_test3();
    w_rc_t mr_index_test4();
    w_rc_t mr_index_test5();
    w_rc_t mr_index_test6();
    w_rc_t insert_rec_to_index(stid_t stid);
    w_rc_t print_the_mr_index(stid_t stid);
    w_rc_t static print_updated_rids(const stid_t& stid, vector<rid_t>& old_rids, vector<rid_t>& new_rids);
    // --
    w_rc_t scan_the_file();
    w_rc_t scan_the_root_index();
    w_rc_t do_work();
    w_rc_t do_init();
    w_rc_t no_init();

};

/*
 * looks up file info in the root index
*/
w_rc_t
smthread_user_t::find_file_info()
{
    file_info_t  info;
    W_DO(ssm->begin_xct());

    bool        found;
    stid_t      _root_iid;
    W_DO(ss_m::vol_root_index(_vid, _root_iid));

    smsize_t    info_len = sizeof(info);
    const vec_t key_vec_tmp(file_info_t::key, strlen(file_info_t::key));
    W_DO(ss_m::find_assoc(_root_iid,
                          key_vec_tmp,
                          &info, info_len, found));
    if (!found) {
        cerr << "No file information found" <<endl;
        return RC(fcASSERT);
    } else {
       cerr << " found assoc "
                << file_info_t::key << " --> " << info << endl;
    }

    W_DO(ssm->commit_xct());

    _start_rid = info.first_rid;
    _fid = info.fid;
    _rec_size = info.rec_size;
    _num_rec = info.num_rec;
    return RCOK;
}

/*
 * This function either formats a new device and creates a
 * volume on it, or mounts an already existing device and
 * returns the ID of the volume on it.
 *
 * It's borrowed from elsewhere; it can handle mounting
 * an already existing device, even though in this main program
 * we don't ever do that.
 */
rc_t
smthread_user_t::create_the_file() 
{
    file_info_t info;  // will be made persistent in the
    // volume root index.

    // create and fill file to scan
    cout << "Creating a file with " << _num_rec 
        << " records of size " << _rec_size << endl;
    W_DO(ssm->begin_xct());

    // Create the file. Stuff its fid in the persistent file_info
    W_DO(ssm->create_mrbt_file(_vid, info.fid, smlevel_3::t_regular));
    rid_t rid;

    _rec_size -= align(sizeof(int));

/// each record will have its ordinal number in the header
/// and zeros for data 
/*
    char* dummy = new char[_rec_size];
    memset(dummy, '\0', _rec_size);
    vec_t data(dummy, sizeof(int));
    int j = 1;
    for(; j < _num_rec; j++)
    {
        {
            w_ostrstream o(dummy, sizeof(int));
            o << j << ends;
            w_assert1(o.c_str() == dummy);
        }
        // header contains record #
        int i = j;
        const vec_t hdr(&i, sizeof(i));
        W_COERCE(ssm->create_mrbt_rec(info.fid, hdr,
				      data.size(), data, rid));
        cout << "Creating rec " << j << endl;
        if (j == 0) {
            info.first_rid = rid;
        }        
    }
*/  
    //cout << "Created all. First rid " << info.first_rid << endl;
    //   delete [] dummy;
    info.first_rid = rid;
    info.num_rec = _num_rec;
    info.rec_size = _rec_size;

    // record file info in the root index : this stores some
    // attributes of the file in general
    stid_t      _root_iid;
    W_DO(ss_m::vol_root_index(_vid, _root_iid));

    const vec_t key_vec_tmp(file_info_t::key, strlen(file_info_t::key));
    const vec_t info_vec_tmp(&info, sizeof(info));
    W_DO(ss_m::create_assoc(_root_iid,
                            key_vec_tmp,
                            info_vec_tmp));
    cerr << "Creating assoc "
            << file_info_t::key << " --> " << info << endl;
    W_DO(ssm->commit_xct());
    return RCOK;
}

// -- mrbt
rc_t smthread_user_t::print_updated_rids(const stid_t& stid, vector<rid_t>& old_rids, vector<rid_t>& new_rids)
{
  cout << endl;
  cout << "Index store id: " << stid << endl;
  cout << "Old rids\tNew rids" << endl;
  for(uint i=0; i<old_rids.size(); i++) {
    cout << old_rids[i] << "\t" << new_rids[i] << endl;
  }

  return RCOK;
}

rc_t smthread_user_t::mr_index_test0()
{
    cout << endl;
    cout << " ------- TEST0 -------" << endl;
    cout << "To test MRBtree with single partition!" << endl;
    cout << endl;
    

    cout << "Creating multi rooted btree index." << endl;
    stid_t stid;
    W_DO(ssm->create_mr_index(_vid, smlevel_0::t_mrbtree_p, smlevel_3::t_regular, 
			      "i4", smlevel_0::t_cc_kvl, stid));

    
    W_DO(insert_rec_to_index(stid));

    
    W_DO(print_the_mr_index(stid));

    
    return RCOK;
}

rc_t smthread_user_t::mr_index_test1()
{
    cout << endl;
    cout << " ------- TEST1 -------" << endl;
    cout << "To test adding inital partitions to MRBtree!" << endl;
    cout << endl;

    
    cout << "Creating multi rooted btree index." << endl;
    stid_t stid;    
    W_DO(ssm->create_mr_index(_vid, smlevel_0::t_mrbtree_p, smlevel_3::t_regular, 
			      "i4", smlevel_0::t_cc_kvl, stid));


    int key1 = 700;
    cvec_t key_vec1((char*)(&key1), sizeof(key1));
    cout << "ssm->add_partition_init(stid = " << stid
	 << ", key = " << key1 << endl;
    W_DO(ssm->add_partition_init(stid, key_vec1));
    
    
    int key2 = 500;
    cvec_t key_vec2((char*)(&key2), sizeof(key2));
    cout << "ssm->add_partition_init(stid = " << stid
	 << ", key = " << key2 << endl;
    W_DO(ssm->add_partition_init(stid, key_vec2));


    W_DO(insert_rec_to_index(stid));

    
    W_DO(print_the_mr_index(stid));

    
    return RCOK;
}

rc_t smthread_user_t::mr_index_test2()
{
    cout << endl;
    cout << " ------- TEST2 -------" << endl;
    cout << "Create a partition after the some assocs are created in MRBtree" << endl;
    cout << "Then add new assocs" << endl;
    cout << "Tests split tree for MRBtree when each heap-page is pointed by only one sub-btree" << endl;
    cout << endl;

    
    cout << "Creating multi rooted btree index." << endl;
    stid_t stid;
    W_DO(ssm->create_mr_index(_vid, smlevel_0::t_mrbtree_p, smlevel_3::t_regular, 
			      "i4", smlevel_0::t_cc_kvl, stid));

    
    W_DO(insert_rec_to_index(stid));


    W_DO(print_the_mr_index(stid));

    int key = 700;
    cvec_t key_vec((char*)(&key), sizeof(key));
    cout << "ssm->add_partition(stid = " << stid
	 << ", key_vec = " << key << endl;
    W_DO(ssm->add_partition(stid, key_vec, false, &print_updated_rids));
    

    W_DO(print_the_mr_index(stid));

    
     // create two more assocs
    W_DO(ssm->begin_xct());
    cout << "ssm->create_mr_assoc" << endl;
    el_filler2 eg;
    eg._fid = _fid;
    int new_key = 1000;
    eg.j = new_key;
    cvec_t new_key_vec((char*)(&new_key), sizeof(new_key));
    cout << "Record key "  << new_key << endl;
    cout << "key size " << new_key_vec.size() << endl;    
    eg._el_size = sizeof(rid_t);
    W_DO(ssm->create_mr_assoc(stid, new_key_vec, eg, false, &print_updated_rids));

    cout << "ssm->create_mr_assoc" << endl;
    eg._el.reset();
    int new_key2 = 0;
    eg.j = new_key2;
    cvec_t new_key_vec2((char*)(&new_key2), sizeof(new_key2));
    cout << "Record key "  << new_key2 << endl;
    cout << "key size " << new_key_vec2.size() << endl;    
    W_DO(ssm->create_mr_assoc(stid, new_key_vec2, eg, false, &print_updated_rids));
    W_DO(ssm->commit_xct()); 


    W_DO(print_the_mr_index(stid));


    W_DO(scan_the_file());

    
    return RCOK;
}

rc_t smthread_user_t::mr_index_test3()
{
    cout << endl;
    cout << " ------- TEST3 -------" << endl;
    cout << "Tests split_tree where each heap page is pointed by one sub_btree mode and merging trees that have the same level" << endl;
    cout << endl;
    
    
    cout << "Creating multi rooted btree index." << endl;
    stid_t stid;
    W_DO(ssm->create_mr_index(_vid, smlevel_0::t_mrbtree_p, smlevel_3::t_regular, 
			      "i4", smlevel_0::t_cc_kvl, stid));

    
    W_DO(insert_rec_to_index(stid));


    W_DO(print_the_mr_index(stid));

    
    int key1 = 200;
    cvec_t key1_vec((char*)(&key1), sizeof(key1));
    cout << "ssm->add_partition(stid = " << stid
	 << ", key_vec = " << key1 << endl;
    W_DO(ssm->add_partition(stid, key1_vec, false, &print_updated_rids));

    
    W_DO(print_the_mr_index(stid));


    int key2 = 400;
    cvec_t key2_vec((char*)(&key2), sizeof(key2));
    cout << "ssm->add_partition(stid = " << stid
	 << ", key_vec = " << key2 << endl;
    W_DO(ssm->add_partition(stid, key2_vec, false, &print_updated_rids));


    W_DO(print_the_mr_index(stid));


    int key3 = 600;
    cvec_t key3_vec((char*)(&key3), sizeof(key3));
    cout << "ssm->add_partition(stid = " << stid
	 << ", key_vec = " << key3 << endl;
    W_DO(ssm->add_partition(stid, key3_vec, false, &print_updated_rids));

    
    W_DO(print_the_mr_index(stid));


    int key4 = 800;
    cvec_t key4_vec((char*)(&key4), sizeof(key4));
    cout << "ssm->add_partition(stid = " << stid
	 << ", key_vec = " << key4 << endl;
    W_DO(ssm->add_partition(stid, key4_vec, false, &print_updated_rids));

    
    W_DO(print_the_mr_index(stid));


    cout << "ssm->delete_partition(stid = " << stid
	 << ", key_vec = " << key1 << endl;
    W_DO(ssm->delete_partition(stid, key1_vec));


    W_DO(print_the_mr_index(stid));


    int key5 = 700;
    cvec_t key5_vec((char*)(&key5), sizeof(key5));
    cout << "ssm->delete_partition(stid = " << stid
	 << ", key_vec = " << key5 << endl;
    W_DO(ssm->delete_partition(stid, key5_vec));

    
    W_DO(print_the_mr_index(stid));

    
    int key6 = 500;
    cvec_t key6_vec((char*)(&key6), sizeof(key6));
    cout << "ssm->delete_partition(stid = " << stid
	 << ", key_vec = " << key6 << endl;
    W_DO(ssm->delete_partition(stid, key6_vec));

    
    W_DO(print_the_mr_index(stid));


    return RCOK;
}

rc_t smthread_user_t::mr_index_test4()
{
    cout << endl;
    cout << " ------- TEST4 -------" << endl;
    cout << "1. Make initial partitions" << endl; 
    cout << "2. Bulk load the recs to MRBtree index" << endl;
    cout << "To test bulk loading from files!" << endl;
    cout << endl;

    
    cout << "Creating multi rooted btree index." << endl;
    stid_t stid;    
    W_DO(ssm->create_mr_index(_vid, smlevel_0::t_mrbtree_p, smlevel_3::t_regular, 
			      "i4", smlevel_0::t_cc_kvl, stid));


    int key1 = 700;
    cvec_t key_vec1((char*)(&key1), sizeof(key1));
    cout << "ssm->add_partition_init(stid = " << stid
	 << ", key = " << key1 << endl;
    W_DO(ssm->add_partition_init(stid, key_vec1));
    
    
    int key2 = 500;
    cvec_t key_vec2((char*)(&key2), sizeof(key2));
    cout << "ssm->add_partition_init(stid = " << stid
	 << ", key = " << key2 << endl;
    W_DO(ssm->add_partition_init(stid, key_vec2));


    W_DO(ssm->begin_xct());
    sm_du_stats_t stats;
    cout << "ssm->bulkld_mr_index(stid = " << stid
	 << ", file = " << _fid << endl;
    // TODO: update
    W_DO(ssm->bulkld_mr_index(stid, _fid, stats));
    W_DO(ssm->commit_xct());

    
    W_DO(print_the_mr_index(stid));

    
    return RCOK;
}

rc_t smthread_user_t::mr_index_test5()
{
    cout << endl;
    cout << " ------- TEST5 -------" << endl;
    cout << " 1. Add initial partitions " << endl;
    cout << "To test merge partitions when root1,level > root2.level in MRBtree!" << endl;
    cout << endl;

    
    cout << "Creating multi rooted btree index." << endl;
    stid_t stid;    
    W_DO(ssm->create_mr_index(_vid, smlevel_0::t_mrbtree_p, smlevel_3::t_regular, 
			      "i4", smlevel_0::t_cc_kvl, stid));


    int key1 = 700;
    cvec_t key_vec1((char*)(&key1), sizeof(key1));
    cout << "ssm->add_partition_init(stid = " << stid
	 << ", key = " << key1 << endl;
    W_DO(ssm->add_partition_init(stid, key_vec1));
    
    
    int key2 = 500;
    cvec_t key_vec2((char*)(&key2), sizeof(key2));
    cout << "ssm->add_partition_init(stid = " << stid
	 << ", key = " << key2 << endl;
    W_DO(ssm->add_partition_init(stid, key_vec2));


    W_DO(insert_rec_to_index(stid));

    
    W_DO(print_the_mr_index(stid));


    int key3 = 600;
    cvec_t key3_vec((char*)(&key3), sizeof(key3));
    cout << "ssm->delete_partition(stid = " << stid
	 << ", key = " << key3 << endl;
    W_DO(ssm->delete_partition(stid, key3_vec));


    W_DO(print_the_mr_index(stid));

    
    return RCOK;
}

rc_t smthread_user_t::mr_index_test6()
{
    cout << endl;
    cout << " ------- TEST6 -------" << endl;
    cout << " 1. Add initial partitions " << endl;
    cout << "To test merge partitions when root1,level < root2.level in MRBtree!" << endl;
    cout << endl;

    
    cout << "Creating multi rooted btree index." << endl;
    stid_t stid;    
    W_DO(ssm->create_mr_index(_vid, smlevel_0::t_mrbtree_p, smlevel_3::t_regular, 
			      "i4", smlevel_0::t_cc_kvl, stid));


    int key1 = 700;
    cvec_t key_vec1((char*)(&key1), sizeof(key1));
    cout << "ssm->add_partition_init(stid = " << stid
	 << ", key = " << key1 << endl;
    W_DO(ssm->add_partition_init(stid, key_vec1));
    
    
    int key2 = 500;
    cvec_t key_vec2((char*)(&key2), sizeof(key2));
    cout << "ssm->add_partition_init(stid = " << stid
	 << ", key = " << key2 << endl;
    W_DO(ssm->add_partition_init(stid, key_vec2));


    W_DO(insert_rec_to_index(stid));

    
    W_DO(print_the_mr_index(stid));


    int key3 = 800;
    cvec_t key3_vec((char*)(&key3), sizeof(key3));
    cout << "ssm->delete_partition(stid = " << stid
	 << ", key = " << key3 << endl;
    W_DO(ssm->delete_partition(stid, key3_vec));


    W_DO(print_the_mr_index(stid));

    
    return RCOK;
}

// Modified the code for scan_the_file
rc_t smthread_user_t::insert_rec_to_index(stid_t stid)
{
    cout << "creating assocs in index " << stid << "  from file " << _fid << endl;
  W_DO(ssm->begin_xct());

  scan_file_i scan(_fid);
  pin_i*      cursor(NULL);
  bool        eof(false);
  int         i(1);
  //set<int>    inserted;
  
  do {
      el_filler2 eg;
      eg._fid = _fid;
      eg._rec_size = _rec_size;
      eg._el_size = sizeof(rid_t);
      //   w_rc_t rc = scan.next(cursor, 0, eof);
      //if(rc.is_error()) {
      // cerr << "Error getting next: " << rc << endl;
      //retval = rc.err_num();
      //return rc;
      //}
      //if(i == 999 || eof) break;
      int j = i;
    cout << "Record " << i << "/" << _num_rec << endl;
    cvec_t       key(&j, sizeof(j));
    //int         hdrcontents;
    //key.copy_to(&i, sizeof(i));
    //if(inserted.find(hdrcontents) == inserted.end() && hdrcontents != 0 && hdrcontents != 1000) {
	cout << "Key: "  << i << endl;
	cout << "key size " << key.size() << endl;
	//vec_t el((char*)(&cursor->rid()), sizeof(cursor->rid()));
	//cout << "El: " << cursor->rid() << endl;
	//cout << "El size "  << el.size() << endl;
	
	//const char *body = cursor->body();
	//w_assert1(cursor->body_size() == _rec_size);
	//cout << "Record body "  << body << endl;
	eg.j = i;
	W_DO(ssm->create_mr_assoc(stid, key, eg, false, &print_updated_rids));
	cout << endl;
	//inserted.insert(hdrcontents);
	//}
	i++;
  } while (i < 1000);
  //w_assert1(i == _num_rec-1);

  W_DO(ssm->commit_xct());
  return RCOK;
}

rc_t smthread_user_t::print_the_mr_index(stid_t stid) 
{
    cout << "printing the mr index from store " << stid << endl;
    W_DO(ssm->begin_xct());
     
    W_DO(ssm->print_mr_index(stid));

    W_DO(ssm->commit_xct());
     
    return RCOK;
}
   
// --

rc_t
smthread_user_t::scan_the_root_index() 
{
    W_DO(ssm->begin_xct());
    stid_t _root_iid;
    W_DO(ss_m::vol_root_index(_vid, _root_iid));
    cout << "Scanning index " << _root_iid << endl;
    scan_index_i scan(_root_iid, 
            scan_index_i::ge, vec_t::neg_inf,
            scan_index_i::le, vec_t::pos_inf, false,
            ss_m::t_cc_kvl);
    bool        eof(false);
    int         i(0);
    smsize_t    klen(0);
    smsize_t    elen(0);
#define MAXKEYSIZE 100
    char *      keybuf[MAXKEYSIZE];
    file_info_t info;

    do {
        w_rc_t rc = scan.next(eof);
        if(rc.is_error()) {
            cerr << "Error getting next: " << rc << endl;
            retval = rc.err_num();
            return rc;
        }
        if(eof) break;

        // get the key len and element len
        W_DO(scan.curr(NULL, klen, NULL, elen));
        // Create vectors for the given lengths.
        vec_t key(keybuf, klen);
        vec_t elem(&info, elen);
        // Get the key and element value
        W_DO(scan.curr(&key, klen, &elem, elen));

        cout << "Key " << keybuf << endl;
        cout << "Value " 
        << " { fid " << info.fid 
        << " first_rid " << info.first_rid
        << " #rec " << info.num_rec
        << " rec size " << info.rec_size << " }"
        << endl;
        i++;
    } while (!eof);
    W_DO(ssm->commit_xct());
    return RCOK;
}

rc_t
smthread_user_t::scan_the_file() 
{
    cout << "Scanning file " << _fid << endl;
    W_DO(ssm->begin_xct());

    scan_file_i scan(_fid);
    pin_i*      cursor(NULL);
    bool        eof(false);
    int         i(0);

    do {
        w_rc_t rc = scan.next(cursor, 0, eof);
        if(rc.is_error()) {
            cerr << "Error getting next: " << rc << endl;
            retval = rc.err_num();
            return rc;
        }
        if(eof) break;

        cout << "Record " << i << "/" << _num_rec
            << " Rid "  << cursor->rid() << endl;
        vec_t       header (cursor->hdr(), cursor->hdr_size());
        int         hdrcontents;
        header.copy_to(&hdrcontents, sizeof(hdrcontents));
        cout << "Record hdr "  << hdrcontents << endl;

        const char *body = cursor->body();
        //w_assert1(cursor->body_size() == _rec_size);
        cout << "Record body "  << body << endl;
	cout << "Record body size " << cursor->body_size() << endl;
        i++;
    } while (!eof);
    //    w_assert1(i == _num_rec+1);

    W_DO(ssm->commit_xct());
    return RCOK;
}

rc_t
smthread_user_t::do_init()
{
    cout << "-i: Initialize " << endl;

    {
        devid_t        devid;
        cout << "Formatting device: " << _device_name 
             << " with a " << _quota << "KB quota ..." << endl;
        W_DO(ssm->format_dev(_device_name, _quota, true));

        cout << "Mounting device: " << _device_name  << endl;
        // mount the new device
        u_int        vol_cnt;
        W_DO(ssm->mount_dev(_device_name, vol_cnt, devid));

        cout << "Mounted device: " << _device_name  
             << " volume count " << vol_cnt
             << " device " << devid
             << endl;

        // generate a volume ID for the new volume we are about to
        // create on the device
        cout << "Generating new lvid: " << endl;
        W_DO(ssm->generate_new_lvid(_lvid));
        cout << "Generated lvid " << _lvid <<  endl;

        // create the new volume 
        cout << "Creating a new volume on the device" << endl;
        cout << "    with a " << _quota << "KB quota ..." << endl;

        W_DO(ssm->create_vol(_device_name, _lvid, _quota, false, _vid));
        cout << "    with local handle(phys volid) " << _vid << endl;

    } 

    W_DO(create_the_file());
    return RCOK;
}

rc_t
smthread_user_t::no_init()
{
    cout << "Using already-existing device: " << _device_name << endl;
    // mount already existing device
    devid_t      devid;
    u_int        vol_cnt;
    w_rc_t rc = ssm->mount_dev(_device_name, vol_cnt, devid);
    if (rc.is_error()) {
        cerr << "Error: could not mount device: " 
            << _device_name << endl;
        cerr << "   Did you forget to run the server with -i?" 
            << endl;
        return rc;
    }
    
    // find ID of the volume on the device
    lvid_t* lvid_list;
    u_int   lvid_cnt;
    W_DO(ssm->list_volumes(_device_name, lvid_list, lvid_cnt));
    if (lvid_cnt == 0) {
        cerr << "Error, device has no volumes" << endl;
        exit(1);
    }
    _lvid = lvid_list[0];
    delete [] lvid_list;

    W_COERCE(find_file_info());
    W_COERCE(scan_the_root_index());
    W_DO(scan_the_file());
    switch(_test_no) {
    case 0:
	W_DO(mr_index_test0()); // ok
	break;
    case 1:
	W_DO(mr_index_test1()); // ok
	break;
    case 2:
	W_DO(mr_index_test2()); // ok
	break;
    case 3:
	W_DO(mr_index_test3()); // ok
	break;
    case 4:
	//W_DO(mr_index_test4()); //
	break;
    case 5:
	W_DO(mr_index_test5()); // ok
	break;
    case 6:
	W_DO(mr_index_test6()); // ok
	break;
    }
    return RCOK;
}

rc_t
smthread_user_t::do_work()
{
    if (_initialize_device) {
      cout << "do init" << endl;
      W_DO(do_init());
    }
    else  
      W_DO(no_init());
    return RCOK;
}

/**\defgroup EGOPTIONS Example of setting up options.
 * This method creates configuration options, starts up
 * the storage manager,
 */
w_rc_t smthread_user_t::handle_options()
{
    option_t* opt_device_name = 0;
    option_t* opt_device_quota = 0;
    option_t* opt_num_rec = 0;

    cout << "Processing configuration options ..." << endl;

    // Create an option group for my options.
    // I use a 3-level naming scheme:
    // executable-name.server.option-name
    // Thus, the file will contain lines like this:
    // create_rec.server.device_name : /tmp/example/device
    // *.server.device_name : /tmp/example/device
    // create_rec.*.device_name : /tmp/example/device
    //
    const int option_level_cnt = 3; 

    _options = new option_group_t (option_level_cnt);
    if(!_options) {
        cerr << "Out of memory: could not allocate from heap." <<
            endl;
        retval = 1;
        return RC(fcINTERNAL);
    }
    option_group_t &options(*_options);

    W_COERCE(options.add_option("device_name", "device/file name",
                         NULL, "device containg volume holding file to scan",
                         true, option_t::set_value_charstr,
                         opt_device_name));

    W_COERCE(options.add_option("device_quota", "# > 1000",
                         "2000", "quota for device",
                         false, option_t::set_value_long,
                         opt_device_quota));

    // Default number of records to create is 1.
    W_COERCE(options.add_option("num_rec", "# > 0",
                         "1", "number of records in file",
                         true, option_t::set_value_long,
                         opt_num_rec));

    // Have the SSM add its options to my group.
    W_COERCE(ss_m::setup_options(&options));

    cout << "Finding configuration option settings." << endl;

    w_rc_t rc = init_config_options(options, "server", _argc, _argv);
    if (rc.is_error()) {
        usage(options);
        retval = 1;
        return rc;
    }
    cout << "Processing command line." << endl;

    // Process the command line: looking for the "-h" flag
    int option;
    while ((option = getopt(_argc, _argv, "hi0123456")) != -1) {
        switch (option) {
        case 'i' :
            _initialize_device = true;
            break;

        case 'h' :
            usage(options);
            break;

	case '0':
	    _test_no = 0;
	    break;
	    
	case '1':
	    _test_no = 1;
	    break;
	    
	case '2':
	    _test_no = 2;
	    break;
	    
	case '3':
	    _test_no = 3;
	    break;
	    
	case '4':
	    _test_no = 4;
	    break;
	    
	case '5':
	    _test_no = 5;
	    break;
		    
	case '6':
	    _test_no = 6;
	    break;
	    
        default:
            usage(options);
            retval = 1;
            return RC(fcNOTIMPLEMENTED);
            break;
        }
    }

    if(!_initialize_device) 
	cout << "TEST" << _test_no << endl;
    
    {
        cout << "Checking for required options...";
        /* check that all required options have been set */
        w_ostrstream      err_stream;
        w_rc_t rc = options.check_required(&err_stream);
        if (rc.is_error()) {
            cerr << "These required options are not set:" << endl;
            cerr << err_stream.c_str() << endl;
            return rc;
        }
        cout << "Options OK; values are: { " << endl;
        options.print_values(false, cout);
        cout << "} end list of options values. " << endl;
    }

    // Grab the options values for later use by run()
    _device_name = opt_device_name->value();
    _quota = strtol(opt_device_quota->value(), 0, 0);
    _num_rec = strtol(opt_num_rec->value(), 0, 0);

    return RCOK;
}

void smthread_user_t::run()
{
    w_rc_t rc = handle_options();
    if(rc.is_error()) {
        retval = 1;
        return;
    }

    // Now start a storage manager.
    cout << "Starting SSM and performing recovery ..." << endl;
    ssm = new ss_m();
    if (!ssm) {
        cerr << "Error: Out of memory for ss_m" << endl;
        retval = 1;
        return;
    }

    cout << "Getting SSM config info for record size ..." << endl;

    sm_config_info_t config_info;
    W_COERCE(ss_m::config_info(config_info));
    _rec_size = config_info.max_small_rec; // minus a header

    // Subroutine to set up the device and volume and
    // create the num_rec records of rec_size.
    rc = do_work();

    if (rc.is_error()) {
        cerr << "Could not set up device/volume due to: " << endl;
        cerr << rc << endl;
        delete ssm;
        rc = RCOK;   // force deletion of w_error_t info hanging off rc
                     // otherwise a leak for w_error_t will be reported
        retval = 1;
        if(rc.is_error()) 
            W_COERCE(rc); // avoid error not checked.
        return;
    }

    sm_stats_info_t       stats;
    W_COERCE(ss_m::gather_stats(stats));
    cout << " SM Statistics : " << endl
         << stats  << endl;

    // Clean up and shut down
    cout << "\nShutting down SSM ..." << endl;
    delete ssm;

    cout << "Finished!" << endl;

    return;
}

// This was copied from file_scan so it has lots of extra junk
int
main(int argc, char* argv[])
{
    smthread_user_t *smtu = new smthread_user_t(argc, argv);
    if (!smtu)
            W_FATAL(fcOUTOFMEMORY);

    w_rc_t e = smtu->fork();
    if(e.is_error()) {
        cerr << "error forking thread: " << e <<endl;
        return 1;
    }
    e = smtu->join();
    if(e.is_error()) {
        cerr << "error forking thread: " << e <<endl;
        return 1;
    }

    int        rv = smtu->retval;
    delete smtu;

    return rv;
}
