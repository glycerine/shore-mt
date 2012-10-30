/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __CLTAB_H__
#define __CLTAB_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/cltab.h,v 1.22 1996/03/26 17:10:10 nhall Exp $
 */
#include <copyright.h>
#include <debug.h>

#include "client.h"

extern int OpenMax;

#ifdef __GNUG__
# pragma interface
#endif
class cltab_t {
private:
    void _destroy(int s);
    client_t* _find(int s);
    w_hash_t<client_t, int> tab;
	smutex_t	_mutex; 
private:
	// hide this so the compiler doesn't generate one (gak)
    cltab_t(const cltab_t &);
public:
	static cltab_t *cltab; // there's only one!

	// thread that will be cleaning up the client table in
	// the background
	cleanup_client_t *cleaner_thread; 

#define CLTAB cltab_t::cltab

    cltab_t();
	~cltab_t();
	void	destroy_defunct();
	void	install(client_t *p) {
		w_rc_t e;
		if(p) {
			if(e = _mutex.acquire()) {
#ifdef DEBUG
				cerr << "could not acquire mutex" << endl; assert(0);
#else
				assert(0);
#endif /*DEBUG*/
			}
			tab.push(p);
			_mutex.release();
		} else {
			assert(0);
		}
	}
	client_t *remove(int s) {
			client_t *c;
			w_rc_t	e;
			if(e=_mutex.acquire()) {
#ifdef DEBUG
				cerr << "could not acquire mutex" << endl; assert(0);
#else
				assert(0);
#endif /*DEBUG*/
			}
			c  = tab.remove(s);
			_mutex.release();
			return c;
	}
    void destroy(int s)		{ 
			w_rc_t	e;
			if(e=_mutex.acquire()) {
#ifdef DEBUG
				cerr << "could not acquire mutex" << endl; assert(0);
#else
				assert(0);
#endif /*DEBUG*/
			}
			_destroy(s); 
			_mutex.release();
		}
    client_t* find(int s)	{ 
			w_rc_t	e;
			if(e=_mutex.acquire()) {
#ifdef DEBUG
				cerr << "could not acquire mutex" << endl; assert(0);
#else
				assert(0);
#endif /*DEBUG*/
			}
			client_t *p = _find(s);
			_mutex.release();
			return p;
		}
	int		count();
	void	dump(bool verbose) ;
	void	shutdown_all() ;
};

#endif
