/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: file_s.h,v 1.24 1997/05/19 19:47:13 nhall Exp $
 */
#ifndef FILE_S_H
#define FILE_S_H

#ifdef __GNUG__
#pragma interface
#endif

typedef uint4 clust_id_t; // not used at this time
class file_p;

enum recflags_t { 
    t_badflag		= 0x00,
    t_forwardroot	= 0x01,     // not used yet
    t_forwarddata	= 0x02,     // not used yet
    t_small		= 0x04,    // simple record
    t_large_0 		= 0x08,    // large with short list of chunks
    t_large_1 		= 0x10,	   // large with 1-level indirection
    t_large_2 		= 0x20,    // large with 2-level indirection
    t_logicalid 	= 0x40     // has serial # (uses logical ids)
};
    
struct rectag_t {
    uint2	hdr_len;	// length of user header 
    uint2	flags;		// enum recflags_t
    uint4	body_len;	// true length of the record 
    /* 8 */

    serial_t	serial_no;	// logical serial number in file
#ifndef BITS64
    fill4	filler;		// for 8 byte alignment with small serial#s
#endif
    /* 16 */

#ifdef notdef
    fill4	cluster_id;	// cluster for this record
				// not used at this time
				// if you add it, be sure to adjust the alignment
				// above
#endif
};

class record_t {
    friend class file_m;
    friend class file_p;
    friend class pin_i;
    friend class ss_m;
public:
    enum {max_len = smlevel_0::max_rec_len };

    rectag_t 	tag;
    char	info[ALIGNON];  // minimal amount of hdr/data for record

    record_t()	{};
    bool is_large() const;
    bool is_small() const;

    smsize_t hdr_size() const;
    smsize_t body_size() const;

    const char* hdr() const;
    const char* body() const;

    int body_offset() const { 
		return offsetof(record_t,info)+align(tag.hdr_len);
	}

    lpid_t pid_containing(smsize_t offset, smsize_t& start_byte, const file_p& page) const;
private:

    // only friends can use these
    uint4  page_count() const;
    lpid_t last_pid(const file_p& page) const;
};

/*
 *  This is the header specific to a file page.  It is stored in 
 *  the first slot on the page.
 */
struct file_p_hdr_t {
    clust_id_t	cluster;
};

inline const char* record_t::hdr() const
{
    return info;
}

inline bool record_t::is_large() const	
{ 
    return tag.flags & (t_large_0 | t_large_1 | t_large_2); 
}

inline bool record_t::is_small() const 
{ 
    return tag.flags & t_small; 
}

inline const char* record_t::body() const
{
    return info + align(tag.hdr_len);
}

inline smsize_t record_t::hdr_size() const
{
    return tag.hdr_len;
}

inline smsize_t record_t::body_size() const
{
    return tag.body_len;
}

#endif /*FILE_S_H*/
