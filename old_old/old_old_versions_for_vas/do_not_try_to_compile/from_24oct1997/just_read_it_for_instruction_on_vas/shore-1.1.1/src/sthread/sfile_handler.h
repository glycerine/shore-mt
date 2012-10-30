/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#ifndef _SFILE_HANDLER_
#define _SFILE_HANDLER_

#ifndef W_H
#include <w.h>
#endif

#ifdef __GNUC__
#pragma interface
#endif

class sfile_common_t {
public:
	enum { rd = 1, wr = 2, ex = 4 };
};	

class sfile_hdl_base_t;

class sfile_handler_t : public sfile_common_t {
public:
	sfile_handler_t();

	// Wait for sfile events
	w_rc_t		prepare(const stime_t &timeout,
				bool no_timeout);

	w_rc_t		wait();
	void		dispatch();

	bool		is_active(int fd);

	/* configure / deconfigure handles which can generate events */
	w_rc_t		start(sfile_hdl_base_t &);
	void		stop(sfile_hdl_base_t &);

	/* enable / disable events for a configured handle */
	void		enable(sfile_hdl_base_t &);
	void		disable(sfile_hdl_base_t &);

	ostream		&print(ostream &s);

private:
	w_list_t<sfile_hdl_base_t> _list;

	fd_set		masks[3];
	fd_set		ready[3];
	fd_set		*any[3];

	int		direction;	
	int		rc, wc, ec;
	int		dtbsz;

	struct timeval	tval;
	struct timeval	*tvp;
};


class sfile_hdl_base_t : public w_vbase_t, public sfile_common_t {
	friend class sfile_handler_t;

public:
	sfile_hdl_base_t(int fd, int mask);
    	~sfile_hdl_base_t();

	w_rc_t			change(int fd);
	
	int			fd;

	virtual void		read_ready()	{};
	virtual void 		write_ready()	{};
	virtual void 		exception_ready()	{};

	virtual int		priority() { return _mode & wr ? 1 : 0; }

	ostream			&print(ostream &) const;
	
	bool			running() const;
	bool			enabled() const;
	void			enable();
	void			disable();

	void			dispatch(bool r, bool w, bool e);

	/* XXX revectored for compatability */
	static	bool		is_active(int fd);

private:
	w_link_t		_link;
	const int		_mode;
	bool			_enabled;
	sfile_handler_t		*_owner;

	// disabled
	NORET			sfile_hdl_base_t(const sfile_hdl_base_t&);
	sfile_hdl_base_t&	operator=(const sfile_hdl_base_t&);
};


class sfile_safe_hdl_t : public sfile_hdl_base_t {
public:
	sfile_safe_hdl_t(int fd, int mode);
	~sfile_safe_hdl_t();

	w_rc_t		wait(long timeout);
	void		shutdown();
	bool		is_down()  { return _shutdown; }

protected:
	void		read_ready();
	void		write_ready();
	void		exception_ready();

private:
	bool		_shutdown;
	sevsem_t	sevsem;

	// disabled
	sfile_safe_hdl_t(const sfile_safe_hdl_t&);
	sfile_safe_hdl_t	&operator=(const sfile_safe_hdl_t&);
};


/* Provide backwards compatability for code that is too
   time consuming to change at the moment.  This will eventually
   go away.

   Compatability?  This is a an sfile which will automagically
   start itself at creation and stop itself when destructed.
   ... just like the old one.
 */

class sfile_compat_hdl_t : public sfile_safe_hdl_t {
public:
	sfile_compat_hdl_t(int fd, int mode);
	~sfile_compat_hdl_t();
};


class sfile_read_hdl_t : public sfile_compat_hdl_t {
public:
	sfile_read_hdl_t(int fd) : sfile_compat_hdl_t(fd, rd) { };
};


class sfile_write_hdl_t : public sfile_compat_hdl_t {
public:
	sfile_write_hdl_t(int fd) : sfile_compat_hdl_t(fd, wr) { };
};


extern ostream &operator<<(ostream &o, sfile_handler_t &h);
extern ostream &operator<<(ostream &o, sfile_hdl_base_t &h);

#endif /* _SFILE_HANDLER_ */
