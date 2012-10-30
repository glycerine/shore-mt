/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/cltab.C,v 1.25 1996/03/28 16:48:16 nhall Exp $
 */
#include <copyright.h>
#ifdef __GNUG__

# pragma implementation "cltab.h"
#endif

#include "externc.h"

#include "client.h"
#include "cltab.h"
#include "debug.h"

#ifdef __GNUG__
template class w_hash_t<client_t, int>;
template class w_hash_i<client_t, int>;
template class w_list_t<client_t>;
template class w_list_i<client_t>;
#endif

scond_t cleanup("cleanup.s");
smutex_t cleanup_mutex("cleanup.m");

cltab_t	* CLTAB=0;

cltab_t::cltab_t() : tab(ShoreVasLayer.OpenMax, offsetof(client_t,_fd),
					offsetof(client_t,hash_link)), 
			_mutex("cltab")  
{

	dassert(CLTAB == 0);
	CLTAB = this; // gets assigned by caller too- oh well


	CLTAB->cleaner_thread = new cleanup_client_t();
	if(!CLTAB->cleaner_thread) {
		W_FATAL(fcOUTOFMEMORY);
	}
	W_COERCE(CLTAB->cleaner_thread->fork());
}

cltab_t::~cltab_t()
{
	FUNC(~cltab_t)
    w_hash_i<client_t, int> iter(tab);
    client_t* p= NULL;

	DBG( << "~cltab_t" );

	dassert(this == CLTAB);

	dassert(CLTAB->cleaner_thread != 0);
	CLTAB->cleaner_thread->destroy();
	W_COERCE(cleaner_thread->wait());
	delete cleaner_thread;
	cleaner_thread = 0;

	while( (p = iter.next())  != NULL) {
		tab.remove(p);
		// defunct ones should have been cleaned up already
	    if	(p->status() != smthread_t::t_defunct) {
			p->shutdown();
		}
		DBG( << "~cltab_t: removing client_t for fd " << p->_fd );
		W_COERCE(p->wait());
		delete p;
	}
	DBG( << "~cltab_t: empty ");

	// there's only one!
	CLTAB = 0;
}

void cltab_t::dump(bool verbose)  
{
	FUNC(cltab_t::dump)
    w_hash_i<client_t, int> iter(tab); // generates warning
	// because we declared this method to be const. -- ignore
    client_t* p= NULL;

	while( (p = iter.next())  != NULL) {
		cout << "CLIENT:" << *p << endl;
	}
}

void cltab_t::_destroy(int s)
{
	FUNC(cltab_t::_destroy);
	client_t *p;
    p  = tab.remove(s);
	if(p)  {
		p->shutdown();
		DBG( << "cltab_t::_destroy: removing " );
		delete p;
	}
}

client_t* cltab_t::_find(int s)
{
    return tab.lookup(s);
}

int		
cltab_t::count()
{
	FUNC(cltab_t::count)
	int c=0;

    w_hash_i<client_t, int> iter(tab); // generates warning
	// because we declared this method to be const. -- ignore
    client_t* p= NULL;

	while( (p = iter.next())  != NULL) {
		c++; 
		// no longer true dassert(p->status() != smthread_t::t_defunct);
	}

	DBG(<<" count is " << c);
	return c;
}

void		
cltab_t::shutdown_all()
{
	FUNC(cltab_t::shutdown_all);

    w_hash_i<client_t, int> iter(tab); // generates warning
	// because we declared this method to be const. -- ignore
    client_t* p= NULL;

	while( (p = iter.next())  != NULL) {
		if(p->status() != smthread_t::t_defunct) {
			p->shutdown();
		}
	}
}

void		
cltab_t::destroy_defunct()
{
	FUNC(cltab_t::destroy_defunct);

    w_hash_i<client_t, int> iter(tab); // generates warning
	// because we declared this method to be const. -- ignore
    client_t* p= NULL;

	while( (p = iter.next())  != NULL) {
		if(p->status() == smthread_t::t_defunct) {
			tab.remove(p);
			p->shutdown();
			DBG( << "destroy_defunct: removing " );
			delete p;
		} else {
			DBG( << "destroy_defunct found non-defunct client "
				<< p->_fd );
		}
	}
}

cleanup_client_t::cleanup_client_t()
 : smthread_t( smthread_t::t_regular, // priority
				false, // block_immediate
				false, // auto_delete
				"cleanup_client_t", 	//thread name
				WAIT_FOREVER // don't block on locks
		)
{
    _quit = false;
}

void
cleanup_client_t::kick()
{
	cleanup.broadcast(); 
}

void
cleanup_client_t::destroy()
{
	_quit = true;
	cleanup.broadcast(); 
}

void
cleanup_client_t::run()
{
    while(1) {
		if(_quit) {
			return;
		}

		// wait on condition 
		w_rc_t e;
		W_COERCE(cleanup_mutex.acquire());
		if(e=cleanup.wait(cleanup_mutex)) {
			cerr << e << endl; assert(0);
		}
		cleanup_mutex.release();

		// clean up any defunct threads
		CLTAB->destroy_defunct();
	}
}
