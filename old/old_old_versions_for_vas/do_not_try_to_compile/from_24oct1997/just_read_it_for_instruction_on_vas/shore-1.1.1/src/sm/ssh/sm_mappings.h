/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994,5,6,7 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sm_mappings.h,v 1.8 1997/02/26 21:04:14 nhall Exp $
 */

#ifndef TRANSIENT_MAPPINGS_H
#define TRANSIENT_MAPPINGS_H

#ifdef __GNUG__
#pragma interface
#endif


#include <sm_vas.h>
#include <mappings.h>

class log_entry;
class ep_map_i;
class ep_map : public name_ep_map {
    /* an abstract class that defines mappings needed by
     * the 2PC coordinator and subordinates
     */
    friend class ep_map_i;
public:
    NORET  	ep_map();
    NORET  	~ep_map();
    rc_t  	name2endpoint(const server_handle_t &, Endpoint &);
    rc_t  	endpoint2name(const Endpoint &, server_handle_t &);

    void  	add_ns(NameService *_ns);

    // For use by sm_coordinator:
    void  	expand(int num);
    rc_t  	add(int argc, const char **argv);
    rc_t        refresh(const char *c, Endpoint& ep, bool force=false);
    void  	clear();
    void  	dump(ostream&) const;

private:
    EndpointBox		_box;
    server_handle_t*	_slist;
    int			_slist_size;
    NameService*	_ns;

    /* No mutex on this because it'll be set up once at the beginning */
    bool  	find(const Endpoint &ep, int &i);
    bool  	find(const server_handle_t &s, int &i) const;
};

class ep_map_i {

public:
     NORET 		ep_map_i(const ep_map *e);
     void 		next(bool &eof);
     rc_t 		name(server_handle_t &);
     rc_t 		ep(Endpoint &);

private:
    int 	_index;
    const ep_map*	_map;
};


class tid_map : public tid_gtid_map {
    /* an abstract class that defines mappings needed by
     * the 2PC coordinator and subordinates
     */

private:
    struct _map {
	gtid_t 		_gtid;
	tid_t  		_tid;
	w_link_t 	_link;
	NORET _map(const gtid_t &g, const tid_t &t);
	NORET ~_map();
    };
    static w_hash_t<tid_map::_map, const gtid_t> __table;
    w_hash_t<tid_map::_map, const gtid_t> *_table;
    smutex_t	_mutex; /* will be expanded and shrunk on the fly */


public:
    NORET tid_map();
    NORET ~tid_map();
    rc_t  add(const gtid_t &, const tid_t &);
    rc_t  remove(const gtid_t &, const tid_t &);
    rc_t  gtid2tid(const gtid_t &, tid_t &);

    void  dump(ostream&) const;
};

#endif /*TRANSIENT_MAPPINGS_H*/
