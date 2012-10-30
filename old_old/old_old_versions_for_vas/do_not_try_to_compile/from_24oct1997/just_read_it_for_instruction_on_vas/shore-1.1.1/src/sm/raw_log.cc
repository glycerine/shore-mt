/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: raw_log.cc,v 1.22 1997/06/15 03:13:37 solomon Exp $
 */
#define SM_SOURCE
#define LOG_C
#ifdef __GNUG__
#   pragma implementation
#endif

#include <stream.h>
#include <iostream.h>
#include <fstream.h>
#include <stdlib.h>
#include <sm_int_1.h>
#include <logdef.i>
#include <logtype.i>
#include "srv_log.h"
#include "raw_log.h"
#include "log_buf.h"


int		raw_log::_fhdl_rd=0;
int		raw_log::_fhdl_app=0;


/*********************************************************************
 * 
 *  raw_log::_make_master_name(master_lsn, min_chkpt_rec_lsn, buf, bufsz)
 *
 *  Make up the name of a master record in buf.
 *
 *********************************************************************/
void
raw_log::_make_master_name(
    const lsn_t& 	master_lsn, 
    const lsn_t&	min_chkpt_rec_lsn,
    char* 		_buf,
    int			_bufsz)
{
    FUNC(raw_log::_make_master_name);
    ostrstream s(_buf, (int) _bufsz);
    s << master_lsn << '.' 
      << min_chkpt_rec_lsn << '\0';
    w_assert1(s);
}

/*********************************************************************
 *
 *  raw_log::raw_log(logdir, int, int, char*, reformat)
 *
 *  Hidden constructor. Open and scan logdir for master lsn and last log
 *  file. Truncate last incomplete log record (if there is any)
 *  from the last log file.
 *
 *********************************************************************/


NORET
raw_log::raw_log(
	const char* logdir, 
	int rdbufsize, 
	int wrbufsize,
	char *shmbase,
	bool reformat
) 
: srv_log(rdbufsize, wrbufsize, shmbase)
{
    FUNC(raw_log::raw_log);

    /* 
     * make sure there's room for the log name
     */
    w_assert1(strlen(logdir) < sizeof(_logdir));
    strcpy(_logdir, logdir);


    smsize_t raw_size = 0;
    {
	w_rc_t e1, e2;    
	/* 
	 * open the files
	 *
	 * Open for I/O; don't truncate them ... the
	 * file is a raw device and better exist!
	 */
	/* XXX keep it open to fetch the geometry info */
	int lflag = smthread_t::OPEN_KEEP;

	/* XXX local I/O ? */
	char *s = getenv("SM_LOG_LOCAL");
	if (s && atoi(s))
		lflag |= smthread_t::OPEN_LOCAL;

	e1 = me()->open(logdir, lflag | smthread_t::OPEN_RDONLY, 0, _fhdl_rd);
	e2 = me()->open(logdir, lflag | smthread_t::OPEN_RDWR | smthread_t::OPEN_SYNC, 0, _fhdl_app);

	if (e1) {
	    smlevel_0::errlog->clog << error_prio 
		<< "ERROR: cannot open log file for read." << flushl;
	    W_COERCE(e1);
	}
	if (e2) {
	    smlevel_0::errlog->clog << error_prio 
		<< "ERROR: cannot open log file for append." << flushl;
	    W_COERCE(e2);
	}

	DBG(<<"Files opened");
        w_rc_t e;

	if (_shared->_max_logsz != 0)  {
	    raw_size = _shared->_max_logsz * max_open_log;

	    if (reformat)  {
	        raw_size /= DEV_BSIZE;	// make an even multiple of DEV_BSIZE so write works on raw device
	        raw_size *= DEV_BSIZE;
    
	        // check to see if we can read and write the last DEV_BSIZE bytes of the log
		off_t where = raw_size - DEV_BSIZE;
	        e = me()->lseek(_fhdl_app, where, SEEK_SET);
		if (e) {
		    cerr << "Bad value for sm_logsize option.  "
			<< "Cannot seek to last block." << endl;
		    W_COERCE(e);
	        }

		/* Use the readbuffer for validating the log size */
		w_assert1(rdbufsize >= DEV_BSIZE);

		/* XXX non-shm I/O */
		e = me()->read(_fhdl_app, _readbuf, DEV_BSIZE);
		if (e) {
			/* its not really an OS error, rather a user error */
			cerr << "Bad value for sm_logsize option, cannot read last block.";
			cerr << endl << e << endl;
			W_COERCE(e);
	        }
    
	        e = me()->write(_fhdl_app, _readbuf, DEV_BSIZE);
		if (e) {
			/* its not really an OS error, rather a user error */
			cerr << "Bad value for sm_logsize option, cannot write last block.";
			cerr << endl << e << endl;
			W_COERCE(e);
	        }
	    }
	}  else  {
	    struct disk_geometry dg;
	    e = me()->fgetgeometry(_fhdl_rd, dg);
	    /* XXX no way to report errors from the constructor */
	    W_COERCE(e);

	    raw_size = dg.blocks * dg.block_size;

	    cout.form("raw device has %d bytes (%d blocks, %d b/block)\n",
		      raw_size, dg.blocks, dg.block_size);

	    DBG(<< "raw device has " << raw_size << " blocks.");
	}
	DBG(<< "using a raw partition of size " << raw_size << " bytes.");
    }

    /* 
     * use the raw size computed above either from the sm_logsize configuration
     * option or the the size of the raw partition.
     */
    smsize_t	size  = raw_size - CHKPT_META_BUF;
    smsize_t	used  = CHKPT_META_BUF;  
    size /= max_open_log;
    size /= XFERSIZE;
    size *= XFERSIZE;

    // set the _max_logsz to the computed size of the partition since other parts
    // of the code depend on this being set correctly.  also set _maxLogDataSize
    // and chkpt_displacement since these also need adjustments in the raw case.

    _shared->_max_logsz = size;
    skip_log *s = new skip_log;
    _shared->_maxLogDataSize = size - s->length();
    delete s;
    smlevel_0::chkpt_displacement = MIN(size/2, 1024*1024);

    // partition size needs to be at least 64KB
    w_assert1(size >= 64*1024);

    w_assert1(size > sizeof(logrec_t));
    {
	DBG(<<"initialize part tbl, part size =" << size);
	w_assert1((size / XFERSIZE)*XFERSIZE == size);
	/*
	 *  initialize partition table
	 */
	partition_index_t i;
	for (i = 0; i < max_open_log; i++)  {
	    _part[i]._init(this);
	    _part[i].init_index(i);

	    _part[i]._start = used;
	    _part[i].set_size(partition_t::nosize);

	    used += size;
	    _part[i]._eop = used;
	}
	/*
	 * Extend the last partition if possible.
	 * It could be extended as much as (7 * XFERSIZE)
	 */
#ifdef dont_do_this
    /**** DON'T DO THIS: parts of the code depend on uniform partition sizes */
	{
	    smsize_t left = raw_size - used;
	    if( left >= XFERSIZE ) {
		left /= XFERSIZE;
		DBG( " extending last partition by " << left <<" chunks" );
		left *= XFERSIZE;

		size += left;
		used += left;

		w_assert3( raw_size - used < XFERSIZE ); 
		w_assert3( used == left + _part[max_open_log -1]._eop);

		_part[max_open_log -1]._eop = used;
	    }
	}
#endif /*dont_do_this*/
    }


    /*
     *  get ready to scan file for master lsn 
     */
    _shared->_master_lsn = 
	_shared->_min_chkpt_rec_lsn = 
	_shared->_curr_lsn = 
	_shared->_durable_lsn = null_lsn;  // set_durable(...)

    if(reformat) {
	smlevel_0::errlog->clog << error_prio 
	    << "Reformatting log..." << flushl;
	int i;
	for (i=0; i < max_open_log; i++)  {
	    _part[i].destroy();
	}
    } else { 
	smlevel_0::errlog->clog << error_prio 
	    << "Recovering log..." << flushl;
	/* 
	 * find last checkpoint  
	 */
	_read_master(_shared->_master_lsn, 
		_shared->_min_chkpt_rec_lsn);

	smlevel_0::errlog->clog << error_prio 
	    << "Master checkpoint at ..." 
	    << _shared->_master_lsn
	    << flushl;
    }
    DBG(<<"master=" << _shared->_master_lsn
        <<"min chkpt rec lsn=" << _shared->_min_chkpt_rec_lsn
    );

    /*
     *  Destroy all partitions less than _min_chkpt_rec_lsn
     *  Open the rest and close them.
     *  In order to find out what index a partition has,
     *  we have to peek at the first record in it.
     *  Determine the last partition that exists.
     */

    partition_number_t 	last_partition = 0;
    bool                last_partition_exists = false;
    off_t 		offset = 0;

    last_partition = 1;
    if(reformat) {
	offset = 0;
	_shared->_curr_lsn = 
	_shared->_durable_lsn = lsn_t(uint4(0), uint4(0));  // set_durable(...)
	// last_partition_exists = false;
    } else {
	raw_partition 	*p;
	int i;


	for (i=0; i < max_open_log; i++)  {
	    p = &_part[i];

	    DBG(<<"constructor: peeking at partition# " << i);
		
	    p->peek(0, true);
	    if(p->num() >= last_partition) {
		DBG(<<"new last_partition: partition# " << i);
		last_partition = p->num();
	        last_partition_exists = true;
		offset = p->size();
	    }
	    if (p->num() > 0)  {
	        if (p->num() < _shared->_min_chkpt_rec_lsn.hi())  {
		    DBG(<<"destroying partition# " << i);
		    p->destroy();
	        }
		else  {
		    p->open_for_read(p->num(), true);
		}
	    }
	    unset_current();
	}
	w_assert1((uint4)offset != partition_t::nosize);
	/* TODO : remove
	// _shared->_curr_lsn = 
	// _shared->_durable_lsn =  // set_durable(...)
        // 	lsn_t(uint4(last_partition), uint4(offset));
	*/

    }
    /*
     *  current and durable lsn are now initialized
     *  for  the purpose of sanity checks in open*()
     */

    _shared->_curr_lsn = 
    _shared->_durable_lsn = // set_durable(...)
	lsn_t(uint4(last_partition), uint4(offset));


    DBG(<< "reformat   = " << reformat
	<<" curr_lsn   = " << _shared->_curr_lsn
        <<" durable lsn= " << _shared->_durable_lsn
    );

    {
	/*
	 *  open the "current" partition
	 */

	DBG(<<" opening last_partition " << last_partition
		<< " expected-to-exist " << last_partition_exists
	    );
	partition_t *p = open(last_partition, last_partition_exists,
		true, true);
	/* XXXX error info lost */
	if(!p) {
	    smlevel_0::errlog->clog << error_prio 
	    << "ERROR: could not open log file for partition "
	    << last_partition << flushl;
	    W_FATAL(eOS);
	}

	w_assert3(p->num() == last_partition);
	w_assert3(partition_num() == last_partition);
	w_assert3(partition_index() == p->index());

#ifdef DEBUG
	{
	raw_partition *r = (raw_partition *)p;
	off_t eof = r->size();
	w_assert1((uint4)eof != partition_t::nosize);
	w_assert3(last_partition == durable_lsn().hi());
	w_assert3(last_partition == curr_lsn().hi());
	w_assert3((uint4)eof == durable_lsn().lo());
	w_assert3((uint4)eof == curr_lsn().lo());
	}
#endif
    }
    DBG( << "partition num = " << partition_num()
	    <<" current_lsn " << curr_lsn()
	    <<" durable_lsn " << durable_lsn());

    compute_space(); // does a sanity check
    w_assert3(curr_lsn() != lsn_t::null);



#ifdef DEBUG
    DBG(<<"************************");
    partition_t *p;
    for (uint i = 0; i < max_open_log; i++)  {
	DBG(<<"Partition " << i );
	p = i_partition(i);
	if( p->exists() ) {
	    DBG( << " EXISTS ");
	    DBG( << " NUM " << p->num());
	    DBG( << " size " << p->size());
	}
	if( p->is_open_for_read() ) {
	    DBG( << " OPEN-READ ");
	}
	if( p->is_open_for_append() ) {
	    DBG( << " OPEN-APP ");
	}
	if( p->flushed() ) {
	    DBG( << " FLUSHED ");
	}
	if( p->is_current() ) {
	    DBG( << " CURRENT ");
	}
    }
#endif

    smlevel_0::errlog->clog << error_prio << endl<< "Log recovered." << flushl;
}



/*********************************************************************
 * 
 *  raw_log::~raw_log()
 *
 *  Destructor. Close all open partitions.
 *
 *********************************************************************/
raw_log::~raw_log()
{
    FUNC(raw_log::~raw_log);

    w_rc_t e;
    if (_fhdl_rd)  {
	e = me()->close(_fhdl_rd);
	if (e) {
		cerr << "warning: raw_log on close(rd):" << endl << e << endl;
	}
	_fhdl_rd = 0;
    }
    if (_fhdl_app )  {
	e = me()->close(_fhdl_app);
	if (e) {
		cerr << "warning: raw log on close(app):" << endl << e << endl;
	}
	_fhdl_app = 0;
    }
}


partition_t *
raw_log::i_partition(partition_index_t i) const
{
    return i<0 ? (partition_t *)0: (partition_t *) &_part[i];
}

void
raw_log::_read_master(
    lsn_t& l,
    lsn_t& min
) 
{
    FUNC(raw_log::_read_master);
    int fd = _fhdl_rd;
    w_assert3(fd);

    w_assert1(CHKPT_META_BUF == DEV_BLOCK_SIZE);

    DBG(<<"read_master seeking to " << 0);
    
    w_rc_t e;
    e = me()->lseek(fd, 0, SEEK_SET);
    if (e) {
	smlevel_0::errlog->clog << error_prio 
	    << "ERROR: could not seek to 0 to read checkpoint: "
	    << flushl;
	W_COERCE(e);
    }

    e = me()->read(fd, readbuf(), CHKPT_META_BUF);
    if (e) {
	smlevel_0::errlog->clog << error_prio 
	    << "ERROR: could not read checkpoint: "
	    << flushl;
	W_COERCE(e);
    } 
    istrstream s(readbuf(), readbufsize());
    lsn_t tmp; lsn_t tmp1; char  separator;

    if (! (s >> tmp >> separator >> tmp1) )  {
	smlevel_0::errlog->clog << error_prio 
	    << "Bad checkpoint master record -- " 
	    << " you must reformat the log."
	    << flushl;
	W_FATAL(eINTERNAL);
    }
    l = tmp;
    min = tmp1;
}

void
raw_log::_write_master(
    const lsn_t& l,
    const lsn_t& min
) 
{
    FUNC(raw_log::_write_master);
    /*
     *  create new master record
     */
    _make_master_name(l, min, _chkpt_meta_buf, CHKPT_META_BUF);

    DBG(<< "writing checkpoint master: " << _chkpt_meta_buf);

    int fd = fhdl_app();

    w_assert3(fd);

    DBG(<<"write_master seeking to " << 0);
    w_rc_t e = me()->lseek(fd, 0, SEEK_SET);
    if (e) {
	smlevel_0::errlog->clog << error_prio 
	    << "ERROR: could not seek to 0 to write checkpoint: "
	    << _chkpt_meta_buf << flushl;
	W_COERCE(e);
    }

    e = me()->write(fd, _chkpt_meta_buf, CHKPT_META_BUF);
    if (e) {
	smlevel_0::errlog->clog << error_prio
	    << "ERROR: could not write checkpoint: "
	    << _chkpt_meta_buf << flushl;
	W_COERCE(e);
    } 
}

/*********************************************************************
 * raw_partition class
 *********************************************************************/

void			
raw_partition::set_fhdl_app(int /*fd*/)
{
   // This is a null operation in the raw case because
   // the file descriptors are opened ONCE at startup.

   w_assert3(fhdl_app() != 0);
}

void
raw_partition::open_for_read(
    partition_number_t __num,
    bool err // = true -- if true, consider it an error
	// if the partition doesn't exist
) 
{
    FUNC(raw_partition::open_for_read);
    w_assert3(fhdl_rd() != 0);
    if( err && !exists()) {
	smlevel_0::errlog->clog << error_prio 
	    << "raw_log: partition " << __num 
	    << " does not exist"
	     << flushl;
	/* XXX bogus error info */
	W_FATAL(smlevel_0::eOS);
    }
    set_state(m_open_for_read);
    return ;
}

void
raw_partition::close(bool /*both*/)  // default: both = true
{
    FUNC(raw_partition::close);
    bool err_encountered=false;

    // We have to flush the writebuf
    // if it's "ours", which is to say,
    // if this is the current partition
    if(is_current()) {
	w_assert3(
	    writebuf().firstbyte().hi() == num()
		||
	    writebuf().firstbyte() == lsn_t::null);

	flush(fhdl_app());
	_owner->unset_current();
    }

    // and invalidate it
     w_assert3(! is_current());
    clr_state(m_open_for_read);
    clr_state(m_open_for_append);

    if(err_encountered) {
	W_FATAL(smlevel_0::eOS);
    }
}


void 
raw_partition::sanity_check() const
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
	// TODO-- rewrite this
	if(flushed() && _owner->writebuf()->firstbyte().hi()==num()) {
	    w_assert3(_owner->writebuf()->flushed().lo() == size());
	}
    }
}

void
raw_partition::_clear()
{
}

void
raw_partition::_init(srv_log *owner)
{
    clear();
    _owner = owner;
}

int
raw_partition::fhdl_rd() const
{
#ifdef DEBUG
    bool isopen = is_open_for_read();
    if(raw_log::_fhdl_rd == 0) {
	w_assert3( !isopen );
    }
#endif
    return raw_log::_fhdl_rd;
}

int
raw_partition::fhdl_app() const
{
    return raw_log::_fhdl_app;
}

/**********************************************************************
 *
 *  raw_partition::destroy()
 *
 *  Destroy a partition
 *
 *********************************************************************/
void
raw_partition::destroy()
{
    FUNC(raw_partition::destroy);
    // Write a skip record into the partition
    // Let the lsn of the skip record be determined
    // by   : num() if this partition already exists
    // ow by: _index if this is a matter of starting over,
    // (we don't have a num() for the partition).


    if(num()>0) {
	w_assert3(num() < _owner->global_min_lsn().hi());
	w_assert3(  exists());
	w_assert3(! is_current() );
	w_assert3(! is_open_for_append() );
    } else {
	_num = _index;
    }

    // must remember the current partition and number, close it,
    // write the skip record, and then reopen the partition, so
    // the writebuf's lsn's get set restored after the skip.

    long		thePartitionIndex = _owner->partition_index(); 
    partition_number_t	thePartitionNumber = 0;
    partition_t		*thePartition = 0;

    if (thePartitionIndex >= 0)  {
        thePartitionNumber = _owner->partition_num();
        thePartition = _owner->curr_partition();
        thePartition->close(true);
    }

    skip(lsn_t::null, fhdl_app());

    if (thePartitionIndex >= 0)  {
        thePartition->open_for_append(thePartitionNumber);
        thePartition->open_for_read(thePartitionNumber, true);
    }

    clear();
    clr_state(m_exists);
    clr_state(m_flushed);
    clr_state(m_open_for_read);
    clr_state(m_open_for_append);


    sanity_check();
}




/**********************************************************************
 *
 *  raw_partition::peek(num, recovery, fd) -- for raw only
 */
void
raw_partition::peek(
    partition_number_t num_wanted,
    bool 	recovery,
    int *	fdp
)
{
    FUNC(raw_partition::peek);
    int fd = fhdl_app();
    w_assert3(fd);
    if(fdp) *fdp  = fd;
    _peek(num_wanted, recovery, fd);
}

