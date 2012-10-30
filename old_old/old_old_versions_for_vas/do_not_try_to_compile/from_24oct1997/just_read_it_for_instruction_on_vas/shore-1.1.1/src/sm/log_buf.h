/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: log_buf.h,v 1.1 1997/04/13 16:14:55 nhall Exp $
 */
#ifndef LOG_BUF_H
#define LOG_BUF_H

#include <w_shmem.h>
#include <spin.h>
#undef ACQUIRE

#ifdef __GNUG__
#pragma interface
#endif

class log_buf {

private:
    char 	*buf;

    lsn_t 	_lsn_firstbyte; // of first byte in the buffer(not necessarily
			     // the beginning of a log record)
    lsn_t 	_lsn_flushed; // we've written to disk up to (but not including) 
			    // this lsn  -- lies between lsn_firstbyte and
			    // lsn_next 
    lsn_t 	_lsn_nextrec; // of next record to be buffered
    lsn_t 	_lsn_lastrec; // last record inserted 
    bool        _durable;     // true iff everything buffered has been
			     // flushed to disk
    bool        _written;     // true iff everything buffered has been
			     // written to disk (for debugging)

    uint4	_len; // logical end of buffer
    uint4	_bufsize; // physical end of buffer

    /* NB: the skip_log is not defined in the class because it would
     necessitate #include-ing all the logrec_t definitions and we
     don't want to do that, sigh.  */

    char	*___skip_log;	// memory for the skip_log
    char	*__skip_log;	// properly aligned skip_log

    void		init(const lsn_t &f, const lsn_t &n, bool, bool); 

public:

    const int XFERSIZE =	log_base::XFERSIZE;

    			log_buf(char *, int sz);
    			~log_buf();

    const lsn_t	&	firstbyte() const { return _lsn_firstbyte; }
    const lsn_t	&	flushed() const { return _lsn_flushed; }
    const lsn_t	&	nextrec() const { return _lsn_nextrec; }
    const lsn_t	&	lastrec() const { return _lsn_lastrec; }
    bool        	durable() const { return _durable; }
    bool        	written() const { return _written; }
    uint4         	len() const { return _len; }
    uint4 		bufsize() const { return _bufsize; }

    bool		fits(const logrec_t &l) const;
    void 		prime(int, off_t, const lsn_t &);
    void 		insert(const logrec_t &r);
    void 		insertskip();
    void 		mark_durable() { _durable = true; }

    void 		write_to(int fd);
    bool 		compensate(const lsn_t &rec, const lsn_t &undo_lsn);

    friend ostream&     operator<<(ostream &, const log_buf &);
};

#endif /* LOG_BUF_H */
