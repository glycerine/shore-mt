/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 * $Header: /p/shore/shore_cvs/src/vas/client/sm_app.C,v 1.52 1997/01/24 16:49:17 nhall Exp $
 */
#include <copyright.h>

#ifndef CLIENT_ONLY
#define CLIENT_ONLY
#endif

#ifdef __GNUG__
// generate code for class record_t
#pragma implementation "file_s.h"
// generate code for class lpid_t
#pragma implementation "sm_s.h"
#endif

#include <sys/param.h>
#include <debug.h>
#include <vas_internal.h>
#include <vaserr.h>
#include <svas_layer.h>

#include "auditpg.h"

// TODO: convert to w_fastnew.h
#include <fast_new.h>

#ifdef DEBUG
struct {
int replace_page;
int invalidateht;
int invalidate_page1;
int invalidage_page2;
int invalidae_obj;
int insert;
int install_page;
int htaudit;
int installpage;
int _locate_object;
int invalidateobj;
int auditempty;
int putpage;
int svas_client_audit;
} counts = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0
};

#define COUNT(a)\
	static _cat(count_,a) = 0; _cat(count_,a) ++; _cat(counts.,a) ++;
#else
#define COUNT(a)
#endif

#ifdef notdef
BEGIN_EXTERNCLIST
    u_long localaddrshift(u_long); // in in.C
END_EXTERNCLIST
#endif


#ifdef DEBUG
void audit_hdr(int line, const void*, serial_t_data &, 
	bool have_pool = false);
#endif


// ************ htentry *****************************

class htentry {
public:
	w_link_t	hash_link;
	// const 	lrid_t& hash_key() { return obj; }

public:	
	const int	pgoffset; 
	bool		valid;
	const char 	*body; 
	const char 	*hdr; 
	const int 	blen;
	const int 	hlen;
	const lrid_t	obj;
	LockMode	lockmode;

public:
	htentry(const char *_b, int _blen, const char *_h, int _hlen, 
		const lrid_t o, int pgo)
		: 	body(_b), blen(_blen), hdr(_h), hlen(_hlen), 
			obj(o), valid(0), pgoffset(pgo), lockmode(NL) { }

	static mem_allocator_t *_mem_allocator;

	void * operator new(size_t s) { 
		assert(_mem_allocator != NULL);
		return _mem_allocator->mem_new(s); 
	}
	void	operator delete(void *p, size_t s) {
		assert(_mem_allocator != NULL);
		_mem_allocator->mem_delete(p,s); 
	}
};

#ifdef notdef
// since there's now a hash func in lid_t.h, we'll
// remove this but save it in case the other is
// too expensive or something
uint4 hash(const lrid_t &obj) {
	return ((unsigned int)obj.serial.guts._low>>1)
		// | (unsigned int)(obj.serial.guts._high)

		| localaddrshift(obj.lvid.high)
		;
}
#endif
mem_allocator_t *htentry::_mem_allocator = NULL;
 
// ************** ht:: *************************

#ifdef HPUX8
#define FDMASK_IS_INT 1
#define FDMASK_IS_SET 0
#else
#define FDMASK_IS_INT 0
#define FDMASK_IS_SET 1
#endif

#if FDMASK_IS_INT
#define ISSET(x,i) (x & (1<<(i)))
#define SET(x,i) (x |= (1<<(i)))
#define CLR(x,i) (x &= ~(1<<(i)))
#define ZERO(x) x =0
#else
#define SET(x,i) FD_SET(i, &x)
#define CLR(x,i) FD_CLR(i, &x)
#define ISSET(x,i) FD_ISSET(i,&x)
#define ZERO(x) FD_ZERO(&x)
#endif

static int 
_ffs(
#if FDMASK_IS_INT
	unsigned long i,
#else
	const fd_set &i,
#endif
	int 	mx
)
{
	for(int j=0; j < mx ; j++) {
		if(ISSET(i,j)) return j;
	}
	return -1;
}

class _ht {

	friend class svas_client;

	const shore_file_page_t	*page_buf;
	int		npgs; 
	int		last_page_replaced;
	w_hash_t <htentry, lrid_t> tab;

	// bit masks:
#if FDMASK_IS_INT
	unsigned long	
#else
	fd_set
#endif
			allpages, invalid_pages, installed_pages
			, hasinvalidobj 
	;

	enum { maxpgs = 8 * sizeof(
#	if FDMASK_IS_INT
		int
#	else
		fd_set
#	endif
		) }; // n bits per word

	struct	log_pid_t {
		lvid_t	lvid;
		shpid_t	page;
		snum_t	store;
		unsigned int csum;
	} *pageinfo;

private: 
	// disable copy constructor
	_ht(const _ht &);
public:
	_ht(int n, int slots, const shore_file_page_t	*pb);
	~_ht();
	inline bool	none_valid() const { 
#if FDMASK_IS_INT
		return ( invalid_pages==installed_pages);
#else
			for(int i = 0; i < howmany(FD_SETSIZE, NFDBITS); i++) {
				if(invalid_pages.fds_bits[i] != installed_pages.fds_bits[i])
					return false;
			}
			return true;
#endif
	}
	inline bool	none_installed() const { 
#if FDMASK_IS_INT
		return (installed_pages==0);
#else
		return _ffs(installed_pages, maxpgs)==-1;
#endif
	}
	inline bool	is_installed(int i) const { 
		return ISSET(installed_pages,i);
	}
	inline bool	not_installed(int i) const {  
		return !is_installed(i); 
	}
	inline bool	is_valid(int i)  const { 
		return is_installed(i) && !ISSET(invalid_pages,i);
	}
	inline bool	not_valid(int i) const {  
		return !is_valid(i); 
	}
	inline bool	has_invalid_obj(int i) const { 
		return (is_installed(i) && ISSET(hasinvalidobj,i));
	}
	void oids_on_page(const shore_file_page_t *pg,
		int &count, lrid_t *list) const; 

	inline const shore_file_page_t *page(int i) const {
		assert(i < npgs && i >= 0);
		return page_buf + i;
	}
	inline const shore_file_page_t	*newest_page() const {  
		return page(last_page_replaced); 
	}
	int		replace_page(); // choose a page to throw away
	void 	invalidate(); 	// whole table; remove entries
	void 	invalidate_page(IN(lrid_t) obj, bool remove); 
	void 	invalidate_page(int offset, bool remove = true);
	void 	invalidate_obj(htentry *, bool remove);
	void 	invalidate_obj(IN(lrid_t) obj, bool  remove);
	htentry *find(const lrid_t &o) const {
		htentry *hte;
		hte = tab.lookup(o);
		if(hte) { 
			return hte;
		} else {
			return 0;
		}
	}
	void	insert(const char *b, smsize_t bleng, 
		const char *h, smsize_t hleng, 
		const lrid_t &obj, int);
	int 	install_page(IN(lrid_t) obj, int offset);
	void	audit(bool empty);
	unsigned int checksum( const shore_file_page_t		*page);
};

_ht::_ht(int n, int slots, const shore_file_page_t *pb) : 
	page_buf(pb),
	npgs(n), 
	last_page_replaced(0),
	tab(npgs*slots, offsetof(htentry,obj),offsetof(htentry,hash_link)),  
	pageinfo(0)
{
	FUNC(_ht::_ht);
#ifdef DEBUG
	assert(npgs <= maxpgs);
#endif

	pageinfo = new log_pid_t[npgs];

	if(htentry::_mem_allocator != NULL) {
		delete htentry::_mem_allocator;
	}
	htentry::_mem_allocator =  new 
		mem_allocator_t(sizeof(htentry), slots, NULL);
	assert(htentry::_mem_allocator != NULL);

	memset(pageinfo, '\0', sizeof(log_pid_t)*npgs);

	ZERO(invalid_pages);
	ZERO(installed_pages);
	ZERO(hasinvalidobj);
	ZERO(allpages);
	for(int i=0; i<npgs; i++)  {
		SET(allpages,i);
	}
	// assert(allpages != 0);
}

_ht::~_ht() 
{
	w_hash_i <htentry, lrid_t> iter(tab);
	htentry	*hte;

	while(hte = iter.next()) {
		tab.remove(hte);
		delete hte;
	}
	assert (pageinfo!=NULL);
	delete[] pageinfo;
	pageinfo = 0;

	delete htentry::_mem_allocator;
	htentry::_mem_allocator=0;
}

unsigned int
_ht::checksum(const shore_file_page_t		*page)
{
	unsigned int *p = (unsigned int *)page; // we know
							// that the page is aligned
	unsigned int	i=0;
	assert( (((unsigned int)p) & 0x3)==0);
	for(int j = 0; j < sizeof(shore_file_page_t);
		j+= sizeof(unsigned int), p++) { i += *p; }
	return  i;
}

int	
_ht::replace_page() 
{
	FUNC(_ht::replace_page);
	COUNT(replace_page);
	// if none, find a page with  at least one invalid object

#if defined(DEBUG) && FDMASK_IS_INT
	assert(allpages != 0);
	DBG(<< "allpages " << hex(allpages) 
		<< " invalid_pages " << hex(invalid_pages)
		<< " installed_pages " << hex(installed_pages)
		<< " hasinvalidobj " << hex(hasinvalidobj)
		);
#endif

	int	pgo =  _ffs(invalid_pages, maxpgs);

#ifdef DEBUG
	if(pgo==-1) {
		// no completely invalid pages
		pgo = _ffs(hasinvalidobj, maxpgs);
	}
#endif

	if(pgo==-1)  {
		//	last_page_replaced = (++last_page_replaced)%npgs :
		// cheaper than modulo: 

		if( ++last_page_replaced >= npgs ) 
			last_page_replaced = 0;
		pgo  = last_page_replaced;
	} 

	DBG( << "Choosing page " << pgo << " for replacement");
	RETURN pgo;
}

void 	
_ht::invalidate() 
{
	FUNC(_ht::invalidate);
	COUNT(invalidateht);
	// invalidate & remove all the objects & pages 

	for(int i=0; i<npgs; i++) {
		invalidate_page(i, true);
	}
	assert(none_valid());
	// would like to assert that the _ht is empty
}

void 	
_ht::invalidate_page(
	int		pgoffset,
	bool		remove
) 
{
	FUNC(_ht::invalidate_page);
	COUNT(invalidate_page1);
	// invalidate all the objects on the page 
	// and also invalidate the page.

	DBG(<<"_ht::invalidate_page " << pgoffset << " remove=" << remove
		<< " addr=" << ::hex((unsigned int)(page_buf + pgoffset)));

	if(!is_installed(pgoffset))  {
		assert(not_valid(pgoffset));
		RETURN;
	}

	// invalidate each object on the page
	const shore_file_page_t		*page = page_buf + pgoffset;
#ifdef DEBUG
	if(ShoreVasLayer.do_audit) audit_pg(page);
#endif

	DBG( << "Invalidating page at offset " << pgoffset
		<< " addr=" << ::hex((unsigned int)page));

#ifdef DEBUG
	// a few debugging assertions about the page

	assert( (*((lvid_t *)(&page->lsn2)) == 
		pageinfo[pgoffset].lvid));
	assert(pageinfo[pgoffset].csum == checksum(page));
	assert(sizeof(page_s) >  page->nslots);
	if(pageinfo[pgoffset].lvid == lvid_t::null) {
		assert( not_valid(pgoffset) );
	}
#endif

	rec_i				iter(page);
	record_t			*rec;
	lrid_t				temp;

	temp.lvid = *((lvid_t *)(&page->lsn2));

	while(rec = iter.next_small()) {
		temp.serial = rec->tag.serial_no;
		invalidate_obj(temp, remove);
	}
	if(remove) {
		CLR(installed_pages, pgoffset);
		CLR(invalid_pages, pgoffset);
		CLR(hasinvalidobj, pgoffset);
	} else {
		assert(is_installed(pgoffset));
		SET(invalid_pages, pgoffset);
	}
#ifdef DEBUG
	if(ShoreVasLayer.do_audit) audit_pg(page);
#endif
}

void 	
_ht::invalidate_page(
	IN(lrid_t) obj, 
	bool remove
) 
{
	FUNC(_ht::invalidate_page);
	COUNT(invalidage_page2);
	// invalidate all the objects on the page on which this
	// object resides; invalidate the page also

	// find out what page it's on
	htentry *hte = tab.lookup(obj);

	assert(hte!=NULL); // TODO: decide if this is an error
	if(hte==NULL) {
		RETURN;
	}

	assert(pageinfo[hte->pgoffset].lvid == obj.lvid);

	invalidate_page(hte->pgoffset, remove);
}

void 	
_ht::invalidate_obj(htentry *hte, bool remove)
{
	FUNC(_ht::invalidate_obj2);
	DBG(<<"_ht::invalidate_obj2 remove=" << remove);

	if(hte) {
#ifdef DEBUG
		serial_t_data pool;
		audit_hdr(__LINE__,hte->hdr, pool, false); 
#endif
		hte->valid = false;
		DBG( << "Invalidating object " << hte->obj
			<< " on pagebuf " <<  hte->pgoffset
			<< " hte=" << hex((u_long)hte)
		);
		SET(hasinvalidobj, hte->pgoffset);
		if(remove) {
			tab.remove(hte);
			delete hte;
		}
	}
}
void 	
_ht::invalidate_obj(IN(lrid_t) obj, bool remove)
{
	FUNC(_ht::invalidate_obj);
	DBG(<<"_ht::invalidate_obj " << obj << " remove=" << remove);

	COUNT(invalidae_obj);
	// invalidate this object only
	// mark the page as having an invalid object
	htentry *hte = tab.lookup(obj);

	invalidate_obj(hte, remove);
}

void	
_ht::insert(const char *b, smsize_t blen, const char *h, smsize_t hlen, 
	const lrid_t &obj, int pgo) 
{
	FUNC(_ht::insert);
	COUNT(insert);

	DBG( << "Inserting object " << obj
		<< " body len=" << blen
		<< " hdr len=" << hlen
		<< " in page buf " << pgo
	);
	htentry *hte;
	if(hte = find(obj)) {
		invalidate_obj(hte, true);
		// TODO: re-use this 
		// slot rather than removing and re-creating
		// it
		DBG(<<"PERFORMANCE HACK NEEDED HERE");
	}
	hte = new htentry(b, blen, h, hlen,  obj, pgo);
	assert(hte != NULL);
	hte->valid = true;
#ifdef DEBUG
	serial_t_data pool;
	audit_hdr(__LINE__,h, pool, false); 
#endif
	tab.push(hte);
}

int
_ht::install_page(
	IN(lrid_t) 	obj,  	// contains logical vid
	int		i	// page offset from page_buf
) 
{
	FUNC(_ht::install_page);
	COUNT(install_page);
	const shore_file_page_t		*page = page_buf+i;
	int				count=0;

	DBG( << "Installing page " << 
		*((lvid_t *)(&page->lsn2)) 	
		<< "." << page->pid.store()
		<< "." << page->pid.page 
		<< " in page buf " << i
		<< " with " << page->nslots
		<< " slots."
	);

#ifdef DEBUG
	if(ShoreVasLayer.do_audit) audit_pg(page);
#endif

	dassert(sizeof(page_s) >  page->nslots);
	dassert( not_valid(i) );

	assert( *((lvid_t *)(&page->lsn2)) == obj.lvid );

	// First, find out if there are any *other* copies
	// of this page around, and if so, invalidate them.
#ifdef DEBUG
	bool found=false;
#endif
	for( int j=0; j<npgs; j++) {
		if(j==i) continue;
#ifdef DEBUG
		DBG(<<"checking pageoffset " << j 
			<< pageinfo[j].lvid << "."
			<< pageinfo[j].store << "."
			<< pageinfo[j].page);

		{
			const shore_file_page_t		*pp = page_buf+j;
			dassert(pageinfo[j].csum == checksum(pp));
		}

#endif
		if( (pageinfo[j].page == page->pid.page) &&
			(pageinfo[j].store == page->pid.store()) &&
			(pageinfo[j].lvid == *((lvid_t *)(&page->lsn2))) ) {

#ifdef DEBUG
			DBG( << "Found same page in pgbuf at offset " << j  );
			if(is_valid(j)) {
				DBG( << " ... needs invalidating... " );
				assert(!found);
				found = true; // can be only one
			}
#endif
			if(is_valid(j)) {
				invalidate_page(j,true);
#ifndef DEBUG
				// can happen to only one page,
				// so no need to continue the loop.
				break;
#endif
			}
		}
	}

	// Now install each object on the page.
	// If it's already in the hash table and valid,
	// remove it and re-install it.  (Necesary
	// because it might have moved from another
	// page onto this page while sitting on the server.)

	pageinfo[i].lvid = *((lvid_t *)(&page->lsn2));
	pageinfo[i].page = page->pid.page;
	pageinfo[i].store = page->pid.store();
	pageinfo[i].csum = checksum(page);

	assert(pageinfo[i].lvid == obj.lvid);

	CLR(hasinvalidobj,i);
	CLR(invalid_pages,i);
	SET(installed_pages,i);

	rec_i				iter(page);
	record_t			*rec;
	lrid_t				temp = obj;

#ifdef DEBUG
	serial_t_data	pool; bool have=false;

	// initialze pool for purify
	memset((void *)&pool, '\0', sizeof(pool));
#endif
	while(rec = iter.next_small()) {
		temp.serial = rec->tag.serial_no;

		DBG( << "Installing small object " 
			<< " serial " << temp.serial
			<< " from pageoffset " << i
		);
#ifdef DEBUG
		audit_hdr(__LINE__,rec->hdr(), pool, have); have = true;
#endif
		insert(rec->body(), (smsize_t)rec->body_size(),
			rec->hdr(), (smsize_t)rec->hdr_size(), temp, i);
#ifdef DEBUG
		audit_hdr(__LINE__,rec->hdr(), pool, have); 
#endif
		count++;
	}
	DBG(<<"installed page w/ " << count << " slots");
#ifdef DEBUG
	if(ShoreVasLayer.do_audit) audit_pg(page);
#endif

	assert(count > 0);
	RETURN count;
}

#ifdef DEBUG
#include <sysprops.h>
void
audit_hdr(
	int				line,
	const void		*arg,
	serial_t_data 	&pool,
	bool			have_pool
)
{
	if(!ShoreVasLayer.do_audit) return;
	//
	// for the time being, assume we don't have any byte-swapping
	// to do ...
	// because we don't have it implemented
	//
	{
	    unsigned long a = 0x80000100;
	    unsigned long b = htonl(a);
	    if(a != b) {
		// can't audit -- no swapping
		return;
	    }
	}

	DBG(<<"audit hdr at line " << line << " "
		<< ::hex((unsigned int)arg) 
		<< " , have_pool=" << have_pool << ", pool= " << pool);

	_sysprops *s = (_sysprops *)arg;
	RegProps *r;
	AnonProps *a;
	int sz;
	ObjectOffset tstart;
	int nindexes;
	ObjectKind tag = sysp_split(*s, &a, &r, &tstart, &nindexes, &sz);

	if(have_pool) {
		assert(pool._low & 0x1);
	}

	switch(tag) {
		case KindAnonymous:
			if(have_pool) {
				assert(a->pool == pool);
			} else {
				pool = a-> pool;
			}
			break;
		case KindRegistered:
			// should not be found on these pages
			DBG(<<"unexpected (registered) tag value: " <<s->common.tag);
			break;
		default:
			DBG(<<"garbage tag value: " <<s->common.tag);
			assert(0);
	}
	assert(pool._low & 0x1);
	assert(s->common.type._low & 0x1);
	assert(((int)s->common.csize)>=0); // for now
	assert(((int)s->common.hsize)>=0); // for now
}

void
audit_pg(
	const shore_file_page_t	*pg
)
{
	int			_slot = pg->nslots;
	int         nslots=0;

	if(!ShoreVasLayer.do_audit) return;

	assert(sizeof(page_s) >  pg->nslots);

	while ( --_slot >= 0 ) {
			// offset should be -1 (not in use)
			// or 8-byte aligned (in use)

			assert((pg->slot[-_slot].offset == -1) ||
				((pg->slot[-_slot].offset & 0x7)==0));
			assert(pg->slot[-_slot].length <= page_s::data_sz);
			if((pg->slot[-_slot].offset & 0x7)==0) {
				nslots ++;
			}
	}
	assert(nslots>0);
}
#endif 

void
_ht::audit(bool empty) 
{
	FUNC(_ht::audit);
	COUNT(htaudit);
	const shore_file_page_t	*pg = page_buf;
	int			i,nslots,sslots,aslots,
				inv_slots,total_slots=0;
	lrid_t			temp;
	int 		total_entries = 0;

	if(!ShoreVasLayer.do_audit) return;

	{	// allpages must reflect the number of
		// pages in the page pool and all the low bits
		// must be set, none others set
		int j=0,k=0;
#if FDMASK_IS_INT
		unsigned int z = (unsigned int)allpages;

		for(i=0; z != 0 ; i++) {
			if(z&1)  j++;
			else  k++;
			z >>= 1;
		}
#else

		for(i=0; i < maxpgs ; i++) {
			if(ISSET(allpages,i))  j++;
							else  k++;
		}
#endif
		assert(j==npgs);
		assert(i==j+k);
		assert(i==maxpgs);
	}

	{
	unsigned int csum;
	// every slot in all the installed pages must be in the ht
	// even if not valid
	for(i=0; i<npgs; i++,pg++) {
		if( not_installed(i) ) 
			continue;

		assert(sizeof(page_s) >  pg->nslots);
		assert(pg->nslots>0);

		csum = checksum(pg);

		nslots = pg->nslots;
		sslots = 0;
		aslots = 0;
		inv_slots = 0;

		assert(pageinfo[i].lvid == *((lvid_t *)(&pg->lsn2)));
		assert(pageinfo[i].page == pg->pid.page);
		assert(pageinfo[i].store == pg->pid.store());
		assert(pageinfo[i].csum == csum);

		temp.lvid = pageinfo[i].lvid;

		rec_i		iter(pg);
		record_t	*rec;
		htentry		*hte;

		while (rec = iter.next()) {
			aslots++;

			if(rec->is_large()) continue;
			sslots++;

			temp.serial = rec->tag.serial_no;
			DBG(<<"record: " << temp.serial);
			if((hte = find(temp)) && (hte->valid) &&
				(hte->pgoffset == i) // !=i means this is is left
				// over from a pre-forwarded version. The object
				// was since invalidated but the page is still 
				// around.
			) {
				assert(hte->hdr == rec->hdr());
				assert(hte->hlen == rec->hdr_size());
				assert(hte->body == rec->body());
				assert(hte->blen == rec->body_size());
			} else {
				inv_slots++;
			}
			assert(iter.slot()>=aslots);
			// aslots counts used ones, slot() counts unused ones as well
		}
		total_slots += sslots;

		assert(sslots <= nslots);
		// aslots only reflects the small ones and the
		// ones in use
		assert(aslots <= nslots);

		if(inv_slots>0) {
			assert(has_invalid_obj(i));
		}
		if(has_invalid_obj(i)) {
			assert(inv_slots>0);
		}
		if(inv_slots == nslots) {
			assert( not_valid(i) );
		}

//		There is no way to tell when to invalidate the page (if 
//		we were invalidating one object at a time
//		if(inv_slots == aslots) {
//			assert( not_valid(i) );
//		}

		if( not_valid(i) ) {
			assert(inv_slots==aslots);
		}
	}
	}

	{
	// everything in the hash table had better be in the 
	// page cache
	w_hash_i <htentry, lrid_t> iter(tab); // warning -- ignore
	htentry	*hte;

	while (hte = iter.next()) {
		total_entries++;

		// get page it's on
		pg = page_buf + hte->pgoffset;

		assert(pageinfo[hte->pgoffset].lvid == *((lvid_t *)(&pg->lsn2)));
		assert(pageinfo[hte->pgoffset].page == pg->pid.page);
		assert(pageinfo[hte->pgoffset].store == pg->pid.store());
		assert(hte->obj.lvid == *((lvid_t *)(&pg->lsn2)));

		assert((char *)(hte->body) > (char *)pg);
		assert((char *)(hte->hdr)+(hte->hlen) < (char *)(hte->body) );
		assert((char *)(hte->body)+(hte->blen) < (char *)pg+
			sizeof(page_s));
		assert((char *)(hte->body) < 
			(char *)pg + sizeof(shore_file_page_t));
	}
	}

	// we might have invalidated tons of entries
	// and thrown them out of the table, but their
	// records are still in the page cache:
	assert(total_entries <= total_slots);
	if(empty) {
		assert( total_entries == 0);
		assert( none_installed() );
		assert( none_valid() );
	}
}

void
_ht::oids_on_page(const shore_file_page_t *pg, int &count, lrid_t *list) const 
{
	rec_i		iter(pg);
	record_t	*rec;
	lrid_t		temp;
	htentry		*hte;

	temp.lvid = *((lvid_t *)(&pg->lsn2));
	count = 0;
	while (rec = iter.next()) {
		if(rec->is_large()) continue;

		temp.serial = rec->tag.serial_no;
		if( (hte = find(temp)) && (hte->valid) ) {
			list[count++]=temp;
		}
	}
	dassert(count <= pg->slot_count());
}

// ************* VAS:: **********************

VASResult
svas_client::createht( 
	int	nbufs,
	const char  *pb	// in
)
{
	FUNC(svas_client::createht);

	dassert(sizeof(page_s) == sizeof(shore_file_page_t));

	int htsize  = nbufs * 
		(page_s::data_sz / 
			/* tag + slot + anon_sysprops */
			(sizeof(rectag_t) + sizeof(int) + 32 
			/* 32==sizeof(anon_sysprops), 8-byte aligned */
			)
		);

	DBG(
		<< " page_s::data_sz=" << page_s::data_sz
		<< " page_s::max_slot=" << page_s::max_slot
		<< " htsize=" << htsize
	);

	dassert(pb != 0);

	// get warning about alignment reqmts for pb
	ht = new _ht(nbufs, htsize, (shore_file_page_t *)pb);
	if(ht==NULL) {
		VERR(SVAS_MallocFailure);
		RETURN SVAS_FAILURE;
	}
	RETURN SVAS_OK;
}

VASResult
svas_client::destroyht()
{
	FUNC(svas_client::destroyht);
	if(ht) {
		if(over_the_wire()) {
			free((void *)ht->page_buf);
			ht->page_buf = 0;
		}
		delete ht;
		ht = 0;
	}
	_num_page_bufs = 0;
	_num_lg_bufs = 0;
	RETURN SVAS_OK;
}

VASResult		
svas_client::num_cached_oids(
	OUT(int)			count
) 
{
	FUNC(svas_client::num_cached_oids);

	if(count==NULL) {
		VERR(SVAS_BadParam1);
		RETURN SVAS_FAILURE;
	}
	const shore_file_page_t *pg = ht->newest_page();
	*count = pg->slot_count();
	RETURN SVAS_OK;
}
VASResult		
svas_client::cached_oids(
	INOUT(int)			count,
	INOUT(lrid_t)		list
) 
{
	FUNC(svas_client::cached_oids);
	const shore_file_page_t *pg = ht->newest_page();

	ht->oids_on_page(pg, *count, list);

	RETURN SVAS_OK;
}
VASResult
svas_client::installpage(
	IN(lrid_t) 	obj,  	// contains logical vid
	int		i	// page offset from page_buf
) 
{
	FUNC(svas_client::installpage);
	COUNT(installpage);

	if(ht) {
		_stats.page_installs++;
		_stats.htinserts += ht->install_page(obj,i);
	} else {
		// this should not get called unless small object page
		// was sent, which should happen only if 
		// num_page_bufs() > 0, in which case, 
		// ht was created.
		assert(0);
	}
	audit("installpage-exit");
	RETURN SVAS_OK;
}

#define OBJ 1
#define HDR 2

VASResult
svas_client::locate_object(
	IN(lrid_t)			obj,
	ccaddr_t 			&loc, // OUT
	LockMode			lockmode,
	OUT(ObjectSize) 		len,
	OUT(bool)			found
)
{
	return _locate_object(OBJ, obj, loc, lockmode, len, found);
}

VASResult
svas_client::locate_header(
	IN(lrid_t)			obj,
	ccaddr_t 			&loc, // OUT
	LockMode			lockmode,
	OUT(ObjectSize) 		len,
	OUT(bool)			found
)
{
	FUNC(svas_client::_locate_header);
	return _locate_object(HDR, obj, loc, lockmode, len, found);
}
VASResult
svas_client::_locate_object(
	int					which,
	IN(lrid_t)			obj,
	ccaddr_t 			&loc, // OUT
	LockMode			lockmode,
	OUT(ObjectSize) 		len,
	OUT(bool)			found
)
{
	FUNC(svas_client::_locate_object);
	COUNT(_locate_object);

	DBG(<<"_locate_object " << obj << (char *)((which==OBJ)?"BODY":"HDR") );

	if(ht==NULL) {
		*found = false;
		RETURN SVAS_OK;
	}

	assert(ht!=NULL);
	if(ht->none_valid()) {
		DBG(<< "none valid for ht");
		*found = false;
		RETURN SVAS_OK;
	}

	_stats.page_lookups++;
	htentry	*hte = ht->find(obj);

	if(hte) {
		if(hte->valid) {
			assert(hte->obj.serial == obj.serial);
			DBG(<< "is valid, found");

			loc = (which==OBJ)?hte->body:hte->hdr;
			*len = (which==OBJ)?hte->blen:hte->hlen;
#ifdef DEBUG
			serial_t_data p;
			if(which==HDR) {
				audit_hdr(__LINE__,loc, p, false);
			}
#endif
			_stats.page_hits++;
			if(hte->lockmode < lockmode)
				hte->lockmode = lockmode;
			*found = true;
		} else {
			DBG(<< "not valid, therefore not found");
			*found = false;
			_stats.page_invalid_hits++;
		}
	} else {
		DBG(<< "not found");
		*found = false;
		_stats.page_misses++;
	}
	RETURN SVAS_OK;
}

VASResult
svas_client::invalidateobj(
	IN(lrid_t) 	obj,
	bool		remove
)
{
	FUNC(svas_client::invalidateobj);
	COUNT(invalidateobj);
	// TODO: remove 2nd argument to this function 
	// since it's ignored for now
	if(ht) ht->invalidate_obj(obj, true);
	RETURN SVAS_OK;
}

#ifdef notdef
VASResult
svas_client::invalidatepage(
	IN(lrid_t) 	obj,
	bool		remove
)
{
	FUNC(svas_client::invalidatepage);
	COUNT(invalidatepage);
	if(ht) { 
		ht->invalidate_page(obj, remove); 
		_stats.page_invalidates++;
	}
	RETURN SVAS_OK;
}
#endif

static const char *auditkind=0;
VASResult
svas_client::audit(const char *msg) // == internal
{
	FUNC(svas_client::audit);
	COUNT(svas_client_audit);

	auditkind = msg;
	if(!ShoreVasLayer.do_audit) return SVAS_OK;
	if(ht) {
		if(!over_the_wire()) assert(ht->npgs!=0);
		ht->audit(false);
	}
	RETURN SVAS_OK;
}
VASResult
svas_client::auditempty()
{
	FUNC(svas_client::auditempty);
	COUNT(auditempty);
	if(!ShoreVasLayer.do_audit) return SVAS_OK;
	if(ht) {
		if(!over_the_wire()) assert(ht->npgs!=0);
		ht->audit(true);
	}
	RETURN SVAS_OK;
}

char	*
svas_client:: putpage(
	int		pgoffset,
	char 		*from,
	smsize_t  	len	
) 
{
	FUNC(svas_client::putpage);
	COUNT(putpage);

	// should not get called unless a small obj page was sent, in
	// which case, we should have created a hash table...
	assert(ht!=NULL);

	const shore_file_page_t 	*to =  ht->page(pgoffset); // warning -- ignore

	// since the page indicated didn't get removed 
	// by replace_page()

	assert(over_the_wire());
	ht->invalidate_page(pgoffset,true);
	_stats.page_invalidates++;

	//assert that len is a multiple of page size
	assert((len/sizeof(shore_file_page_t))*sizeof(shore_file_page_t)==len);

	DBG(  << hex << "copying into page buf" 
		<< (unsigned)to << " from " 
		<< (unsigned)from
	);
	memcpy((void *)to, (void *)from, (unsigned int)len);

	RETURN from;
}
int	
svas_client::replace_page() 
{
	FUNC(svas_client::replace_page);
	COUNT(replace_page);
	int offset;

	if(ht) {
		offset = ht->replace_page();

		// if over-the-wire, wait until page comes (it might not)
		// before invalidating it.
		// if in shm, we have no choice - we have to invalidate
		// it now because by the time the page is re-used, it's too late.

		ht->invalidate_page(offset,!over_the_wire());
		_stats.page_invalidates++;
		DBG(<<"replace_page chose " << offset);
	} else {
		DBG(<<"No pages for replace_page");
		offset = 0;
	}
	RETURN  offset;
}

void
svas_client::invalidate_ht() 
{
	FUNC(svas_client::invalidate_ht);
	if(ht) {
		if(ShoreVasLayer.do_audit) audit();
		ht->invalidate();
		if(ShoreVasLayer.do_audit) auditempty();
	}
	RETURN;
}
#ifdef __GNUG__
template class w_hash_t <htentry, lrid_t>;
template class w_hash_i <htentry, lrid_t>;
template class w_list_t<htentry>;
template class w_list_i<htentry>;
#endif

/* STATISTICS ***********************************************************/


VASResult	
svas_client::gatherRemoteStats(w_statistics_t &_remote_stats )
{
	FUNC(svas_client::remoteStats);

	if(_remote_stats.writable()) {
		_DO_(gather_remote(_remote_stats));
	} else {
		VERR(OS_NotEmpty); 
	}
	RETURN SVAS_OK;
failure:
	RETURN SVAS_FAILURE;
}
