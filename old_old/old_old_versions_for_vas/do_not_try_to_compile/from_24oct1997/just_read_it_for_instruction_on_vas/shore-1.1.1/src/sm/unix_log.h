/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: unix_log.h,v 1.5 1996/02/27 21:59:46 nhall Exp $
 */
#ifndef UNIX_LOG_H
#define UNIX_LOG_H

#ifdef __GNUG__
#pragma interface
#endif


class unix_partition : public partition_t {
public:
	// these are abstract in the parent class
	void			_clear();
	void			_init(srv_log *o);
	FHDL			fhdl_rd() const;
	FHDL			fhdl_app() const;
	FHDL			seekend_app();
	void			open_for_append(partition_number_t n);
	void			open_for_read(partition_number_t n, bool err=true);
	int			seeklsn_rd(uint4 offset);
#ifdef OLD
	w_rc_t                  write(const logrec_t &r, const lsn_t &ll);
	void			flush(bool force = false);
#endif
	// w_rc_t                  read(logrec_t *&r, lsn_t &ll);
	void			close(bool both=true);
#ifdef OLD
	bool			exists() const;
	bool			is_open_for_read() const;
	bool			is_open_for_append() const;
	bool			flushed() const;
	bool			is_current() const; 
#endif
	void			peek(partition_number_t n, bool, int *fd = 0);
	void			destroy();
	void			sanity_check() const;
	void			set_fhdl_app(int fd);
	void			_flush(int fd);

private:
	FHDL			_fhdl_rd;
	FHDL			_fhdl_app;
};

class unix_log : public srv_log {
    friend class unix_partition;

public:
    NORET			unix_log(const char* logdir,
				    int rdbufsize, 
				    int wrbufsize, 
				    char *shmbase,
				    bool reformat);
    NORET			~unix_log();

    void			_write_master(const lsn_t& l, const lsn_t& min);
    partition_t *		i_partition(partition_index_t i) const;

    static void			_make_log_name(
	partition_number_t	    idx,
	char*			    buf,
	int			    bufsz);

				// bool e==true-->msg if can't be unlinked
    static void			destroy_file(partition_number_t n, bool e);

private:
    struct unix_partition 	_part[max_open_log];

    ////////////////////////////////////////////////
    // just for unix files:
    ///////////////////////////////////////////////

    void			_make_master_name(
	const lsn_t&		    master_lsn, 
	const lsn_t&		    min_chkpt_rec_lsn,
	char*			    buf,
	int 			    bufsz);

    static const char 		master_prefix[];
    static const char 		log_prefix[];

};

#endif /*UNIX_LOG_H*/
