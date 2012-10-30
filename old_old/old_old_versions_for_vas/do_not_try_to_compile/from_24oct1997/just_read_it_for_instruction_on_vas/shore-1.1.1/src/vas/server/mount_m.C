/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/mount_m.C,v 1.46 1997/01/24 16:48:06 nhall Exp $
 */
#include <copyright.h>

#ifdef __GNUG__
# pragma implementation "mount_m.h"
#endif

struct svc_req; // forward

#include <rsrc.h>
#include "mount_m.h"
#include "vaserr.h"
#include "smcalls.h"
#include "sysprops.h"
#include "svas_layer.h"

#ifdef __GNUG__
template class rsrc_m<mount_info, lvid_t>;
template class rsrc_i<mount_info, lvid_t>;
template class rsrc_t<mount_info, lvid_t>;
template class w_hash_t<rsrc_t<mount_info, lvid_t>, lvid_t>;
template class w_list_t<rsrc_t<mount_info, lvid_t> >;
template class w_list_i<rsrc_t<mount_info, lvid_t> >;
#endif

rsrc_m<mount_info, HASH_VALUE>* _table;

EXTERNC void init_mount_table(int);
EXTERNC void finish_mount_table();

void
init_mount_table(int n) {
	dassert(mount_table == NULL);
	mount_table = new mount_m(n);
	dassert(mount_table != NULL);
}

void
finish_mount_table() {
	FUNC(finish_mount_table);
	if(mount_table != NULL) {
		delete mount_table;
	}
	mount_table = NULL;
}
// rsrc_m<mount_info, HASH_VALUE>* mount_m::_table;

mount_m::mount_m(int max) : _max_mount(max), 
	_cur_mount(0),
	_current_cookie((Cookie)10000)
{
	FUNC(mount_m::mount_m(int max));

	DBG(<<"mount_m CONSTRUCTOR CALLED, max=" << max);
    _info = new mount_info [_max_mount];
 	_table = new rsrc_m<mount_info, HASH_VALUE>(_info, _max_mount, "mount_m");
	audit();
	DUMP(end of mount_m::mount_m(int max));
}

void
mount_m::dismount_all()
{
	FUNC(mount_m::dismount_all);
	audit();
	w_rc_t e;
	{
		rsrc_i<mount_info, HASH_VALUE> iter(*_table);

		DUMP(mount_m::dismount_all);
		mount_info* mount_info = NULL;
		while ( (mount_info = iter.next()) != NULL) {
			DBG(
			 << "Dismounting... " << mount_info->mountpoint
			)
			e = iter.discard_curr();
			if(e) {
				ShoreVasLayer.logmessage("mount_m::dismount",e);
			}
		}
	}
	audit();
}

mount_m::~mount_m()
{
	FUNC(mount_m::~mount_m);
	// TODO: tell other servers about our going down
	dismount_all();

	delete [] _info;
	audit();
	delete _table;
}

VASResult
mount_m::tmount( 
	const lvid_t    &lvid, 	// IN
	const serial_t	&rootoid,	// IN
	const serial_t	&rpfid, // IN
	const Path		_mntpnt,	// IN
	const lrid_t	mnt,	// IN
	bool			writable, // IN
	uid_t			uid 	// IN
)
{
	FUNC(mount_m::tmount(...));
	return __mount(mount_info::transient_mount,
		lvid, rootoid, rpfid, _mntpnt, mnt, writable, uid);
}
VASResult
mount_m::pmount( 
	const lvid_t    &lvid, 	// IN
	const serial_t	&rootoid,	// IN
	const serial_t	&rpfid, // IN
	const lrid_t	dir,	// IN
	const Path		lnk,	// IN
	bool			writable, // IN
	uid_t			uid 	// IN
)
{
	FUNC(mount_m::pmount(...));
	return __mount(mount_info::persistent_mount,
		lvid, rootoid, rpfid, lnk, dir, writable, uid);
}

VASResult
mount_m::__mount( 
	mount_info::mount_kind 		k,		
	const lvid_t    &lvid, 	
	const serial_t	&rootoid,
	const serial_t	&rpfid, 
	const Path		_path,   // meaning depends on mount_kind 
	const lrid_t	obj,	// ditto	
	bool			writable, 
	uid_t			uid 
)
{
	FUNC(mount_m::__mount(...));
	Path 		path = _path;
	bool		found, is_new;
	mount_info 	*mount_info = NULL;
	rc_t		err;

	if( strlen(path) > MNT_STR_LEN) {
		DUMP(returning OS_PathTooLong);
		RETURN OS_PathTooLong;
	}
	// get rid of leading '/'s
	while((*path == '/') && (*(path+1)=='/')) path++;

	string_t junk(path, string_t::OwnSpace); // get space 
	// the key value in junk will be used for the key in the table.

	audit();

	err = _table->grab(mount_info, lvid, found, is_new);
	if(err) {
		RETURN 	SVAS_SmFailure; // should not happen!
	}

	// WARNING: this must be unpinned before leaving outer scope

	if(found) {
		_table->unpin(mount_info);
		audit();
		 RETURN SVAS_Already;
	}
	assert(is_new); // there should be space

	mount_info->cookie = _current_cookie;
	_current_cookie += 1;
	mount_info->kind = k;
	mount_info->mnt = obj;
	mount_info->lvid = lvid;
	mount_info->root = rootoid;
	mount_info->reg_fid = rpfid;
	mount_info->use_count = 0;
	mount_info->uid = uid;
	mount_info->writable = writable;

	mount_info->mountpoint.steal(&junk);

	_table->publish(mount_info);
	_table->unpin(mount_info);
	audit();

	_cur_mount++;
	RETURN SVAS_OK;
}

VASResult
mount_m::_dismount(mount_info *mount_info)
{
	FUNC(mount_m::_dismount);
	rc_t	e;
	// mount_info must already be pinned
	assert(_table->pin_cnt(mount_info) > 0);

	if(mount_info->use_count > 0) {
		_table->unpin(mount_info);
		audit();
		RETURN OS_InUse;
	} else {
		// recycle space for string right away.
		mount_info->mountpoint.strcopy((Path)NULL, string_t::IsNil);
		// mount_info->devicename.strcopy((Path)NULL, string_t::IsNil);

		e = _table->remove(mount_info); // unpins it
		audit();

		_cur_mount--;
		if(!e) {
			RETURN SVAS_OK;
		}
	}
	dassert(e);
	ShoreVasLayer.catastrophic(
		"Please report this to shore_support@cs.wisc.edu.", e);
	return SVAS_FAILURE;
}

VASResult 
mount_m::find(	
	const Path	_path, 		// IN - exact match required
	bool		*iswritable, // OUT
	lvid_t		*lvid 		// OUT
)
{
	FUNC(mount_m::find);
	Path 			path = _path;
	bool 			found = false;
    mount_info* 	mount_info;

	// get rid of leading '/'s
	while((*path == '/') && (*(path+1)=='/')) path++;
	mount_info = _find(path, &found);

	if(found) {
		*iswritable = mount_info->writable;
		*lvid = mount_info->lvid;
		_table->unpin(mount_info);
		audit();
		RETURN SVAS_OK;
	} else {
		audit();
		RETURN SVAS_NotFound;
	}
}
VASResult 
mount_m::find(
	const lvid_t		&lvid, // IN
	serial_t			*const rpfid, // OUT
	bool				*const writable // OUT
)
{
	bool			found = true;
    mount_info* 	mount_info = _find(lvid, &found);

	if(mount_info == NULL || !found) {
		audit();
		RETURN SVAS_FAILURE;
	}
	*rpfid = 	mount_info->reg_fid;
	*writable = mount_info->writable;
	_table->unpin(mount_info);
	audit();
	RETURN SVAS_OK;
}

VASResult 
mount_m::namei(
	const Path		path, // IN
	bool		*iswritable, // OUT
	int			*prefix_len, // OUT
	lvid_t		*lvid, // OUT
	serial_t	*root,	// OUT
	serial_t	*reg_file	// OUT
)
{
	FUNC(mount_m::namei);
	bool 		found = false;
    mount_info* 	mount_info;

	audit();
	assert(*path != '/');

	mount_info = _namei(path, &found, prefix_len);
	// mount-info is pinned

	if(found) {
		*iswritable = mount_info->writable;
		*lvid = mount_info->lvid;
		*root = mount_info->root;
		*reg_file = mount_info->reg_fid;
		_table->unpin(mount_info);
		audit();
		RETURN SVAS_OK;
	} else {
		audit();
		RETURN SVAS_NotFound;
	}
}

VASResult 
mount_m::pmap(
	lrid_t		from, // IN
	lrid_t		*to, // IN (lvid, root)
	bool		*iswritable, // OUT
	serial_t	*reg_file	// OUT
)
{
	return _map(mount_info::persistent_mount, from, to, iswritable, reg_file);
}
VASResult 
mount_m::tmap(
	lrid_t		from, // IN
	lrid_t		*to, // IN (lvid, root)
	bool		*iswritable, // OUT
	serial_t	*reg_file	// OUT
)
{
	return _map(mount_info::transient_mount, from, to, iswritable, reg_file);
}
VASResult 
mount_m::_map(
	mount_info::mount_kind 		k,		
	lrid_t		from, // IN
	lrid_t		*to, // IN (lvid, root)
	bool		*iswritable, // OUT
	serial_t	*reg_file	// OUT
)
{
	FUNC(mount_m::_map);
	bool			found = false;
	{
    rsrc_i<mount_info, HASH_VALUE> iter(*_table); // when it goes out
		// of scope, it unpins whatever was pinned
    mount_info* mount_info = NULL;

	DBG(<<"map from= " << from);

    while ( (mount_info = iter.next()) != NULL) {

		DBG(<<"mount_info->mnt=" << mount_info->mnt << " from=" << from);
		if(mount_info->mnt == from && mount_info->kind == k) {
			found = true;
			DBG(<< "map found lvid=" 
				<< mount_info->lvid
				<< " root=" 
				<< mount_info->root
			)
			if(to) {
				to->lvid = mount_info->lvid;
				to->serial = mount_info->root;
			}
			if(reg_file) *reg_file = 	mount_info->reg_fid;
			if(iswritable) *iswritable = mount_info->writable;

			break;
		}
    }
	DBG(<<"map returns " 
		<< (char *)(found?" found" : "nothing"));
	}
	audit();
	RETURN (found?SVAS_OK:SVAS_FAILURE);
}
 
VASResult
mount_m::dismount(const lvid_t &lvid)
{
	FUNC(mount_m::_dismount);
	bool 		found;
    mount_info* 	mount_info = _find(lvid, &found);

	if(found) {
		RETURN _dismount(mount_info); // does the unpin
	} else
		RETURN SVAS_NotFound;
}

VASResult
mount_m::dismount(const Path _path, lvid_t *lvid)
{
	FUNC(mount_m::_dismount);
	Path		path = _path;
	bool 		found;

	// get rid of leading '/'s
	while((*path == '/') && (*(path+1)=='/')) path++;
    mount_info* 	mount_info = _find(path, &found);

	if(found) {
		*lvid = mount_info->lvid;
		RETURN _dismount(mount_info); // does the unpin
	} else {
		RETURN SVAS_NotFound;
	}
}

// leaves the mount_info that it returns pinned
mount_info *
mount_m::_namei(const Path path, bool *found, int *prefix_len)
{
	FUNC(mount_m::_namei);
	{
		rsrc_i<mount_info, HASH_VALUE> iter(*_table);
		string_t temp(path, string_t::CopyPtr);

		DBG(
			<< "mount_m:_namei looking for " << path
		)
		assert(*path != '.');

		if(*path == '/') {
			ShoreVasLayer.logmessage("why this assert?");
			assert(*path != '/');// WHY?
		}

		mount_info* mount_info = NULL;

		while ( (mount_info = iter.next()) != NULL) {
			if ( *prefix_len = mount_info->mountpoint <= temp) {
				// the prefix matches a full legit path portion
				// prefix_len does not include the '\0' or '/' 
				_table->pin(mount_info); // a second time
				// because the iterator unpins when it goes
				// out of scope
				*found = true;
				RETURN mount_info;
			}
		}
		*found = false;
	}
	audit();
	RETURN NULL;
}

// leaves mount_info pinned
mount_info *
mount_m::_find(const lvid_t& lvid, bool *found)
{
	FUNC(mount_m::_find);
	rc_t err;

	DBG(<<"searching for " << lvid);
    mount_info* mi = NULL;

	audit();

	err =  _table->find(mi, lvid); 
	if(err) {
		mi = 0;
	}

	if(mi) {
		DBG(<<"found " << lvid);
		*found = true;
	} else {
		DBG(<<"not found " << lvid);
		audit();
		*found = false;
	}
	RETURN mi;
}

// Find by MOUNT POINT (NOT DEVICE PATH NAME)
// leaves mount_info pinned
mount_info *
mount_m::_find(const Path _path, bool *found)
{
	FUNC(mount_m::_find);
	Path 		path = _path;
	mount_info 	*mi;
	rc_t		err;

	if( strlen(path) > MNT_STR_LEN) {
		*found = false;
		audit();
		RETURN NULL;
	}
	// get rid of leading '/'s
	while((*path == '/') && (*(path+1)=='/')) path++;

	string_t temp(path, string_t::CopyPtr);
    rsrc_i<mount_info, HASH_VALUE> iter(*_table);
    while ( (mi = iter.next()) != NULL) {

		DBG(<<"Comparing mountpoint " << mi->mountpoint
		<< " with path " << path);

		if (mi->mountpoint== path) {
			_table->pin(mi); // a second time
			// because iter unpins when it goes out of scope
			break;
		}
    }

	if(mi) {
		DBG(<<"Found " << mi->mountpoint );
		*found = true;
		// should be pinned
	} else {
		*found = false;
		audit();
	}
	RETURN mi;
}

mount_m *mount_table = NULL;

void
print_mount_table()
{
	FUNC(print_mount_table);
#ifdef DEBUG
	mount_table->audit();
#endif /*DEBUG*/
	mount_table->dump();
	mount_table->audit();
}

ostream &	
operator	<<(ostream &o, const mount_m &t) 
{
	FUNC(mount_m::operator<<);

#ifdef GNUG_BUG_13
	_table->dump(o,false);
#else
	o << *t._table ;
#endif
	o  << "<<end of mount table>>" << endl;
	return o;
}

void
mount_m::debug_dump()const
{
	const mount_m &m = *this;

	_table->dump(cerr,true); // DEBUG
}

void
mount_m::dump()const
{
	const mount_m &m = *this;
	cerr << m << endl; // DEBUG
}

ostream &	
operator	<<(ostream &o, const mount_info &m)
{
	FUNC(mount_info::operator<<);
	rc_t	smerrorrc;
	{
		smksize_t capacity, used;
		int	smerror = 0;

		DBG(<<"about to call vol_usage on " << m.lvid);

		if SMCALL(get_volume_quota(m.lvid, capacity, used)) {
			// 
			o << "Storage Manager error for " 
				<< m.mountpoint << ":" <<  smerrorrc << endl;
			return o; 
		}
		if(m.kind == mount_info::transient_mount) {
			o << "T: " << m.mountpoint 
				<< "(" << m.mnt << ")";
		} else {
			o << "P: " << m.mnt << ", " << m.mountpoint;
		}
		o << "\t is " << m.lvid << "." << m.root <<  endl; 

		o << "\tMounted " 
			<< ((m.writable)?" WR ":" RO ") << " by " << m.uid << endl; 

		o << "\tCapacity=" << capacity << "K used=" << used
			<< "K use_count=" << m.use_count << endl; 

#ifdef DEBUG
 		o	 << "\treg_fid=" << m.reg_fid
 			 << endl;
#endif
    }
	return o;
}

VASResult 
mount_m:: _cd(
	mount_info 		*mount_info, // IN
	ToFrom			toOrFrom	// IN  
)
{
	assert(_table->pin_cnt(mount_info) > 0);
	if(toOrFrom == To) {
		assert(mount_info->use_count >= 0);
		mount_info->use_count++;
	} else {
		assert(toOrFrom == From);
		assert(mount_info->use_count > 0);
		mount_info->use_count--;
	}
	_table->unpin(mount_info);
	audit();
	return SVAS_OK;
}

VASResult 
mount_m:: cd(
	const lvid_t	&lvid,		 // IN
	ToFrom			toOrFrom	// IN  
)
{
	FUNC(mount_m::cd);
	bool 			found;
    mount_info* 	mount_info = _find(lvid, &found);

	if(found)  {
		VASResult		res;
		res =  _cd(mount_info, toOrFrom);
		audit(); //_cd unpins it
		RETURN res;
	} else {
		audit();
		RETURN SVAS_NotFound;
	}
}

VASResult 
mount_m:: getmnt_info(
	FSDATA 			*fsd, 		// "INOUT"
	ObjectSize 		bufbytes, 	// "IN"
	int		 		*const	nresults, // "OUT"
	Cookie			*const cookie	// "INOUT"
)
{
	FUNC(mount_m::getmnt_info);
	audit();
	rc_t	smerrorrc;
	{
		rsrc_i<mount_info, HASH_VALUE> iter(*_table); // unpins when 
			// it goes out of scope
		mount_info* 	mount_info = NULL;

		(*nresults) = 0;

		assert(cookie != NULL);
		assert(bufbytes >= sizeof(FSDATA));

		if(*cookie != NoSuchCookie) {
			DBG(<< "*cookie=" << *cookie);
			while ( ((mount_info = iter.next()) != NULL) &&
				(mount_info->cookie != *cookie)) ;

			if(mount_info == NULL) {
				// cookie not found
				audit();
				RETURN SVAS_BadParam4;
			} 
			// Cookie was the last one returned, so get the next one!
			DBG(
				<< "incoming cookie gets us to " << mount_info->mountpoint
				<< " ... now get the next one."
			)
		} 
		mount_info = iter.next();
		if(mount_info != NULL) {
			// mount_info found, not null
			do {
				smksize_t capacity, used;
				int		kb_per_page = ss_m::page_sz/1024;
				int		smerror = 0;

				dassert(kb_per_page >= 1);

				if SMCALL(get_volume_quota(mount_info->lvid, capacity, used)) {
					// ?? ERR(SVAS_SmFailure);
					// not sure how to handle this error
					assert(0);
					RETURN SVAS_FAILURE;
				}
				dassert(capacity % kb_per_page == 0);
				dassert(used % kb_per_page == 0);

				fsd->blocks = (u_long) capacity/kb_per_page;

				fsd->bavail = fsd->bfree = 
					(u_long) (capacity - used)/kb_per_page;

				fsd->tsize = fsd->bsize = ss_m::page_sz;

				dassert(fsd->blocks != 0);
				dassert(fsd->blocks >= fsd->bfree);
				dassert(fsd->bfree >= fsd->bavail);

				fsd->root.serial = mount_info->root;
				fsd->root.lvid = mount_info->lvid;

				fsd->mnt = mount_info->mnt;
				fsd++;
				(*nresults)++;
				bufbytes -= sizeof(FSDATA);

			} while (  (bufbytes >= sizeof(FSDATA)) &&
					((mount_info = iter.next()) != NULL) );
		} else {
			DBG(
				<< "no more mount_infos"
			)
		}
	// done:
		if(mount_info != NULL)  {
			*cookie = mount_info->cookie;
			DBG(
				<< "MORE TO LIST ..."
			)
			assert((*nresults) != 0);
		} else {
			// end of list, so null out the cookie 
			*cookie = NoSuchCookie;
			DBG(
				<< "END OF LIST ... "
			)
		}
		DBG(
			<< " returning cookie" << ::hex((u_long)(*cookie))
			<< ", nresults= " << (*nresults)
		)
	}
	audit();
	RETURN SVAS_OK;
}

// EFSD version
VASResult 
mount_m:: getmnt_info(
	const lvid_t	&lvid, 	// in
	FSDATA *fsd 			// out
)
{
	bool			found = true;
    mount_info* 	mount_info = _find(lvid, &found);
	rc_t			smerrorrc;

	if(mount_info == NULL || !found) {
		audit();
		RETURN SVAS_FAILURE;
	}

	smksize_t capacity, used;
	int		kb_per_page = ss_m::page_sz/1024;
	int	 	smerror = 0;

	dassert(kb_per_page >= 1);

	if SMCALL(get_volume_quota(mount_info->lvid, capacity, used)) {
		// ?? ERR(SVAS_SmFailure);
		// not sure how to handle this error
		assert(0);
		RETURN SVAS_FAILURE;
	}
	dassert(capacity % kb_per_page == 0);
	dassert(used % kb_per_page == 0);

	fsd->blocks = (u_long) capacity/kb_per_page;
	fsd->bavail = fsd->bfree = 
		(u_long) (capacity - used)/kb_per_page;
	fsd->tsize = fsd->bsize = ss_m::page_sz;

	dassert(fsd->blocks != 0);
	dassert(fsd->blocks >= fsd->bfree);
	dassert(fsd->bfree >= fsd->bavail);

	fsd->root.serial = mount_info->root;
	fsd->root.lvid = mount_info->lvid;

	fsd->mnt = mount_info->mnt;

	_table->unpin(mount_info);
	audit();

	RETURN SVAS_OK;
}
#ifdef DEBUG
extern "C" void dumpthreads();
#endif

void 
mount_m::audit() const {
	int locks;
#ifdef DEBUG
	if((locks = _table->audit()) > 0) {
#ifdef GNUG_BUG_13
		_table->dump(cout, 0);
		cout << endl;
		dumpthreads();
#else
		cout << *_table << endl;
#endif

		dassert(locks == 0);
		assert(0);
	}
#endif
}
