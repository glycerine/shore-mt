/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: raw_log.h,v 1.6 1996/05/03 04:07:47 kupsch Exp $
 */
#ifndef RAW_LOG_H
#define RAW_LOG_H

#ifdef __GNUG__
#pragma interface
#endif

class raw_log; // forward

#define DEV_BLOCK_SIZE 	512

class raw_partition : public partition_t {
	friend class raw_log;

public:

	// these are abstract in the parent class
	void			_init(srv_log *o);
	void			_clear();
	int			fhdl_rd() const;
	int			fhdl_app() const;
	void			open_for_read(partition_number_t n, bool err=true);
	void			open_for_append(partition_number_t n);
	w_rc_t                  read(logrec_t *&r, lsn_t &ll, int fd = 0);
	void			close(bool both=true);
#ifdef OLD
	w_rc_t                  write(const logrec_t &r, const lsn_t &ll);
	void			flush(bool force = false);
	bool			flushed() const;
	bool			exists() const;
	bool			is_open_for_read() const;
	bool			is_open_for_append() const;
	bool			is_current() const; 
#endif
	void			destroy();
	void			sanity_check() const;

	void			peek(partition_number_t n, bool, int *fd=0);
	void 			open_for_read();
	void 			open_for_append();
        void			set_fhdl_app(int fd);

	void			_flush(int /*fd*/){}
};


class raw_log : public srv_log {
    friend class raw_partition;

public:
    NORET			raw_log(const char* logdir,
					int rdbufsize,
					int wrbufsize,
					char *shmbase,
					bool reformat);
    NORET			~raw_log();

    void			_write_master(const lsn_t& l, const lsn_t& min);
    partition_t *		i_partition(partition_index_t i) const;


    void			 _read_master(
				lsn_t& l,
				lsn_t& min
				); 
    static void 		_make_master_name(
				const lsn_t& 	master_lsn, 
				const lsn_t&	min_chkpt_rec_lsn,
				char* 		buf,
				int		bufsz);
private:
    struct raw_partition 	_part[max_open_log];
    static int			_fhdl_rd;
    static int			_fhdl_app;

protected:
    int				fhdl_rd() const { return _fhdl_rd; }
    int				fhdl_app() const { return _fhdl_app; }
};


#endif /*RAW_LOG_H*/
