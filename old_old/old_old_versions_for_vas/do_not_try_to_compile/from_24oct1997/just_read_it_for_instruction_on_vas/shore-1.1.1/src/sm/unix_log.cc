/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: unix_log.cc,v 1.28 1997/09/19 11:52:32 solomon Exp $
 */
#define SM_SOURCE
#define LOG_C
#ifdef __GNUG__
#   pragma implementation
#endif

/* XXX posix-dependency */
#include <dirent.h>

#include <sm_int_1.h>
#include <logdef.i>
#include <logtype.i>
#include "srv_log.h"
#include "unix_log.h"

#include <sys/stat.h>
#ifndef Linux
/* XXX complete sthread I/O facilities not in place yet */
extern "C" {
	int	fstat(int, struct stat *);
	int	truncate(const char *, off_t);
	int	fsync(int);
}
#endif

const char unix_log::master_prefix[] = "chk."; // same size as log_prefix
const char unix_log::log_prefix[] = "log.";

/*********************************************************************
 *
 *  unix_log::_make_log_name(idx, buf, bufsz)
 *
 *  Make up the name of a log file in buf.
 *
 *********************************************************************/
void
unix_log::_make_log_name(uint4 idx, char* buf, int bufsz)
{
    ostrstream s(buf, (int) bufsz);
    s << _logdir << '/' 
      << log_prefix << idx << ends;
    w_assert1(s);
}



/*********************************************************************
 * 
 *  unix_log::_make_master_name(master_lsn, min_chkpt_rec_lsn, buf, bufsz)
 *
 *  Make up the name of a master record in buf.
 *
 *********************************************************************/
void
unix_log::_make_master_name(
    const lsn_t& 	master_lsn, 
    const lsn_t&	min_chkpt_rec_lsn,
    char* 		buf,
    int			bufsz)
{
    ostrstream s(buf, (int) bufsz);
    s << _logdir << '/' << master_prefix << master_lsn << '.' 
      << min_chkpt_rec_lsn << ends;
    w_assert1(s);
}




/*********************************************************************
 *
 *  unix_log::unix_log(logdir, segmentid, reformat)
 *
 *  Hidden constructor. Open and scan logdir for master lsn and last log
 *  file. Truncate last incomplete log record (if there is any)
 *  from the last log file.
 *
 *********************************************************************/

NORET
unix_log::unix_log(const char* logdir, 
    int rdbufsize,
    int wrbufsize,
    char *shmbase,
    bool reformat) 
    : srv_log(rdbufsize, wrbufsize, shmbase)
{
    FUNC(unix_log::unix_log);

    partition_number_t 	last_partition = partition_num();
    bool		last_partition_exists = false;
    /* 
     * make sure there's room for the log names
     */
    w_assert1(strlen(logdir) < sizeof(_logdir));
    strcpy(_logdir, logdir);

    DIR* ldir = opendir(_logdir);
    if (! ldir) {
	w_rc_t e = RC(eOS);
	smlevel_0::errlog->clog << error_prio 
	    << "Error: could not open the log directory " << _logdir <<flushl;
	smlevel_0::errlog->clog << error_prio 
	    << "\tNote: the log directory is specified using\n" 
	    "\t      the sm_logdir option." << flushl;
	W_COERCE(e);
    }

    /*
     *  scan directory for master lsn and last log file 
     */
    dirent* dd;
    _shared->_master_lsn = null_lsn;

    uint4 min_index = max_uint4;
    char fname[smlevel_0::max_devname];

    {
	/*
	 *  initialize partition table
	 */
	partition_index_t i;
	for (i = 0; i < max_open_log; i++)  {
	    _part[i].init_index(i);
	    _part[i]._init(this);
	}
    }

    if (reformat) {
	smlevel_0::errlog->clog << error_prio 
	    << "Reformatting logs..." << endl;

	while ((dd = readdir(ldir)))  {
	    int parse_ok = (strncmp(dd->d_name,master_prefix,strlen(master_prefix))==0);
	    if(!parse_ok) {
		parse_ok = (strncmp(dd->d_name,log_prefix,strlen(log_prefix))==0);
	    }
	    if(parse_ok) {
		smlevel_0::errlog->clog << error_prio 
		    << "\t" << dd->d_name << "..." << endl;

		{
		    ostrstream s(fname, (int) smlevel_0::max_devname);
		    s << _logdir << '/' << dd->d_name << '\0';
		    w_assert1(s);
		    if( unlink(fname) < 0) {
			smlevel_0::errlog->clog << error_prio 
			    << "unlink failed !" << endl;
		    }
		}
	    }
	}
	// closedir(ldir);

	w_assert3(last_partition_exists == false);
    }

    while ((dd = readdir(ldir)))  {
	char buf[smlevel_0::max_devname+1];
	const prefix_len = sizeof(master_prefix) - 1;
	w_assert3(prefix_len < sizeof(buf));
	strncpy(buf, dd->d_name, prefix_len);
	buf[prefix_len] = '\0';
	int parse_ok = (strlen(buf) == prefix_len);

	DBG(<<"found log file " << dd->d_name);
	if (parse_ok) {
	    lsn_t tmp;
	    istrstream s(dd->d_name + prefix_len);
	    if (strcmp(buf, master_prefix) == 0)  {
		/*
		 *  File name matches master prefix.
		 *  Extract master lsn.
		 */
		char separator;
		lsn_t tmp1;
		if (! (s >> tmp >> separator >> tmp1) )  {
		    smlevel_0::errlog->clog << error_prio 
			<< "bad master log file \"" << buf << "\"" << flushl;
		    W_FATAL(eINTERNAL);
		}
		if (tmp < _shared->_master_lsn)  {
		    /* 
		     *  Swap tmp <-> _master_lsn, tmp1 <-> _min_chkpt_rec_lsn
		     */
		    lsn_t swap;
		    swap = _shared->_master_lsn;
		    _shared->_master_lsn = tmp;
		    tmp = _shared->_master_lsn;
		    swap = _shared->_min_chkpt_rec_lsn;
		    _shared->_min_chkpt_rec_lsn = tmp1;
		    tmp1 = _shared->_min_chkpt_rec_lsn;
		}
		/*
		 *  Remove the older master record.
		 */
		if (_shared->_master_lsn != lsn_t::null) {
		    _make_master_name(_shared->_master_lsn,
				      _shared->_min_chkpt_rec_lsn,
				      fname,
				      sizeof(fname));
		    (void) unlink(fname);
		}
		/*
		 *  Save the new master record
		 */
		_shared->_master_lsn = tmp;
		_shared->_min_chkpt_rec_lsn = tmp1;

	    } else if (strcmp(buf, log_prefix) == 0)  {
		/*
		 *  File name matches log prefix
		 */

		uint4 curr;
		if (! (s >> curr))  {
		    smlevel_0::errlog->clog << error_prio 
		    << "bad log file \"" << buf << "\"" << flushl;
		    W_FATAL(eINTERNAL);
		}
		DBG(<<"curr " << curr
			<< " partition_num()==" << partition_num() );

		if (curr >= last_partition) {
		    last_partition = curr;
		    last_partition_exists = true;
		}
		if (curr < min_index) {
		    min_index = curr;
		}
	    }
	    parse_ok = (int) (const void*) s;
	} 

	/*
	 *  if we couldn't parse the file name and it was not "." or ..
	 *  then print and error message
	 */
	if (!parse_ok && ! (strcmp(dd->d_name, ".") == 0 || 
			    strcmp(dd->d_name, "..") == 0)) {
	    smlevel_0::errlog->clog << error_prio 
	    << "unix_log: cannot parse " << dd->d_name << flushl;
	}
    }
    closedir(ldir);

#ifdef DEBUG
    if(reformat) {
	w_assert3(partition_num() == 1);
	w_assert3(_shared->_min_chkpt_rec_lsn.hi() == 1);
	w_assert3(_shared->_min_chkpt_rec_lsn.lo() == 0);
    } else {
       // ??
    }
    w_assert3(partition_index() == -1);
#endif

    DBG(<<"Last partition is " << last_partition
	<< " existing = " << last_partition_exists
     );

    /*
     *  Destroy all partitions less than _min_chkpt_rec_lsn
     *  Open the rest and close them.
     *  There might not be an existing last_partition,
     *  regardless of the value of "reformat"
     */
    {
	partition_number_t n;
	partition_t	*p;

	w_assert3(min_chkpt_rec_lsn().hi() <= last_partition);

	for (n = min_index; n < min_chkpt_rec_lsn().hi(); n++)  {
	    // not an error if we can't unlink (probably doesn't exist)
	    unix_log::destroy_file(n, false);
	}
	for (n = _shared->_min_chkpt_rec_lsn.hi(); n < last_partition; n++)  {
	    // open and check each file (get its size)
	    p = open(n, true, false, true);
	    w_assert3(p == n_partition(n));
	    p->close(false);
	    unset_current();
	}
    }


    /*
     *
	The goal of this code is to determine where is the last complete
	log record in the log file and truncate the file at the
	end of that record.  It detects this by scanning the file and
	either reaching eof or else detecting an incomplete record.
	If it finds an incomplete record then the end of the preceding
	record is where it will truncate the file.

	The file is scanned by attempting to fread the length of a log
	record header.	If this fread does not read enough bytes, then
	we've reached an incomplete log record.  If it does read enough,
	then the buffer should contain a valid log record header and
	it is checked to determine the complete length of the record.
	Fseek is then called to advance to the end of the record.
	If the fseek fails then it indicates an incomplete record.

     *  NB:
	This is done here rather than in peek() since in the unix-file
	case, we only check the *last* partition opened, not each
	one read.
     *
     */
    _make_log_name(last_partition, fname, sizeof(fname));
    DBG(<<" checking " << fname);

    FILE *f =  fopen(fname, "rb");
    off_t pos = 0;
    if (f)  {
	w_assert3(last_partition_exists == true);
	char buf[logrec_t::hdr_sz];
	int n;
	while ((n = fread(buf, 1, sizeof(buf), f)) == sizeof(buf))  {
	    logrec_t  *l = (logrec_t*) buf;

	    if( l->type() == logrec_t::t_skip) {
		break;
	    }

	    uint2 len = l->length();

	    // Truncate now writes a 0 length
	    w_assert1((len >= logrec_t::hdr_sz) ||
		(len == 0 && pos == 0));

	    // seek to lsn_ck at end of record
	    if (fseek(f, len - (sizeof(buf)+sizeof(lsn_t)), 1))  {
		if (feof(f))  break;
	    }

	    lsn_t lsn_ck;
	    n = fread(&lsn_ck, 1, sizeof(lsn_ck), f);
	    if (n != sizeof(lsn_ck))  {
		// reached eof
		if (! feof(f))  {
		    smlevel_0::errlog->clog << error_prio 
		    << "ERROR: unexpected log file inconsistency." << flushl;
		    W_FATAL(eOS);
		}
		break;
	    }
	    // make sure log record's lsn matched its position in file
	    if ( (lsn_ck.lo() != (uint4) pos) ||
		    (lsn_ck.hi() != (uint4) last_partition ) ) {
		// found partial log record, end of log is previous record
		smlevel_0::errlog->clog << error_prio <<
		    "Found unexpected end of log -- probably due to a previous crash." << flushl;
		smlevel_0::errlog->clog << error_prio <<
		    "   Recovery will continue ..." << flushl;
		break;
	    }
	    // remember current position
	    pos = ftell(f);
	}
	fclose(f);
	/*
	smlevel_0::errlog->clog << error_prio <<
	    "Truncating " << fname << " to " << pos <<flushl;
	*/

	{
	    DBG(<<"truncating " << fname << " to " << pos);
	    truncate(fname, pos);

	    //
	    // but we can't just use truncate() --
	    // we have to truncate to as size that's a mpl
	    // of the page size. First append a skip record
	    f =  fopen(fname, "ab");
	    skip_log *s = new skip_log;
	    s->set_lsn_ck( lsn_t(uint4(last_partition), uint4(pos)) );

	    DBG(<<"writing skip_log at pos " << pos << " with lsn "
		<< s->lsn_ck() );

	    if ( fwrite(s, s->length(), 1, f) != 1)  {
		smlevel_0::errlog->clog << error_prio <<
		    "   fwrite: can't write skip rec to log ..." << flushl;
		W_FATAL(eOS);
	    }
	    off_t o = pos;
	    o += s->length();
	    o = o % XFERSIZE;
	    if(o > 0) {
		o = XFERSIZE - o;
		char *junk = new char[o];
		if (!junk)
			W_FATAL(fcOUTOFMEMORY);
#ifdef PURIFY
		if(purify_is_running()) {
		    memset(junk,'\0', o);
		}
#endif
		
		DBG(<<"writing junk of length " << o);
		if ( fwrite(junk, o, 1, f) != 1)  {
		    smlevel_0::errlog->clog << error_prio <<
		    "   fwrite: can't round out log block size ..." << flushl;
		    W_FATAL(eOS);
		}
		delete[] junk;
		o = 0;
	    }
	    delete s;
	    off_t eof = ftell(f);
	    DBG(<<"eof is now " << eof);

	    if(eof % XFERSIZE != 0) {
		smlevel_0::errlog->clog << error_prio <<
		    "   ftell: can't write skip rec to log ..." << flushl;
		W_FATAL(eOS);
	    }
#ifndef WINNT
	    if( fsync(fileno(f)) < 0) {
		smlevel_0::errlog->clog << error_prio <<
		    "   fsync: can't sync fsync truncated log ..." << flushl;
		W_FATAL(eOS);
	    }
#endif
#ifdef DEBUG
	    {
		w_rc_t e;
		struct stat statbuf;
		e = MAKERC(fstat(fileno(f), &statbuf) == -1, eOS);
		if (e) {
		    smlevel_0::errlog->clog << error_prio 
			    << " Cannot stat fd " << fileno(f)
			    << ":" << endl << e << endl << flushl;
		    W_COERCE(e)
		}
		DBG(<< "size of " << fname << " is " << statbuf.st_size);
	    }
#endif
	    fclose(f);
	}

    } else {
	w_assert3(last_partition_exists == false);
    }

    /*
     *  initialize current and durable lsn for
     *  the purpose of sanity checks in open*()
     *  and elsewhere
     */
    DBG( << "partition num = " << partition_num()
	<<" current_lsn " << curr_lsn()
	<<" durable_lsn " << durable_lsn());

    _shared->_curr_lsn = 
    _shared->_durable_lsn =	 // set_durable(...)
    	lsn_t(uint4(last_partition), uint4(pos));

    DBG( << "partition num = " << partition_num()
	    <<" current_lsn " << curr_lsn()
	    <<" durable_lsn " << durable_lsn());

    {
	/*
	 *  create/open the "current" partition
	 *  "current" could be new or existing
	 *  Check its size and all the records in it
	 *  by passing "true" for the last argument to open()
	 */

	partition_t *p = open(last_partition,
		last_partition_exists, true, true);

	/* XXX error info lost */
	if(!p) {
	    smlevel_0::errlog->clog << error_prio 
	    << "ERROR: could not open log file for partition "
	    << last_partition << flushl;
	    W_FATAL(eOS);
	}

	w_assert3(p->num() == last_partition);
	w_assert3(partition_num() == last_partition);
	w_assert3(partition_index() == p->index());

    }
    DBG( << "partition num = " << partition_num()
	    <<" current_lsn " << curr_lsn()
	    <<" durable_lsn " << durable_lsn());

    compute_space(); // does a sanity check
}



/*********************************************************************
 * 
 *  unix_log::~unix_log()
 *
 *  Destructor. Close all open partitions.
 *
 *********************************************************************/
unix_log::~unix_log()
{
    partition_t	*p;
    FHDL f;
    for (uint i = 0; i < max_openlog; i++) {
	p = i_partition(i);
	f = ((unix_partition *)p)->fhdl_rd();
	if (f)  {
	    w_rc_t e;
	    DBG(<< " CLOSE " << f);
	    e = me()->close(f);
	    if (e) {
		    cerr << "warning: unix log on close(rd):" << endl
			    <<  e << endl;
	    }
	}
	f = ((unix_partition *)p)->fhdl_app();
	if (f)  {
	    w_rc_t e;
	    DBG(<< " CLOSE " << f);
	    e = me()->close(f);
	    if (e) {
		    cerr << "warning: unix log on close(app):" << endl
			    <<  e << endl;
	    }
	}
	p->_clear();
    }
}


partition_t *
unix_log::i_partition(partition_index_t i) const
{
    return i<0 ? (partition_t *)0: (partition_t *) &_part[i];
}

void
unix_log::_write_master(
    const lsn_t& l,
    const lsn_t& min
) 
{
    /*
     *  create new master record
     */
    _make_master_name(l, min, _chkpt_meta_buf, CHKPT_META_BUF);
    DBG(<< "writing checkpoint master: " << _chkpt_meta_buf);

    FILE* f = fopen(_chkpt_meta_buf, "ab");
    if (! f) {
	smlevel_0::errlog->clog << error_prio 
	    << "ERROR: could not open a new log checkpoint file: "
	    << _chkpt_meta_buf << flushl;
	W_FATAL(eOS);
    }
    fclose(f);

    /*
     *  destroy old master record
     */
    _make_master_name(_shared->_master_lsn, 
	_shared->_min_chkpt_rec_lsn, _chkpt_meta_buf, CHKPT_META_BUF);
    (void) unlink(_chkpt_meta_buf);
}

/*********************************************************************
 * unix_partition class
 *********************************************************************/

void			
unix_partition::set_fhdl_app(int fd)
{
   w_assert3(fhdl_app() == 0);
   DBG(<<"SET APP " << fd);
   _fhdl_app = fd;
}

void
unix_partition::peek(
    partition_number_t  __num, 
    bool 		recovery,
    int *		fdp
)
{
    FUNC(unix_partition::peek);
    int fd;

    w_assert3(num() == 0);
    w_assert3(__num != 0);
    clear();

    clr_state(m_exists);
    clr_state(m_flushed);
    clr_state(m_open_for_read);

    char fname[smlevel_0::max_devname];
    unix_log::_make_log_name(__num, fname, sizeof(fname));

    off_t part_size;

    DBG(<<"opening " << fname);

    // first create it if necessary.
    int flags = smthread_t::OPEN_RDWR | smthread_t::OPEN_SYNC
	    | smthread_t::OPEN_CREATE;
    w_rc_t e;
    char *s = getenv("SM_LOG_LOCAL");
    if (s && atoi(s))
	    flags |= smthread_t::OPEN_LOCAL;
    else
	    flags |= smthread_t::OPEN_KEEP;
    e = me()->open(fname, flags, 0744, fd);
    if (e) {
	smlevel_0::errlog->clog << error_prio
	    << "ERROR: cannot open log file: " << fd  << flushl;
	W_COERCE(e);
    }
    {
     w_rc_t e;
     struct stat statbuf;
     e = me()->fstat(fd, statbuf);
     if (e) {
	smlevel_0::errlog->clog << error_prio 
	    << " Cannot stat fd " << fd 
	    << " rc = " << e << flushl;
	W_COERCE(e);
     }
     part_size = statbuf.st_size;
     DBG(<< "size of " << fname << " is " << statbuf.st_size);
    }

    // We will eventually want to write a record with the durable
    // lsn.  But if this is start-up and we've initialized
    // with a partial partition, we have to prime the
    // buf with the last block in the partition.
    //
    // If this was a pre-existing partition, we have to scan it
    // to find the *real* end of the file.

    if( part_size > 0) {
	_peek(__num, recovery, fd);
    } else {
	// write a skip record so that prime() can
	// cope with it.
	// Have to do this carefully -- since using
	// the standard insert()/write code causes a
	// prime() to occur and that doesn't solve anything.

	DBG(<<" peek DESTROYING PARTITION  on fd " << fd);

	// First: write any-old junk
	w_rc_t e = me()->ftruncate(fd,  XFERSIZE);
	if (e)	{
	     smlevel_0::errlog->clog << error_prio
                << "cannot write garbage block " << flushl;
            W_COERCE(e);
	}
	// set_size(XFERSIZE);

	// Now write the skip record and flush it to the disk:
	// skip(lsn_t::null, fd);
	skip(lsn_t(uint4(__num),uint4(0)), fd);

	// First: write any-old junk
	e = me()->fsync(fd);
        if (e) {
	     smlevel_0::errlog->clog << error_prio
                << "cannot sync after skip block " << flushl;
            W_COERCE(e);
	}

	set_size(partition_t::nosize);
    }

    if (fdp) {
	DBG(<< " SAVED, NOT CLOSED fd " << fd);
	*fdp = fd;
    } else {

	DBG(<< " CLOSE " << fd);
	w_rc_t e = me()->close(fd);
	if (e) {
	    smlevel_0::errlog->clog << error_prio 
	    << "ERROR: could not close the log file." << flushl;
	    W_COERCE(e);
	}
	
    }
    return; 
}

void			
unix_partition::_flush(int fd)
{
    // We only cound the fsyncs called as
    // a result of _flush(), not from peek
    // or start-up
    smlevel_0::stats.log_fsync_cnt ++;

    w_rc_t e = me()->fsync(fd);
    if (e) {
	 smlevel_0::errlog->clog << error_prio
	    << "cannot sync after skip block " << flushl;
	W_COERCE(e);
    }
}

void
unix_partition::open_for_read(
    partition_number_t  __num,
    bool err // = true.  if true, it's an error for the partition not to exist
)
{
    FUNC(unix_partition::open_for_read);

    DBG(<<" open " << __num << " err=" << err);

    w_assert1(__num != 0);

    // do the equiv of opening existing file
    // if not already in the list and opened
    //
    if(! fhdl_rd()) {
	char fname[smlevel_0::max_devname];
	unix_log::_make_log_name(__num, fname, sizeof(fname));

	int fd;
	w_rc_t e;
	DBG(<< " OPEN " << fname);
	int flags = smthread_t::OPEN_RDONLY;

	char *s = getenv("SM_LOG_LOCAL");
	if (s && atoi(s))
		flags |= smthread_t::OPEN_LOCAL;

	e = me()->open(fname, flags, 0, fd);

	DBG(<< " OPEN " << fname << " returned " << _fhdl_rd);

	if (e) {
	    if(err) {
		smlevel_0::errlog->clog << error_prio << flushl;
		smlevel_0::errlog->clog << error_prio
		<< "ERROR: cannot open log file: " << fd << flushl;
		W_COERCE(e);
	    } else {
		w_assert3(! exists());
		w_assert3(_fhdl_rd == 0);
		// _fhdl_rd = 0;
		clr_state(m_open_for_read);
		DBG(<<"fhdl_app() is " << _fhdl_app);
		return;
	    }
	}

	w_assert3(_fhdl_rd == 0);
        _fhdl_rd = fd;

	DBG(<<"size is " << size());
	// size might not be known, might be anything
	// if this is an old partition

	set_state(m_exists);
	set_state(m_open_for_read);
    }
    _num = __num;
    w_assert3(exists());
    w_assert3(is_open_for_read());
    // might not be flushed, but if
    // it isn't, surely it's flushed up to
    // the offset we're reading
    //w_assert3(flushed());

    w_assert3(_fhdl_rd != 0);
    DBG(<<"_fhdl_rd = " <<_fhdl_rd );
}

/*
 * close for append, or if both==true, close
 * the read-file also
 */
void
unix_partition::close(bool both) 
{
    bool err_encountered=false;
    w_rc_t e;

    if(is_current()) {
	W_COERCE(_owner->flush(_owner->curr_lsn()));
	w_assert3(flushed());
	_owner->unset_current();
    }
    if (both) {
	if (fhdl_rd()) {
	    DBG(<< " CLOSE " << fhdl_rd());
	    e = me()->close(fhdl_rd());
	    if (e) {
		smlevel_0::errlog->clog << error_prio 
			<< "ERROR: could not close the log file."
			<< e << endl << flushl;
		err_encountered = true;
	    }
	}
	_fhdl_rd = 0;
	clr_state(m_open_for_read);
    }

    if (is_open_for_append()) {
	DBG(<< " CLOSE " << fhdl_rd());
	e = me()->close(fhdl_app());
	if (e) {
	    smlevel_0::errlog->clog << error_prio 
	    << "ERROR: could not close the log file."
	    << endl << e << endl << flushl;
	    err_encountered = true;
	}
	_fhdl_app = 0;
	clr_state(m_open_for_append);
	DBG(<<"fhdl_app() is " << _fhdl_app);
    }
    clr_state(m_flushed);
    if (err_encountered) {
	W_COERCE(e);
    }
}


void 
unix_partition::sanity_check() const
{
    if(num() == 0) {
       // initial state
       w_assert3(size() == nosize);
       w_assert3(!is_open_for_read());
       w_assert3(!is_open_for_append());
       w_assert3(!exists());
       // don't even ask about flushed
    } else {
       w_assert3(exists());
       (void) is_open_for_read();
       (void) is_open_for_append();
    }
    if(is_current()) {
       w_assert3(is_open_for_append());
    }
}



/**********************************************************************
 *
 *  unix_partition::destroy()
 *
 *  Destroy a log file.
 *
 *********************************************************************/
void
unix_partition::destroy()
{
    w_assert3(num() < _owner->global_min_lsn().hi());

    if(num()>0) {
	w_assert3(exists());
	w_assert3(! is_current() );
	w_assert3(! is_open_for_read() );
	w_assert3(! is_open_for_append() );

	unix_log::destroy_file(num(), true);
	clr_state(m_exists);
	// _num = 0;
	clear();
    }
    w_assert3( !exists());
    sanity_check();
}

void
unix_log::destroy_file(partition_number_t n, bool pmsg)
{
    char fname[smlevel_0::max_devname];
    unix_log::_make_log_name(n, fname, sizeof(fname));
    if (unlink(fname) == -1)  {
	if(pmsg) {
	    smlevel_0::errlog->clog << error_prio 
	    << "warning : cannot free log file \"" 
	    << fname << '\"' << flushl;
	    smlevel_0::errlog->clog << error_prio 
	    << "          " << RC(eOS) << flushl;
	}
    }
}

void
unix_partition::_clear()
{
    clr_state(m_open_for_read);
    clr_state(m_open_for_append);
    _fhdl_rd = 0;
    _fhdl_app = 0;
}

void
unix_partition::_init(srv_log *owner)
{
    _start = 0; // always
    _owner = owner;
    _eop = owner->limit(); // always
    clear();
}

FHDL
unix_partition::fhdl_rd() const
{
#ifdef DEBUG
    bool isopen = is_open_for_read();
    if(_fhdl_rd == (FHDL)0) {
	w_assert3( !isopen );
    } else {
	w_assert3(isopen);
    }
#endif
    return _fhdl_rd;
}

FHDL
unix_partition::fhdl_app() const
{
    if(_fhdl_app) {
	w_assert3(is_open_for_append());
    } else {
	w_assert3(! is_open_for_append());
    }
    return _fhdl_app;
}

