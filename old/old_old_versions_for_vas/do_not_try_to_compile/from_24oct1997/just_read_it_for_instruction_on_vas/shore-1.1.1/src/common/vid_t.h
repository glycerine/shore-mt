/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/common/vid_t.h,v 1.16 1997/05/19 19:41:16 nhall Exp $
 */
#ifndef VID_T_H
#define VID_T_H

#ifdef __GNUG__
// implementation is in lid_t.c
#pragma interface
#endif

/*
 * NB -- THIS FILE MUST BE LEGITIMATE INPUT TO cc and RPCGEN !!!!
 * Please do as follows:
 * a) keep all comments in traditional C style.
 * b) If you put something c++-specific make sure it's 
 * 	  got ifdefs around it
 */

struct vid_t {

#ifdef __cplusplus
    enum {mask_alias = 0x8000,
	  first_alias = mask_alias,
	  first_local = 1
	 };

    		vid_t() : vol(0) {}
    		vid_t(uint2 v) : vol(v) {}
    bool	is_remote() const { return is_alias();}
    bool	is_alias() const { return (vol & mask_alias) == mask_alias;}
    void	init_alias()	{vol = first_alias;}
    void	init_local()	{vol = first_local;}

    void	incr_alias()	{w_assert3(is_alias());
				    vol++;
				    if (!is_alias()) init_alias();
				}

    void	incr_local()	{w_assert3(!is_alias());
				    vol++;
				    if (is_alias()) vol = 0;
				}

    // This function casts a vid_t to a uint2.  It is needed
    // in lid_t.h where there is a hack to use vid_t to
    // create a logical volume ID.
    		operator uint2 () const {return vol;}

//    		operator int () const {return (int)vol;}
//    		operator unsigned int () const {return (unsigned int)vol;}

#endif /* __cplusplus */

    // Data Members
    uint2	vol;

#ifdef __cplusplus
    static const vid_t null;
    friend inline ostream& operator<<(ostream&, const vid_t& v);
    friend inline istream& operator>>(istream&, vid_t& v);
    friend bool operator==(const vid_t& v1, const vid_t& v2)  {
	return v1.vol == v2.vol;
    }
    friend bool operator!=(const vid_t& v1, const vid_t& v2)  {
	return v1.vol != v2.vol;
    }
#endif /* __cplusplus */
};

#ifdef __cplusplus
inline ostream& operator<<(ostream& o, const vid_t& v)
{
    return o << v.vol;
}
 
inline istream& operator>>(istream& i, vid_t& v)
{
    return i >> v.vol;
}
#endif /*__cplusplus*/
#endif /*VID_T_H*/
