/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __MOUNT_M_H__
#define __MOUNT_M_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/mount_m.h,v 1.25 1995/07/14 22:39:38 nhall Exp $
 */
#include <copyright.h>

/*
 *	$RCSfile: mount_m.h,v $
 */

#include <assert.h>
#include <debug.h>
#include <stdio.h>
// #include <sysdefs.h>
#include <sm_vas.h>
#include <vas_types.h>
#include <svas_error_def.h>
#include <os_error_def.h>
#include <string_t.h>
// #include <lid_t.h>
// #include <tid_t.h>

#define MNT_STR_LEN 159

#define HASH_ON_LVID
#define HASH_VALUE	lvid_t

/* 
 * The mount_info struct contains the information 
 * VAS's information about mount points.
 * This is the root of all namei() lookups.
 */

#ifdef __GNUG__
# pragma interface
#endif

class mount_m;
class mount_info {
	friend class mount_m;

	enum mount_kind { transient_mount,   persistent_mount };
		// transient is unix-style and can be ro/rw
		// persistent is afs-style and is only rw for now

	enum mount_kind	kind;
    lvid_t      lvid; // logical volume id  -- KEY for hash table
					 // and key for one version of find()
	serial_t	root;	// oid of root dir on this volume/mounted 
						//	(sub-)filesystem

	// for transient mounts
	// - mountpoint is full path of local mountpoint (directory)
	// - mnt is the oid of that same mountpoint
	// - lvid is the volume id of the volume mounted 
	// - root is the oid of the root dir on the volume  

	// for persistent mounts, 
	// - mountpoint is the filename of the link
	// - mnt is the directory in which the link resides
	// - lvid is the volume id of the volume mounted 
	// - root is the oid of the root dir on the volume  

	string_t	mountpoint;	// see above
							// key for find(path...)
	lrid_t		mnt;	// see above 
							// key for [pt_]map()

	Cookie		cookie; // integer for getMnt
	int 		use_count; // # client processes that have "cd"-ed to
						// somewhere in this volume
	serial_t	reg_fid;
	uid_t		uid; // of him who mounted it; "shore" if done from terminal
	bool		writable; // False--> mounted read-only 

	friend  ostream&	operator<<(ostream &o, const mount_info &m);
};

class mount_m {

public:
	enum	ToFrom	{To=1,From=2};

    mount_m(int max);	// max # entries?
    ~mount_m();
	void dump()const;
	void debug_dump()const;
	VASResult pmount( 	
				const lvid_t    &lvid, 	// lvid,rooti identify root of volume
				const serial_t	&rooti,	
				const serial_t	&rpfid, // file for registered objs
				const lrid_t	dir,	// dir+ln identify local path of link
				const Path		ln,		// in
				bool			writable, // in
				uid_t			uid
	);
	VASResult tmount( 	
				const lvid_t    &lvid, 	// in
				const serial_t	&rooti,	// in
				const serial_t	&rpfid, // file for registered objs
				const Path		mp,		// path of mountpoint
				const lrid_t	mnt,	// oid of dir being mapped to volume
				bool			writable, // in
				uid_t			uid
	);
	VASResult find(const Path	path, // in
				bool			*iswritable, // out
				lvid_t			*lvid // out
	);
	VASResult find(
				const lvid_t	&lvid, // in
				serial_t		*const rpfid, // out
				bool			*const writable // out
	);
private:
	VASResult _map(
		enum mount_info::mount_kind  k,	
		lrid_t		from, 
		lrid_t		*to=0, // OUT
		bool		*iswritable=0, // OUT
		serial_t	*reg_file=0	// OUT
	);
public:
	VASResult tmap(
		lrid_t		from, // IN
		lrid_t		*to=0, // OUT
		bool		*iswritable=0, // OUT
		serial_t	*reg_file=0	// OUT
	);
	VASResult pmap(
		lrid_t		from, // IN
		lrid_t		*to=0, // OUT
		bool		*iswritable=0, // OUT
		serial_t	*reg_file=0	// OUT
	);
	VASResult namei(const Path	path, // in
				bool			*iswritable, // in
				int				*prefix_len, // out
				lvid_t			*lvid, // out
				serial_t		*rootobj,	// out
				serial_t		*reg_file	// out
	);

	VASResult dismount(const lvid_t &lvid); // in
	VASResult dismount(const Path path, lvid_t *lvid); // in, inout

#ifdef notdef
	VASResult cd(
				const Path	path, 			// in
				ToFrom			toOrFrom	// in  
	);
#endif
	VASResult cd(
				const lvid_t	&lvid, 		// in
				ToFrom			toOrFrom	// in  
	);

	VASResult getmnt_info(
		FSDATA 			*resultbuf, // inout
		ObjectSize		bufbytes, 	// in
		int		 	*const nresults, // out
		Cookie		*const cookie	// inout
	);
	VASResult getmnt_info(
		const lvid_t	&lvid, 	// out
		FSDATA 			*fsd 	// out
	);
	void dismount_all();

	// df command on volumes DU DF
	VASResult df_info(const lvid_t&, struct df_info&);

private:
	VASResult _cd( 
				mount_info		*mount_info, // in
				ToFrom			toOrFrom	// in  
	);
	VASResult _dismount(
				mount_info *mount_info
	);
	mount_info *_find(
				const lvid_t		&lvid,  // in
				bool *found				// out
	);
	mount_info *_find(
				const Path		path, 	// in
				bool *found			// out
	);
	mount_info *_namei(
				const Path	path, 		// in
				bool 		*found,		// out
				int			*prefix_len // out
	);
	VASResult __mount( 	
				mount_info::mount_kind		mk,
				const lvid_t    &lvid, 	// in
				const serial_t	&rooti,	// in
				const serial_t	&rpfid, // file for registered objs
				const Path		mp,		// in
				const lrid_t	mnt,	// in
				bool			writable, // in
				uid_t			uid
	);

private:
    mount_info  *_info;
    int	    	 _max_mount;
    int	    	 _cur_mount;
	Cookie		_current_cookie;

public:
	friend ostream&	operator<<	(ostream &o, const mount_m &m);
	void audit() const;
};

extern mount_m *mount_table;

EXTERNC void print_mount_table();
#endif	
