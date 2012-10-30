/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __CLIENT_H__
#define __CLIENT_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/server/client.h,v 1.21 1997/01/24 16:47:45 nhall Exp $
 */
#include <copyright.h>
#include <vas_internal.h>
#ifdef USE_KRB
#include <des.h>
#endif
#include <w_hash.h>

#ifdef __GNUG__
# pragma interface
#endif

// Because the FD_ZERO(&r); uses bzero():
#ifndef bzero
#define bzero(a,b) memset(a, '\0', b)
#endif


class cltab;
class svas_server;

class cleanup_client_t: public smthread_t {
private:
	bool _quit;
public:
    cleanup_client_t();
    ~cleanup_client_t() {}
	void 			kick();
	void 			run();
	void 			destroy();
};

class client_t: public smthread_t {
	friend class cltab;
	friend class svas_server;

#ifdef USE_KRB
public:
	static unsigned char 	nokey[8];
private:
	des_cblock			key;
#endif

private:
	u_long  			start_sec;

protected:
	sfile_read_hdl_t	*_ready; // this is created in run(), which calls
								// _run() iff _server is non-null, so 
								// errors are signalled to skip _run() if
								// _server is null.
	svas_server			*_server;// this is created in the constructor;
								// if anything goes wrong, e.g., authentication
								// fails, this must be left 0; that way
								// _run() won't get called, and the
								// thread can continue right on out and 
								// be destroyed.

	VASResult			_init(mode_t mask, int sm, int lg) {
				return _server->_init( mask, sm, lg );
			}
public:

	int					_fd;
    w_link_t			hash_link; // key is socket

	// void				is_active() const { _ready?true:false; }
	void				shutdown();
	void				abort();
	bool				is_down() const { return 
							_ready?_ready->is_down():true; }
	svas_server			*server() const { return _server; }

	// constructor
	client_t::client_t(const int fd, 
			const char *const uname, 
			int remoteness,
#ifdef USE_KRB
			const u_long time_sec, 
			const des_cblock skey,
#endif
			uid_t idu,
			gid_t idg,
			ErrLog		*el,
			bool		*ok
		);
	// special constructor for nfs servers
	client_t::client_t(const int fd, 
#ifdef USE_KRB
			const u_long time_sec, 
			const des_cblock skey,
#endif
			ErrLog		*el,
			bool		*ok,
			bool 		formount = false
		);
	// destructor
	~client_t() ;
	friend ostream &operator <<(ostream &o, const client_t&);
	virtual void	_dump(ostream &o);
	static  void	disconnect_server(void *s) { 
		client_t	*c = (client_t *)s;
		c->_server = NULL; 
	}
	void			attach_server(svas_server *s) { _server = s; }
	void 			run();
	// NB: client_t is an abstract class
	virtual void 	_run()=0;
};

#ifdef DEBUG
extern "C" void space(int); // in main.C
#define SPACE space(__LINE__);
#else
#define SPACE
#endif

#endif /*__CLIENT_H__*/
