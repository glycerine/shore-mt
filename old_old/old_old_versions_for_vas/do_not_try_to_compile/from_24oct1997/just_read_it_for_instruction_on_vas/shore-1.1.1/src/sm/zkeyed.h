/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: zkeyed.h,v 1.22 1997/05/19 19:48:38 nhall Exp $
 */
#ifndef ZKEYED_H
#define ZKEYED_H

#ifdef __GNUG__
#pragma interface
#endif

/*--------------------------------------------------------------*
 *  class zkeyed_p					        *
 *--------------------------------------------------------------*/
class zkeyed_p : public page_p {
public:
    
    rc_t 			link_up(
	shpid_t 		    new_prev,
	shpid_t 		    new_next);

    rc_t 			format(
	const lpid_t& 		    pid,
	tag_t 			    tag,
	uint4_t			    flags,
	const cvec_t& 		    hdr);    

    rc_t			insert(
	const cvec_t& 		    key, 
	const cvec_t& 		    aux, 
	slotid_t 		    slot,
	bool			    do_it=true
	);
    rc_t			remove(slotid_t slot);
    rc_t			shift(slotid_t snum, zkeyed_p* rsib);

#ifdef __GNUC__
    /* gnu has a bug */
    rc_t			rec(
	int 			    idx, 
	cvec_t& 		    key,
	const char*& 		    aux,
	int& 			    auxlen) const;
#endif /*__GNUC__*/    
    
protected:
    
    MAKEPAGE(zkeyed_p, page_p, 1);

#ifndef __GNUC__
    rc_t			rec(
	int 			    idx,
	cvec_t& 		    key,
	const char*& 		    aux, 
	int& 			    auxlen) const;
#endif /*!__GNUC__*/
    
    int 			rec_size(int idx) const;
    int 			nrecs() const;
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

/*--------------------------------------------------------------*
 *  zkeyed_p::rec_size()					*
 *--------------------------------------------------------------*/
inline int zkeyed_p::rec_size(int idx) const
{
    return page_p::tuple_size(idx + 1);
}

/*--------------------------------------------------------------*
 *    zkeyed_p::nrecs()						*
 *--------------------------------------------------------------*/
inline int zkeyed_p::nrecs() const
{
    return nslots() - 1;
}

/*--------------------------------------------------------------*
 *    zkeyed_p::link_up()					*
 *--------------------------------------------------------------*/
inline rc_t
zkeyed_p::link_up(shpid_t new_prev, shpid_t new_next)
{
    return page_p::link_up(new_prev, new_next);
}

/*--------------------------------------------------------------*
 *    zkeyed_p::rec()						*
 *--------------------------------------------------------------*/
inline rc_t
zkeyed_p::rec(
    int 		idx, 
    cvec_t& 		key,
    const char*& 	aux,
    int& 		auxlen) const
{
    const char* base = (char*) page_p::tuple_addr(idx + 1);
    const char* p = base;
    /* 
     * a record is:
     * -length (int4) of key
     * -key
     * -value
     */
    int l = (int) * (int4*) p;
    p += sizeof(int4);
    key.put(p, l);
    
    p += l;
    aux = p;
    auxlen = page_p::tuple_size(idx + 1) - (p - base);
    
    return RCOK;
}

#endif /*ZKEYED_H*/
