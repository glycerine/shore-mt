/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: keyed.h,v 1.23 1997/05/19 19:47:18 nhall Exp $
 */
#ifndef KEYED_H
#define KEYED_H

#ifdef __GNUG__
#pragma interface
#endif

class keyrec_t {
public:
    struct hdr_s {
	uint2	klen;
	uint2	elen;
	shpid_t	child;
    };
    
    const char* key() const;
    const char* elem() const;
    const char* sep() const;
    smsize_t klen() const 	{ return _hdr.klen; }
    smsize_t elen() const	{ return _hdr.elen; }
    smsize_t slen() const	{ return _hdr.klen + _hdr.elen; }
    smsize_t rlen() const	{ return _body() + slen() - (char*) this; }
    shpid_t child() const	{ return _hdr.child; }
    
private:
    hdr_s 	_hdr;
    char*	_body()	const	{ return ((char*) &_hdr) + sizeof(_hdr); }
};
    

/*--------------------------------------------------------------*
 *  class keyed_p					        *
 *--------------------------------------------------------------*/
class keyed_p : public page_p {
public:
    
    rc_t			link_up(shpid_t new_prev, shpid_t new_next);

    rc_t 			format(
	const lpid_t& 		    pid,
	tag_t 			    tag, 
	uint4_t			    flags,
	const cvec_t& 		    hdr);

    rc_t			insert(
	const cvec_t& 		    key, 
	const cvec_t& 		    el, 
	int 			    slot, 
	shpid_t 		    child = 0);
    rc_t			remove(int slot);
    rc_t			shift(int snum, keyed_p* rsib);

    
protected:
    
    MAKEPAGE(keyed_p, page_p, 1)
    
    const keyrec_t& 		rec(slotid_t idx) const;

    int 			rec_size(slotid_t idx) const;
    slotid_t 			nrecs() const;
    rc_t			set_hdr(const cvec_t& data);
    const void* 		get_hdr() const {
	return page_p::tuple_addr(0);
    }
    
private:
    // disabled
    void* 			tuple_addr(int);
    int 			tuple_size(int);
    friend class page_link_log;   // just to keep g++ happy
};

inline const char* keyrec_t::key() const	{ return _body(); }
inline const char* keyrec_t::elem() const	{ return _body() + _hdr.klen; }
inline const char* keyrec_t::sep() const	{ return _body(); }

/*--------------------------------------------------------------*
 *  keyed_p::rec()					        *
 *--------------------------------------------------------------*/
inline const keyrec_t& 
keyed_p::rec(slotid_t idx) const
{
    return * (keyrec_t*) page_p::tuple_addr(idx + 1);
}

/*--------------------------------------------------------------*
 *  keyed_p::rec_size()					        *
 *--------------------------------------------------------------*/
inline int
keyed_p::rec_size(slotid_t idx) const
{
    return page_p::tuple_size(idx + 1);
}

/*--------------------------------------------------------------*
 *    keyed_p::nrecs()						*
 *--------------------------------------------------------------*/
inline slotid_t 
keyed_p::nrecs() const
{
    return nslots() - 1;
}

/*--------------------------------------------------------------*
 *    keyed_p::link_up()					*
 *--------------------------------------------------------------*/
inline rc_t
keyed_p::link_up(shpid_t new_prev, shpid_t new_next)
{
    return page_p::link_up(new_prev, new_next);
}

#endif /*KEYED_H*/
