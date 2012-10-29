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

// -*- mode:c++; c-basic-offset:4 -*-
/*<std-header orig-src='shore' incl-file-exclusion='LOCK_X_H'>

 $Id: lock_cache.h,v 1.8 2010/10/27 17:04:23 nhall Exp $

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

#ifndef LOCK_CACHE_H
#define LOCK_CACHE_H

#include "w_defines.h"

/*  -- do not edit anything above this line --   </std-header>*/

class lock_cache_elem_t : public w_base_t {
private:
    lock_base_t::lmode_t     _mode; // sometimes the lock mgr needs a ref
    // to a mode. Arg.
public:
    lockid_t                lock_id;
    bool                    valid;
    lock_request_t*         req;

    lock_cache_elem_t()
    : _mode(NL),
      valid(false),
      req(0)
    {
    }

    lock_base_t::lmode_t mode() const { return req? req->mode() : NL; }
    lock_base_t::lmode_t &cache_mode() { _mode = mode(); return _mode; } 


    const lock_cache_elem_t &operator=(const lock_cache_elem_t &r)
    {
        lock_id = r.lock_id;
        valid = r.valid;
        req = r.req;
        return *this;
    }

    void clear() {
        _mode = NL;
        valid = false;
        req = NULL;
        lock_id.zero();
    }

    void dump(ostream &out) const {
        if(valid) 
        {
            out << "\tlock_id " << lock_id 
                << " _mode " << _mode 
                << endl;
            if(req) {
                out << " request: ";
                out << *req;
            } 
            cout << endl;
        }
        if(req && (_mode != req->mode())) {
            // I'm printing this just to show that the
            // modes aren't always correct. The caller has
            // to use cache_mode to get a handle on
            // the mode, and that will ensure that it's
            // current.
            // smsh script lock.upgrade.release demonstrates this.
            out << " ***** MODE MISMATCH IN LOCK CACHE " << endl;
        }
    }

private:
    // disabled
    lock_cache_elem_t(const lock_cache_elem_t &);
};
    

template <int S, int L>
class lock_cache_t : public w_base_t {
    lock_cache_elem_t        buf[S][L];
    // First index is the bucket; 
    // Second index is the lock level (granularity/lspace)
    // We segregate the items by granularity to make
    // it easy to purge the cache of all locks of a given
    // granularity (or higher or lower).
public:
    void reset() {
        for(int j=0; j < L; j++) 
        for(int i=0; i < S; i++) buf[i][j].clear();
    }
    lock_cache_elem_t* probe(const lockid_t& id, int l) {
        // probe a single bucket. Caller should verify its contents
        uint4_t idx = id.hash();
        return  &buf[idx % S][l];
    }
    lock_cache_elem_t* search(const lockid_t& id) {
        // probe a single bucket. If it fails, oh well.
        lock_cache_elem_t* p = probe(id, id.lspace());
        return (p->lock_id == id && p->valid)? p : NULL;
    }

    // Remove from the table all locks subsumed by this one.
    // To make this a little more easily, we've made the table into an array of
    // hash tables.
    void compact(const lockid_t &_l) 
    {
        for (int k = _l.lspace() + 1; k <= lockid_t::cached_granularity; k++)
        {
            for(int i=0; i < S; i++) {
                lock_cache_elem_t *p = &buf[i][k];
                if(! p->valid ) continue;

                // make a copy
                lockid_t l(p->lock_id);
                // truncate to the lspace of the incoming argument
                w_rc_t rc = l.truncate(_l.lspace());
                // if that makes a lockid that matches the argument,
                // then the argument is a parent.
                //
                if (!rc.is_error() && (l == _l)) {
                    p->clear(); // make it available
                }
                // else, ignore it.
            }
        }
    }
    void compact() {
        // do nothing...
        for(int j=0; j < L; j++) 
        for(int i=0; i < S; i++) compact(buf[i][j].lock_id);
    }

    // Used by unit tests only, to verify that a parent lock
    // has no children in the cache after a compact is done.
    bool find_child(const lockid_t &parent, lockid_t &found ) const {
        // Look only at the entries whose lspace is > than
        // the parent's. Some will make no sense because they
        // aren't in the hierarchy.
        int j = parent.lspace()+1;
        for(; j < L; j++)  {
            for(int i=0; i < S; i++) {
               lockid_t tmp =     buf[i][j].lock_id;
               w_rc_t rc=tmp.truncate(parent.lspace());
               if(!rc.is_error()) {
                   // now see if the parent matches this 
                   // truncated  lockid
                   if(parent == tmp) {
                      found = buf[i][j].lock_id;
                      return true;
                   }
               }
            }
        }
        return false;
    }
    void dump(ostream &out) const {
        for(int j=0; j < L; j++) 
        for(int i=0; i < S; i++) {
                if(buf[i][j].valid)  {
                    out << "L " << j << " i "  << i ;
                    buf[i][j].dump(out);
                }
            }
    }

    // Return true iff a lock in the table was evicted.
    bool put(const lockid_t& id, lock_base_t::lmode_t /*m*/, 
                lock_request_t* req, lock_cache_elem_t &victim) 
    {
        bool evicted = true;
        lock_cache_elem_t* p = probe(id, id.lspace());
        // p->valid means the bucket is in use, i.e., contains an entry
        if(p->valid) 
        {
            // don't replace entries that are higher in the hierarchy!
            // Hash *might* take us to an entry that happens to
            // subsume our lock. (no longer the case?)
            w_assert0(p->lock_id.lspace() == id.lspace());

            // Element in table has equal or higher granularity.
            // Replace it.
            victim = *p;
        } else {
            // empty slot. Use it. Didn't evict anyone.
            evicted = false;
        }
    
        p->lock_id = id;
        p->req = req;
        p->valid = true;
        return evicted;
    }
};


#endif
