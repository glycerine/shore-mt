/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __SVAS_NFS_H__
#define __SVAS_NFS_H__
/*
 * $Header: /p/shore/shore_cvs/src/vas/server/svas_nfs.h,v 1.9 1995/07/14 22:40:03 nhall Exp $
 */

#include <copyright.h>
#include <vas_internal.h>
#include <svas_server.h>
#include <efs.h>

#ifdef __GNUC__
#	ifdef Ultrix42
#		define TIME_T const int
#	else
#		define TIME_T long
#	endif
#else
#	define TIME_T time_t
#endif /* __GNUC__ */

// forward decls:
class 	client_t;	
class 	Object;		
class 	Directory;	
class 	Anonymous;
class 	Registered;
class 	Pool;	
class 	Index;
class 	Xref;
class 	Symlink;
class 	svas_layer;	
class 	SyspCache;	
struct 	efs_handle; // forward

enum how_to_end {
	notset, suspendit, abortit, commitit
};
typedef enum how_to_end how_to_end;

class 	svas_nfs : public svas_server {
	friend class 	svas_layer;	
	friend class 	client_t;	
	friend class 	Object;		

protected:
	static		u_int 			fileid_hash(const serial_t &s);
	static		u_int 			fileid_hash(const serial_t_data &s);
	static 		u_int 			volumeid_hash(const lvid_t &s);

protected:
					// constructor
					svas_nfs(client_t *_cl, ErrLog *_el): svas_server(_cl,_el)
					{
						assert(sizeof(nfscookie)== sizeof(int));
						(void) set_service(ds_degree3);
					}
					~svas_nfs() { DBG(<<"~svas_nfs"); }

VASResult		_mkDir(
					IN(lrid_t)   	dir,		// -- can't be "/"
					IN(serial_t) 	reg_file,	// 
					const Path	 	name,		
					mode_t			mode,
					uid_t 			uid,
					gid_t 			gid,
					OUT(lrid_t)		result		
				);

VASResult 		_mkSymlink(
					IN(lrid_t)      dir,        // -- can't be "/"
					IN(serial_t)    reg_file,   //
					const           Path name,  // 
					mode_t          mode,       //
					uid_t           new_uid,    
					gid_t           new_gid,   
					const           Path target,
					OUT(lrid_t)     result     
				);

VASResult		rmUnixFile(
					IN(lrid_t) 	dir,			// -- can't be "/"
					const Path	 name,			// in
					bool		force=false		// work-around hack
				);

VASResult		_mkUnixFile(
					IN(lrid_t)		dir	,
					const 	Path 	filename,		// in
					IN(serial_t)	reg_file	,
					mode_t			mode,		// in
					uid_t			user,
					gid_t			grp,
					OUT(lrid_t)		result		
				);
				/*****/

public:
	nfs_replybuf	*get_nfs_replybuf() {	return 
		(nfs_replybuf *)get_client_replybuf(); }

	// returns true if this is a mountd or an nfsd
	bool			is_nfsd() const { return true; }

	V_IMPL( VASResult		commitTrans())
	V_IMPL( VASResult		suspendTrans())
	V_IMPL( VASResult		resumeTrans(class xct_t* tx))

	/*
	// Stub version:  gid's used to represent xct's are in the range
	// 1000000..1000099.  There should be a way for the client to control
	// which gid's can be used for this purpose (and the table should grow
	// dynamically!)
	*/
	const u_int gid_base = 1000000;
	const int 	max_gids = 100;
	static xct_t* group_map[max_gids];
	friend void nfs_free_xct();
	friend u_int nfs_alloc_xct();

	xct_t*
	group_to_xct(u_int gid) 
	{
		if (gid < gid_base || gid >= gid_base + max_gids)
			return 0;
		return group_map[gid - gid_base];
	}
	mode_t 	NF2S_IF(ftype f);
	ftype 	S_IF2NF(mode_t mode) ;
	int 	vasstatus2errno();

	nfsstat efs_stat(const nfs_fh &h, SysProps &s, ftype &type, 
		int	*is_anon=0);
	int convert_if_xref(const lvid_t &lvid, serial_t &serial);
	enum nfsstat errno2nfs(int e);
public:

	const char *efs_string(const nfs_fh &);
	const char *efs_string(const fattr &);
	const char *efs_string(const attrstat &);

	nfsstat efs_attrs(const nfs_fh &, fattr &);
	nfsstat efs_attrs(const nfs_fh &, const sattr &);
	nfsstat get_reg_file(efs_handle &h);
	nfsstat efs_lookup(const nfs_fh &, const char *, nfs_fh &);
	nfsstat efs_readlink(const nfs_fh &, ObjectSize &, char *);
	nfsstat efs_read(const nfs_fh &h, ObjectOffset, ObjectSize &, char *);
	nfsstat efs_write(const nfs_fh &h, ObjectOffset, ObjectSize &, char *);
	nfsstat efs_create(const nfs_fh &, const char *, ftype, const sattr &, nfs_fh &);
	nfsstat efs_remove(const nfs_fh &, const char *);
	nfsstat efs_rename(const nfs_fh &, const char *, const nfs_fh &, const char *);
	nfsstat efs_link(const nfs_fh &, const nfs_fh &, const char *);
	nfsstat efs_symlink(const nfs_fh &, const char *, const sattr &, const char *);
	nfsstat efs_mkdir(const nfs_fh &, const char *, const sattr &, nfs_fh &);
	nfsstat efs_rmdir(const nfs_fh &, const char *);
	nfsstat efs_readdir(const nfs_fh &, u_int, u_int, entry *&, bool &);
	nfsstat efs_getfsattrs(const nfs_fh &, statfsokres &);

	void 	nfs_end(how_to_end what, bool ongoing) ;
	enum how_to_end nfs_begin(svc_req *svcreq,struct authunix_parms *p);
 // nfs_begin

};

class 	svas_mount : public svas_nfs {
	friend class 	svas_layer;	
	friend class 	client_t;	
	friend class 	Object;		

	// info on who's mounting what
	struct mountent {
		char 		*host;
		char 		*fsname;
		lrid_t 		lrid;
		mountent 	*next;
	};
	static mountent		*mounts;

protected:
					// constructor
					svas_mount(client_t *_cl, ErrLog *_el): 
						svas_nfs(_cl,_el)
					{
					}
public:
					~svas_mount();

	nfsstat 		mnt(const char *, authunix_parms *, nfs_fh &);
	mountlist 		mounted();
	void 			unmount(authunix_parms *, const char *);
	exports 		export_info();
	how_to_end 		mount_begin(svc_req *svcreq,struct authunix_parms *p);
};

EXTERNC svas_nfs 	*ARGN2nfs(void *v);
EXTERNC svas_mount  *ARGN2mount(void *v);

#endif /*__SVAS_NFS_H__*/
