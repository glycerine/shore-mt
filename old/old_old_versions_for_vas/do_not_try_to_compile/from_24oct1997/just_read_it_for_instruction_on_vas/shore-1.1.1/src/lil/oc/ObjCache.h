/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

// ObjCache.h
//

/* $Header: /p/shore/shore_cvs/src/lil/oc/ObjCache.h,v 1.79 1997/01/24 20:14:09 solomon Exp $ */

#ifndef _OBJCACHE_H_
#define _OBJCACHE_H_

#ifndef OBJECT_CACHE
#define OBJECT_CACHE
#endif

#include <iostream.h>

#ifdef __GNUG__
#pragma interface
#endif

#include "OCTypes.h"
#include "OTEntry.h"
#include "MemMgr.h"
#include "LOIDTable.h"
#include "VolumeTable.h"
#include "vec_t.h"
//#ifdef SERVER_ONLY
// need threads include for ocvas definition
//#include "sm_s.h"
//#include "smthread.h"
//#endif

// for unix compat stuff
#include <unistd.h>

#ifndef roundup
// roundup may or may not have been defined, according to include dependencies.
#define         roundup(x, y)   ((((x)+((y)-1))/(y))*(y))
#endif

class svas_base;

// The size of a chunk of the object table
#define OT_CHUNK_SIZE		500

// A bunch of preallocated serials.
struct SerialSet
{
    int nserials;
    serial_t serial;
};

// A node of the xact_alloc_list
struct XactAllocListNode
{
    caddr_t p;
    bool in_use;
    XactAllocListNode *next;
};

struct OTEntry;
struct OTChunk;

////////////////////////////////////////////////////////////////////////
//
//    The Object Cache.  An application can never have more than one
//    object cache.  Therefore, all methods and data members of this
//    class are static.
//    hm, for server version, ditch this.  
//
////////////////////////////////////////////////////////////////////////

class ObjCache
{
    friend class Shore;
    friend class OCRef;
    friend class PoolScan;
    friend class DirScan;
    friend class OTEntry;
    friend class LOIDTable;
    friend class sdl_index_base; // needs to do lvid->vindex translation.
    friend class MemMgr;	// needed to do writeback callback
	friend class OCstats;	// needed to compute statistics.


 private:
    w_rc_t return_error(w_rc_t rc);

 protected:
    ////////////////////////////////////////////////////////////////
    //
    //	   Applications call these methods via wrappers in the
    //		    Shore, OCRef or PoolScan classes.
    //
    ////////////////////////////////////////////////////////////////

    // Initialization and shutdown methods
    static w_rc_t init_error_codes();
    w_rc_t init();
	void init_svars(); // initialize former static variables.
    w_rc_t exit();

	// needed by OCRef
	bool is_initialized() { return initialized; }
	bool is_einitialized() { return e_initialized; }

    // Transaction methods.
    w_rc_t begin_transaction(int degree = 2);
    w_rc_t commit_transaction(bool invalidate = false );
    w_rc_t chain_transaction();
    w_rc_t abort_transaction(bool invalidate = false);
    TxStatus get_txstatus();


 private:
    w_rc_t automatic_stats(bool clear = true);

 protected:
    // UNIX compatiblity methods
    w_rc_t chdir(const char *path);
    w_rc_t unlink(const char *path);
    w_rc_t mkdir(const char *path, mode_t mode);
    w_rc_t rmdir(const char *path);
    w_rc_t rmpool(const char *path);
    w_rc_t readlink(const char *path, char *buf, int bufsize,
			   int *resultlen);
    w_rc_t readxref(const char *path, OTEntry *&ote);
    w_rc_t utimes(const char *path, struct timeval *tvp);
    w_rc_t rename(const char *oldpath, const char *newpath);
    w_rc_t symlink(const char *contents, const char *linkname);
    w_rc_t umask(mode_t newmask, mode_t *oldmask);
    w_rc_t getcwd(char *buf, int bufsize);
    w_rc_t chmod(const char *path, mode_t newmode);
    w_rc_t chown(const char *path, uid_t owner, gid_t group);
    w_rc_t access(const char *path, int mode, int &error); // R_OK, etc

    // Object creation methods.
    w_rc_t create_anonymous(OTEntry *pool, rType *type,
				   OTEntry *&ote);

    w_rc_t create_transient(rType *type,
				   OTEntry *&ote);

    w_rc_t create_registered(const char *path,
				    mode_t mode, rType *type,
				    OTEntry *&ote);

    w_rc_t create_pool(const char *path, mode_t mode, OTEntry *&ote);
    w_rc_t create_xref(const char *path, mode_t mode, OTEntry *ote);

    // Destroys the given object.
    w_rc_t destroy_object(OTEntry *ote);

    // Pathname lookup methods.  The latter form returns the object's
    // type as well as its ote.  This form is more expensive than the
    // simpler one, and should only be used when the type is needed.
	// the first form has an optional arg indicating the type expected.
    w_rc_t lookup(const char *path, OTEntry *&ote, rType *tpt=0);
    w_rc_t lookup(const char *path, LOID &type, OTEntry *&ote);

    // index manipulation methods
    w_rc_t addIndex(const IndexId &iid, IndexKind k);
    w_rc_t insertIndexElem(const IndexId &iid, const vec_t &k,
				  const vec_t &v);
    w_rc_t removeIndexElem(const IndexId &iid, const vec_t &k,
				  int *nrm);
    w_rc_t removeIndexElem(const IndexId &iid, const vec_t &k,
				  const vec_t &v);
    w_rc_t findIndexElem(const IndexId &iid, const vec_t &k, 
				const vec_t &value,
				ObjectSize *vlen, bool *found);
    shrc openIndexScan(const IndexId &iid,
		CompareOp o1, const vec_t & k1,
		CompareOp o2, const vec_t &k2,
		Cookie & ck);
    shrc nextIndexScan(Cookie & ck,
		const vec_t &k, ObjectSize & kl,
		const vec_t &v, ObjectSize& vl,
		bool &eof);
    shrc closeIndexScan(Cookie &ck) ;

    // Methods to retrieve object metadata (NB: some of these methods
    // may have to go to the server to get the requested info).
    w_rc_t get_type(OTEntry *ote, rType *&type);
    w_rc_t get_type(OTEntry *ote, LOID &type_loid);
    w_rc_t get_pool(OTEntry *ote, OTEntry *&pool_ote);
    w_rc_t get_kind(OTEntry *ote, ObjectKind &kind);
    w_rc_t get_lockmode(OTEntry *ote, LockMode &lm);
    w_rc_t get_volref(OTEntry *ote);
    w_rc_t get_primary_loid(OTEntry *ote, LOID &loid);
    w_rc_t get_loid(OTEntry *ote, LOID &loid);
    w_rc_t simple_get_loid(OTEntry *ote, LOID &loid);
	// get correct volume-relative loid for the metatype object referenced
	// by type on  the volume ote refers to.
    w_rc_t get_type_loid(OTEntry *ote, const rType * type, LOID &t_loid);

    // Methods to retrieve an object into the object cache and/or deal
    // with locks.  In lock_obj, `force' means to request the lock
    // from the vas even if we already hold a lockwhose mode covers
    // lm.
    w_rc_t fetch(OTEntry *ote, LockMode lm = READ_LOCK_MODE);

    void   prefetch(OTEntry *ote); // called to prefetch a page...
    w_rc_t make_writable(OTEntry *ote, LockMode lm = WRITE_LOCK_MODE);
    w_rc_t lock_obj(OTEntry *ote, LockMode lm, bool force = false);

    // Removes an object from the object cache.  If the object is not
    // currently in the cache then no action is taken.  If the object
    // is in the cache and is dirty, then it is first written back to
    // the server.  The function does not release locks held on the
    // object.
    w_rc_t flush(OTEntry *ote, bool reclaim_space=true);

    // Indicates whether the given object is in the cache
    w_rc_t is_resident(OTEntry *ote, bool &res);

    // Determines whether the given ote is valid (contains a valid
    // LOID) and can be locked in the given lock mode.
    w_rc_t valid(OTEntry *ote, LockMode lm);

    // Pool scan functions.
    w_rc_t poolscan_open(const char *path, Cookie *cookie);
    w_rc_t poolscan_open(OTEntry *pool_ote, Cookie *cookie);
    w_rc_t poolscan_next(Cookie *cookie, bool fetch,
				LockMode lm, OTEntry *&ote);
    w_rc_t poolscan_close(Cookie cookie);

    // Directory scan functions
    w_rc_t dirscan_open(const char *path, LOID &loid);
    w_rc_t dirscan_next(const LOID &loid, Cookie *cookie,
			       char *buf, int buflen, int *nentries);

    // Statistics gathering and clearing

    shrc gather_stats(w_statistics_t &, bool remote=false);
    void clear_stats();


 private:
    ////////////////////////////////////////////////////////////////
    //
    //	   These methods are internal methods of ObjCache.
	// 		we don't have to make static shadows of these.
    //
    ////////////////////////////////////////////////////////////////

    // Help functions for object creation.  The first is analogous to
    // a constructor for otes.  The second allocates memory for the
    // given object.  The number of bytes is given by ote->type.
    w_rc_t new_ote(const rType *type,
			  int vindex, const VolRef &volref,
			  ObjFlags flags, OTEntry *pool, LockMode lm,
			  OTEntry *&ote);
    w_rc_t alloc_mem(OTEntry *ote);

    // Help functions for object destruction.
    w_rc_t remove_destroyed(OTEntry *ote);
    w_rc_t free_mem(OTEntry *ote);

    // Transactional memory allocator.  This facility might be of use
    // to applications in general, but the current implementation is
    // so simple-minded that its use is restricted (the only current
    // user is the DirScan class).  Xact_alloc is a memory allocator.
    // Any memory allocated with it is freed when the transaction
    // terminates, unless it is freed earlier by xact_free.  Blocks of
    // allocated memory are kept on a list whose head is
    // ObjCache::xact_alloc_list.  The list is cleared in reset().
    caddr_t xact_alloc(int nbytes);
    w_rc_t xact_free(caddr_t p);

    // These methods convert serial numbers to otes.  The first two
    // forms return a primary ote only if the given serial is a
    // primary serial.  Therefore, theya are always local operations.
    // The second two forms always return a primary ote, and may
    // therefore have to go to the vas to snap the serial.
    w_rc_t get_ote(int vindex, const VolRef &volref, OTEntry *&ote);
    w_rc_t get_ote(const LOID &loid, OTEntry *&ote);
    w_rc_t get_primary_ote(const LOID &loid, OTEntry *&primary);
    w_rc_t get_primary_ote(int vindex, const VolRef &volref,
				  OTEntry *& primary);

    // These methods fill in the given OStat or SysProps structures.
    // The first two forms are for application use; the second two
    // forms are for internal use.
    w_rc_t stat(const char *path, OStat *osp);
    w_rc_t stat(OTEntry *ote, OStat *osp);
    void stat(SysProps *props, OStat *osp);
    w_rc_t sysprops(OTEntry *ote, SysProps *props = 0,
			   bool cache_obj = false,
			   LockMode lock = SYSPROPS_LOCK_MODE,
			   int * prefect_count = 0);

    // Returns a pointer to the alias for the given otentry
    // on the given volume.
    w_rc_t get_matching(OTEntry *otentry, int vindex,
			       OTEntry *&ote);

    // Returns the primary alias of the given otentry.
    w_rc_t snap(OTEntry *ote, OTEntry *&primary);

    // Creates an alias for the given otentry on the given volume.
    w_rc_t create_alias(OTEntry *otentry, int vindex,
			       OTEntry *&alias);

    // Adds an (already created) alias for otentry with the given
    // vindex and volref.
    w_rc_t add_alias(OTEntry *otentry, LOID &loid,
			    OTEntry *&alias);
    w_rc_t add_alias(OTEntry *otentry, int vindex,
			    VolRef &volref, OTEntry *&alias); 

    // Returns a pointer to an unused otentry.
    w_rc_t alloc_ote(OTEntry *&ote);

    // These methods handle allocation of serial numbers.  The first
    // method allocates serial numbers from the given volume.  The
    // number of serials allocated will be at least `nserials,' but
    // may be more.  If we already have that number of serials from
    // that volume then no action is taken.  The second method returns
    // an unused serial number from the given volume and places it in
    // `volref.'
    w_rc_t allocate_serials(int vindex, int nserials = 100);
    w_rc_t new_serial(int vindex, VolRef &volref);

    // Flushes the object cache.  For each object, if it is new or
    // modified, then it is created on the server or written back.  In
    // either case, the object is removed from the cache and the space
    // it occupied is freed.  If `reset_locks' is True then the
    // object's lock state is reset to NL (No Lock).
    w_rc_t flush(bool reset_locks);

    // incrementally remove objects from the oc, if they are not
    // pinned...
    w_rc_t release_mem(int mlen);

    // If the given object is new or modified, then it is written back
    // to the server.  If the object was a new anonymous object, then
    // `pool_create' is set to 1; otherwise 0.
    w_rc_t writeback(OTEntry *otentry);

    // Calls vas::utimes on each of the pools in the given list.  The
    // list is the list of pools in which objects have been created
    // during the current transaction.  Each pool is removed from the
    // list after its timestamps are updated.  NB: The list is chained
    // together using the `pool' field of the OTEntry.
    w_rc_t update_pool_timestamps(struct timeval *tvp);

    // Clears the pool list.  This is called if we failed to commit
    // the transaction before update_pool_timestamps completed.
    void clear_pool_list();

    // Resets the object cache.  Flags are:
    //
    // Invalidate: Passing True allows the object cache to free some
    // large data structures, but also causes all existing refs to
    // become invalid (including global refs and refs on the stack).
    // Any attempt to dereference a ref invalidated in this manner
    // will have undefined results.
    //
    // Flush: Passing True causes all objects to be removed from the
    // cache, and all of the space that was allocated to cached
    // objects to be freed.  NOTE: this parameter should always be
    // True unless the caller knows that the cache has already been
    // flushed (for example by a *successful* call to `writeback').
    // If False is passed and the cache was not already flushed, then
    // the memory allocated to cached objects will never be freed.
    // However, if the caller knows that the cache has been flushed,
    // then False can be passed safely.  The benefit of passing False
    // is that the cache manager may be able to avoid making a pass
    // through the object table.
    //
    // Reset_locks: Passing True causes the lock state of each object
    // to be reset to NL (No Lock).  If `invalidate' is True then this
    // flag is ignored.  If `invalidate' is False, then True should be
    // passed UNLESS the caller knows that the locks have already been
    // reset (i.e., by a *successful* call to `writeback').  If refs
    // are not invalidated and locks are not reset, then the object
    // cache will have incorrect information about locks held by
    // transactions, and may therefore fail to request necessary
    // locks.
    void reset(bool invalidate,
		      bool flush = true, bool reset_locks = true);

	// writeback_all - write back all objects w/o resetting
	// anything -- used for chaining transactions
	w_rc_t writeback_all();

	// writeback_mmgr - write back all objects in mmgr w/o resetting
	// anything -- will keep pages clustered.  If the reset_locks
	// flag is true, any memory will be deallocated from the cache.
	// otherwise, the space will be reclaimable.
	w_rc_t writeback_mmgr(bool reset_locks);

	// clean up transaction information on commit, abort, etc
	void reset_xact(bool chained=false);

    // This function makes a meager attempt at dealing with redundant
    // error codes from lower layers.  This is a band-aid solution for
    // a much larger problem that should be dealt with soon.
    w_rc_t fix_rc(w_rc_t rc);

	// these functions begin and end svas batching.
	w_rc_t begin_batch();
	w_rc_t end_batch();

    // Helps with error handling.  This function can be removed when
    // the client side of the vas uses w_rc_t.
    int get_vas_errcode();

    w_rc_t last_error();

    ////////////////////////////////////////////////////////////////
    //
    //			     Data members
    //
    ////////////////////////////////////////////////////////////////

    // Indicates whether the object cache has been initialized.
    bool initialized;

    // Indicates whether the error stuff has been initialized.
    static bool e_initialized;

    // The number of the current transaction.  This is not a
    // transaction id - it is only used on the client side.  The
    // primary users of this value are the scan classes, which use it
    // to implicitly close scans that are open when a transaction
    // terminates.  If this value is 0 then there is no transaction
    // running.
    int curr_xact_num;

    // The value that the next transaction to be started will use.
    int next_xact_num;

    // Table of volumes that objects live in.
    VolumeTable vt;

    // Allocates/manages memory in the object cache.
    MemMgr mmgr;

    // The object table that keeps track of the objects in the object cache
    LOIDTable lt;

    // The value-added server that services the object cache.
//#ifdef SERVER_ONLY
    // for server-based cache, do this through the thread pointer.
    svas_base * get_ocvas(); // { return (vas *)(&(me()->user_p())); };
//#define vas get_ocvas()
//#else
    svas_base *vas;
//#endif

    // Points to the first chunk of OT entries.
    OTChunk *first;

    // Points to the current chunk (the one we are currently allocating from).
    OTChunk *current;

    // The index of the next free OT entry in the current chunk.
    int next_free;

    // The head of a linked list of pools in which objects have been
    // created during the current transaction
    OTEntry *pool_list;

    // An array of unsed serials indexed by the vindex of the volume they
    // came from.  `serial_array_size' is the number of elements in the array.
    // Thus if nvolumes <= vindex, then we we don't have any preallocated
    // serials from the volume.

    int serial_array_size;
    SerialSet *serials;

    // The xact alloc list.  See xact_alloc and xact_free for details.
    XactAllocListNode *xact_alloc_list;

protected:
    bool	do_prefetch;
    bool	do_pagecluster;
    bool	do_pstats;
    bool	do_premstats;
    bool	do_batch;
    bool	do_refcount;
	bool 	batch_active;
	bool  inside_server; // running inside server process
    int		batch_q_len;
    int		auditlevel;
    unsigned int	_last_error_num;
    bool	aborting;

	int 		unix_error(int);
	int 		unix_error(w_rc_t);

	int 		oc_unix_error(int);
	int 		oc_unix_error(w_rc_t);
	void 	set_mem_limit(int);
	ObjCache() { initialized = false; }
	~ObjCache(); // reclaim storage.
};


// The object table is allocated in chunks.  `Alloc_ote' dispenses
// otentries and allocates new chucks as necessary.  Currently,
// otentries are never freed individually: the entire OT is
// (optionally) freed at transaction commit.  See the documentation
// for `reset.'
struct OTChunk
{
    // A bunch of OT entries
    OTEntry entries[OT_CHUNK_SIZE];

    // A pointer to the next bunch
    OTChunk *next;
};

ostream &operator<<(ostream &os, OTEntry *ote);

// This macro helps with handling errors from the vas and lower.  It
// is just a temp fix until the vas client side uses w_rc_t.
// w_rc_t constructor takes file, line, errnum -- we sort
// of fake it out here with the file & line
#define VAS_DO(x)						\
    if(x){							\
	w_rc_t __e(__FILE__, __LINE__, get_vas_errcode());	\
	return return_error(__e);					\
    }

// These routines are implemented in the language binding.  When an
// object is first fetched from disk, the object cache calls
// prepare_for_application to swizzle pointers, perform byte-swapping,
// initialize vtable pointers, and anything else that needs to be done
// to make the object usable by the application.  When the object is
// to be written back to disk, the object cache calls prepare_for_disk
// to unswizzle and un-byte-swap the object.  When the object is to be
// destroyed (either via destroy_object or unlink), the object cache
// calls prepare_to_destroy to fix up relationships, indices, etc.

// `Type_loid' is the oid of the object's type object.  `Volid' is the
// volid of the volume the object came from.  `Obj' is the address of
// the object.  The language binding returns a pointer to the type
// object corresponding to `type_loid.'  This is the type object that
// is returned by OCRef::deref and OCRef::get_type., as well as being
// used in prepare_for_disk.
// variant for heap/text support: pass in  tlen explicitly; it is at end
// of heap

extern w_rc_t prepare_for_application(LOID &type_loid, 
					OTEntry *ote,
				      StringRec &string,
				      smsize_t tlen);

// `Type' is the type object originally obtained in
// prepare_for_application.  `Volid' is the volid of the volume the
// object lives on.  `Obj' is a pointer to the object.
// this variant is used by the sdl heap/text support version.

extern w_rc_t prepare_for_disk( OTEntry *ote,
			       struct vec_t * vpt,
			       smsize_t &hl, smsize_t &tl);

// this function frees any memory associated with
// an object that doesn't
// belong to the object cache.
extern w_rc_t prepare_to_destroy(OTEntry *ote);
// this function reclaims (reswizzles) an object that
// has been "prepared_for_disk" but is still cached (memory resident)
extern w_rc_t reclaim_object(OTEntry *ote);

// an object that doesn't
// belong to the object cache.
extern w_rc_t prepare_to_destroy(OTEntry *ote);


// this function is a callback from ObjCache to sdl level meant to
// be called just before commit; it updates module refcounts as necessary.
extern w_rc_t prepare_to_commit(bool refcounting = false);
extern w_rc_t prepare_to_abort();

// compute resident heap of a cached object..
size_t compute_heapsize(OTEntry *ote,size_t *tlen);


// Returns the type object corresponding to LOID if there is one.

extern rType *get_type_object(LOID &loid);

extern void sdl_init();

// return the vas pointer this cache is to use for sm operations.
// when linked with server, it will return the vas corresponding
// to the current thread; when linked as a client, it will get
// a new vas client structure.  It sets the boolean in_server
// appropriately.
extern w_rc_t get_my_svas(svas_base ** vpt, bool & in_server);


// we use a macro to define ObjCache member function calls that
// were formerly static, e.g.
// the call ObjCache::chdir("/")
// is replaced by
// OC_CALL(chdir)("/").  This is ugly, but should not show up
// at user level
extern ObjCache * cur_oc; // hack; need to redefine for global context
extern ObjCache * check_my_oc(); 
extern ObjCache * get_my_oc(); 
// function which makes this context dependent,
extern void set_my_oc(ObjCache *); 
// function which makes this context dependent,
// so we can have per thread oc's in the server.
// #define OC_ACC(name) cur_oc->name
// #include "svas_base.h"
// #define OC_ACC(name) ((ObjCache *)(svas_base::get_oc()))->name
#define OC_ACC(name) (get_my_oc())->name
#endif
