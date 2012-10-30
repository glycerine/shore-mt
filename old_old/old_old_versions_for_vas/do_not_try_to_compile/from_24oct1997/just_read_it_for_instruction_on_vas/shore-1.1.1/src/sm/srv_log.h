/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: srv_log.h,v 1.15 1997/04/13 16:29:52 nhall Exp $
 */
#ifndef SRV_LOG_H
#define SRV_LOG_H

#include <log.h>

#ifdef __GNUG__
#pragma interface
#endif


typedef	uint4	partition_number_t;
typedef	int	partition_index_t;
typedef enum    { 
	m_exists=0x2,
	m_open_for_read=0x4,
	m_open_for_append=0x8,
	m_flushed=0x10,	// has no data cached
};

#define CHKPT_META_BUF 512
			

class log_buf; //forward
class srv_log; //forward
class partition_t; //forward


class srv_log : public log_base {
     friend class partition_t;


protected:

    char *			_chkpt_meta_buf;

public:
    void                check_wal(const lsn_t &) ;
    void 		compute_space(); 

    // CONSTRUCTOR -- figures out which srv type to construct
    static srv_log*	new_log_m(
		const char *logdir,
		int rdbufsize,
		int wrbufsize,
		char *shmbase,
		bool reformat
	  );
    // NORET 		srv_log(const char *segid); is protected, below

    virtual
    NORET ~srv_log();



#define VIRTUAL(x) x;
#define NULLARG = 0
    COMMON_INTERFACE
#undef VIRTUAL
#undef NULLARG

    ///////////////////////////////////////////////////////////////////////
    // done entirely by server side, in generic way:
    ///////////////////////////////////////////////////////////////////////
    void 		 	set_master(
					const lsn_t& 		    lsn,
					const lsn_t&		    min_rec_lsn,
					const lsn_t&		    min_xct_lsn);

    partition_t *		close_min(partition_number_t n);
				// the defaults are for the case
				// in which we're opening a file to 
				// be the new "current"
    partition_t *		open(partition_number_t n, 
				    bool existing = false,
				    bool forappend = true,
				    bool during_recovery = false
				); 
    partition_t *		n_partition(partition_number_t n) const;
    partition_t *		curr_partition() const;

    void			set_current(partition_index_t, partition_number_t); 
    void			unset_current(); 

    partition_index_t		partition_index() const { return _curr_index; }
    partition_number_t		partition_num() const { return _curr_num; }
    uint4			limit() const { return _shared->_max_logsz; }
    uint4			logDataLimit() const { return _shared->_maxLogDataSize; }

    virtual
    void			_write_master(const lsn_t& l, const lsn_t& min)=0;

    virtual
    partition_t *		i_partition(partition_index_t i) const = 0;

    void			set_durable(const lsn_t &ll);

protected:
    NORET 			srv_log( int rdbufsize,
				    int wrbufsize,
				    char *shmbase
				);
    void			sanity_check() const;
    partition_index_t		get_index(partition_number_t)const; 
    void 			_compute_space(); 

    // Data members:

    static bool			_initialized;
    static char 		_logdir[max_devname];
    partition_index_t		_curr_index; // index of partition
    partition_number_t		_curr_num;   // partition number

    // log_buf stuff:
public:
    log_buf *			writebuf() { return _writebuf; }
    int				writebufsize() const { return _wrbufsize; }
    char *			readbuf() { return _readbuf; }
    int 			readbufsize() const { return _rdbufsize; }
protected:
    int 			_rdbufsize;
    int 			_wrbufsize;
    char*   			_readbuf;  
    log_buf*   			_writebuf;  
};

typedef int  FHDL;


/* abstract class: */
class partition_t {
	friend class srv_log;

public:
	const XFERSIZE = log_base::XFERSIZE;
	partition_t() :_start(0),
		_index(0), _num(0), _size(0), _mask(0), 
		_owner(0) {}

	virtual
	~partition_t() {};

protected: // these are common to all partition types:
	const			max_open_log = smlevel_0::max_openlog;

	uint4			_start;
	partition_index_t	_index;
	partition_number_t 	_num;
	uint4			_size;
	uint4			_mask;
	srv_log			*_owner;

protected:
	uint4			_eop; // physical end of partition
	// logical end of partition is _size;

public:
	const uint4		nosize = max_uint4;

	const log_buf &		writebuf() const { return *_owner->writebuf(); }
	int			writebufsize() const { return _owner->
						writebufsize(); }
	char   *		readbuf() { return _owner->readbuf(); }
	int			readbufsize() const { return _owner->
						readbufsize(); }

	virtual 
	int 			fhdl_app() const = 0;
	virtual 
	int 			fhdl_rd() const = 0;

	void			set_state(uint4 m) { _mask |= m ; }
	void			clr_state(uint4 m) { _mask &= ~m ; }

	virtual
	void			_clear()=0;
	void			clear() {  	_num=0;_size=nosize;
						_mask=0; _clear(); }
	void			init_index(partition_index_t i) { _index=i; }

	partition_index_t	index() const {  return _index; }
	partition_number_t	num() const { return _num; }


	uint4			size()const { return _size; }
	void			set_size(uint4 v) { 
					_size =  v;
				}

	virtual
	void			open_for_read(partition_number_t n, bool err=true) = 0;

	void			open_for_append(partition_number_t n);

	void			skip(const lsn_t &ll, int fd);

	virtual
	w_rc_t			write(const logrec_t &r, const lsn_t &ll) ;

	w_rc_t			read(logrec_t *&r, lsn_t &ll, int fd);

	virtual
	void			close(bool both) = 0;

	virtual
	void			destroy() = 0;

	void			flush(int fd, bool force=false);

	virtual
	void			_flush(int fd)=0;

	virtual
	void			sanity_check() const =0;

	virtual
	bool			exists() const;

	virtual
	bool			is_open_for_read() const;

	virtual
	bool			is_open_for_append() const;

	virtual
	bool			flushed() const;

	virtual
	bool			is_current() const;

	virtual
	void			peek(partition_number_t n, bool, int* fd=0) = 0;

	virtual
	void			set_fhdl_app(int fd)=0;

	void			_peek(partition_number_t n, bool, int fd);

};

#endif /* SRV_LOG_H */

