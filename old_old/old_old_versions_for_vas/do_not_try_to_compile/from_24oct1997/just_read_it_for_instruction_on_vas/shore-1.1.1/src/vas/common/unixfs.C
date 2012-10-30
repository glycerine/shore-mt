/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include "unixfs.h"

fdentry::fdentry()
{ 
	_universe = is_none;
	my_fd = 0;
}
fdentry::~fdentry()
{ 
	if(_universe==is_unix) {
		if(syscall(SYS_close,my_fd) <0) {
			assert(errno!=0);
			return; 
		}
	} else {
		//??
	}
}

static fdlist _fdlist;

#ifdef notdef
#define USER_DEF_TYPE ReservedOid::_UserDefined,
#else
static lrid_t      garbage = ReservedOid::_UserDefined;
#define USER_DEF_TYPE garbage
#endif

fdlist::fdlist ()
{
	VASResult res;
	struct rlimit buf;

	_universe = is_unix;

#ifndef notdef
    garbage.serial.increment(1000);
    garbage.serial.increment(1000);
    garbage.serial.increment(1000);
#endif

	if(getrlimit(RLIMIT_NOFILE, &buf)<0) {
		perror("getrlimit(RLIMIT_NOFILE)");
		exit(1);
	}
	_openmax = buf.rlim_cur;
	_fds = new fdentry[_openmax];
	if(_fds==0) {
		// catastrophic
		cerr << "fdlist:: Cannot malloc" << endl;
		exit(1);
	}
	vas = newvas("localhost", 16,2,&res);
	if(!vas) {
		cerr << "fdlist:: Cannot connect to vas at localhost : " 
			<<  res
			<< endl;
		exit(1);
	}
	junkfd = /* creat(".unix.junk",0);*/
		syscall(SYS_open, ".unix.junk", O_WRONLY | O_CREAT | O_TRUNC, 0);
	if(junkfd<0) {
		cerr << "fdlist:: Cannot open junk file." << endl;
		exit(1);
	}
}

fdlist::~fdlist()
{
	assert(_fds);
	delete[]_fds;
}

fdentry * 
fdlist::locate_fd(int i) 
{
	fdentry *result;
	assert(_fds);

	if(i>_openmax) {
		return 0;
	}
	result = &_fds[i];

	assert((result->_universe == is_unix) ||
		(result->_universe == is_none) ||
		(result->_universe == is_shore));

	return result;
}
int		
fdlist::get_new_fd(int arg)
{
#define JUNKFD _fdlist.junkfd
	fdentry *fde;
	int		fd;
	// unix dup(2)
	if(arg>=0) {
		// caller wants a file descriptor larger than arg
		// has attributes of fd JUNKFD, unfortunately
		//
		if((fd = syscall(SYS_fcntl,JUNKFD,F_DUPFD,(int)arg))<0) {
			assert(errno!=0);
			return -1;
		}
	} else {
		// caller doesn't care what fd we allocate
		if((fd = syscall(SYS_dup,JUNKFD))<0) {
			assert(errno!=0);
			return -1;
		}
	}
	fde = _fdlist.locate_fd(fd);

	fde->my_fd = fd;
	assert(fde->_universe == is_none);
	assert(fd != 0); // for now
	return fd;
}

void	
fdlist::remove(int i) 
{
	fdentry * fde = _fdlist.locate_fd(i);
	assert(fde);
	fde->_universe = is_none;
}
fdentry *	
fdlist::install(int i)
{
	fdentry * fde = _fdlist.locate_fd(i);
	assert(fde);
	assert(fde->_universe == is_none);
	fde->_universe = is_unix;
	fde->tstart= NoText;
	fde->tsize= 0;
	return fde;
}
fdentry *	
fdlist::install(int i, 
	int flags,
	lrid_t lrid, 
	smsize_t tstart, 
	smsize_t tsize
) 
{
	fdentry * fde = _fdlist.locate_fd(i);
	assert(fde);
	assert(fde->_universe == is_none);
	fde->_universe = is_shore;
	fde->lrid = lrid;
	dassert(tstart != NoText);
	fde->tstart= tstart;
	fde->tsize= tsize;
	fde->openflags = flags;
	return fde;
}

/**************************** BEGIN MACRO DEFS ********************************/

#define Vas _fdlist.vas

#define NOT_IMPLEMENTED\
	errno = ENOSYS;

#define BEGIN_NESTED_TRANSACTION \
{ 	VASResult res;\
	if(Vas->trans(&__t) == SVAS_FAILURE) {\
		__nested = TRUE;\
		if(Vas->beginTrans(0,&__t)!=SVAS_OK) { SET_ERRNO; return -1; }\
	}

// NB: don't let the caller put a semicolon after this!

#define END_NESTED_TRANSACTION \
	if(__nested) {\
		res = Vas->commitTrans(__t);\
	}\
}

#undef _DO_
#define _DO_(x) if((Vas->x)!=SVAS_OK) { SET_ERRNO; }
#undef _DO_WORKED_ 
#define _DO_WORKED_ else

#define CHECKCONNECTED \
	if(_fdlist.vas && _fdlist.vas->connected()) {\
	} else {\
		delete _fdlist.vas;\
		_fdlist.vas = NULL;\
		errno =  EHOSTDOWN; \
		return -1;\
	}

#define SET_ERRNO\
	if(ISUERR(Vas->status.vasreason)) {\
		errno = UNMKUERR(Vas->status.vasreason);\
	} else {\
		errno = EPROTO;\
	}
#define ESREVINU\
	default:\
		assert(0);\
	}

#define PROLOGUE(fname)\
	errno=0; FUNC(fname);\

#define EPILOGUE\
	if(errno) { return  -1; } else { return 0; }

#define FDPROLOGUE(fname)\
	errno=0; FUNC(fname);\
	int cc=0;\
	fdentry *fde = _fdlist.locate_fd(fd);\
	bool __nested=FALSE; tid_t __t;\
	if(!fde) {\
		errno = EBADF;\
		return -1;\
	}\
	if(fde->_universe == is_none) {\
		fde->_universe = is_unix; /*for now*/ \
	}\
	switch(fde->_universe) {

#define FDEPILOGUE\
	ESREVINU;\
	EPILOGUE;

#define PATHPROLOGUE(fname)\
	errno=0; FUNC(fname);\
	bool __nested=FALSE; tid_t __t;\
	if(strncmp(path,"shore:",6)==0) {\
		/*chroot("/shore");*/  \
		_fdlist._universe = is_shore;\
	} else if(strncmp(path,"unix:",6)==0) /*chroot("/unix");*/  \
		_fdlist._universe = is_unix;\
	switch(_fdlist._universe) {

#define PATHEPILOGUE ESREVINU; EPILOGUE;

/****************************** END MACRO DEFS ********************************/

int	fchroot(int fd)
{
	FDPROLOGUE(fchroot)

	case is_none:
		_fdlist._universe = fde->_universe = is_unix;
		break;

	case is_unix:
	case is_shore:
		_fdlist._universe = fde->_universe;
		break;

	FDEPILOGUE
}

int	chroot(char *path)
{
	PROLOGUE(chroot);
	if(strcmp(path,"/unix")==0) {
		_fdlist._universe = is_unix;
	} else if(strcmp(path,"/shore")==0) {
		_fdlist._universe = is_shore;
	} else if(getuid()==0) {
		return syscall(SYS_chroot,path);
	} else {
		errno = ENOENT;
	}
	EPILOGUE
}

int open (char *path, int flags, mode_t  mode) 
{
	int fd;
	PATHPROLOGUE(open);

	case is_unix:
		return syscall(SYS_open, path, flags, mode);

	case is_shore: {
			LockMode lock = S;
			smsize_t tstart, tsize;
			lrid_t	lrid;
			if((fd = _fdlist.get_new_fd())<0) {
				return -1;
			}

			CHECKCONNECTED;
			BEGIN_NESTED_TRANSACTION
			if(errno!=0) { return -1; }

			if(flags & (O_WRONLY | O_RDWR | O_APPEND | O_TRUNC)) {
				lock = X;
			}
			if(Vas->lookup(path, &lrid)!=SVAS_OK) {
				// not found
				// should we try to create it?
				if((Vas->status.vasreason==SVAS_NoSuchObject) && 
					(flags & O_CREAT)) {
					errno = 0;
				} else {
					SET_ERRNO;
				}
				if(errno==0) { 	// does not exist.. create a UnixFile
					vec_t heap;
					vec_t core;
#ifdef JUNK
					_DO_(mkUnixFile(path, mode, heap, &lrid)) 
#endif
					_DO_(mkRegistered(path,mode,
						USER_DEF_TYPE,
						core, heap, 
						0, WholeObject, &lrid))

					_DO_WORKED_ {
						tstart = 0;
						tsize = WholeObject;
					}
					// we should have an X lock now
				}
			} else if((flags & (O_EXCL|O_CREAT))==(O_EXCL|O_CREAT)){
				errno = EEXIST;
			} else {
				// exists-- better make sure it's got a
				// text portion
				SysProps sysprops;
				_DO_(sysprops(lrid, &sysprops,TRUE/*whole page ok*/,lock))
				_DO_WORKED_ if(sysprops.tstart==NoText) {
					if(sysprops.type == ReservedSerial::_Directory) 
						errno = EISDIR;
					else
						errno = ENOENT; // no such unix file...
				} else {
					tstart =  sysprops.tstart;
					tsize = sysprops.tsize;
				}
			}
			END_NESTED_TRANSACTION
			if(errno==0) {
				// check permissions, flags
#ifdef notdef
	// we should already have an x lock
				if(flags & (O_WRONLY | O_RDWR | O_APPEND | O_TRUNC)) {
					_DO_(lockObj(lrid,X,FALSE));
				}
#endif
				if(flags & O_SYNC) {
					NOT_IMPLEMENTED
				}
			}
			if(errno==0) {
				(void)_fdlist.install(fd,flags,lrid,tstart,tsize);

				// TODO: what else do we have to do to 
				// maintain unix close semantics?
			}
		}
		break;

	PATHEPILOGUE
}

int creat (char *path, mode_t  mode) 
{
	return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

int close (int fd)
{
	FDPROLOGUE(close)

	case is_unix:
		_fdlist.remove(fd);
		(void) syscall(SYS_close,fd);
		break;

	case is_shore:
		_fdlist.remove(fd);
		CHECKCONNECTED;
		BEGIN_NESTED_TRANSACTION
			// TODO: what do we need to do here
			// for unix file close semantics
			// wrt destroyed objects?
		END_NESTED_TRANSACTION
		break;

	FDEPILOGUE
}

int getdents (int fd, char *buf, int nbytes)
{
	FDPROLOGUE(getdents)

	case is_unix:
		return syscall(SYS_getdents,fd, buf, nbytes);

	case is_shore:
		CHECKCONNECTED;
		BEGIN_NESTED_TRANSACTION
		{
			int nents;
			char *_buf;
			char *in, *out=buf;
			_entry *e;
			struct dirent *d;

			_buf = new char [nbytes];
			if(!_buf) {
				if(!errno) errno = ENOMEM;
			} else {
				in=_buf;

				_DO_(getDirEntries(fde->lrid,_buf,nbytes,&nents,&fde->cookie)) 
				_DO_WORKED_ {
					in = _buf;
					out = buf;
					for(int i=0; i < nents; i++) {
						e = (_entry *)in;
						d = (dirent *)out;

						d->d_off = i;
						d->d_fileno = e->serial.guts._low;
						d->d_reclen = e->entry_len;
						d->d_namlen = e->entry_len;
						strncpy(&d->d_name[0],&e->name,e->string_len+1);
						in += e->entry_len;
						out += d->d_reclen;
						cc += e->entry_len;
					}
					assert(cc == (int)(in-_buf));
				}
			} 
			delete _buf;
		}
		END_NESTED_TRANSACTION
		break;

	FDEPILOGUE
}

int	fstat(int fd,struct stat *statbuf)
{
	FDPROLOGUE(fstat)

	case is_unix:
		(void) syscall(SYS_fstat,fd,statbuf);
		break;

	case is_shore: {
			struct stat buf;
			struct SysProps sysprops;
			CHECKCONNECTED;
			BEGIN_NESTED_TRANSACTION

			dassert(sysprops.tag == KindRegistered);
			dassert(sizeof(dev_t)==sizeof(sysprops.volume.vid));
			buf.st_dev =  (dev_t)(sysprops.volume.vid);
			buf.st_rdev =  sysprops.volume.vid;

			dassert(sizeof(ino_t)==sizeof(sysprops.ref));
			buf.st_ino =  sysprops.ref._low;

			// TODO: what to do with 8-byte refs?
			buf.st_mode =  sysprops.specific_u.rp.mode;
			buf.st_nlink =  sysprops.specific_u.rp.nlink;
			buf.st_uid =  sysprops.specific_u.rp.uid;
			buf.st_gid =  sysprops.specific_u.rp.gid;
			buf.st_size =  sysprops.tsize;
			buf.st_atime =  sysprops.specific_u.rp.atime;
			buf.st_mtime =  sysprops.specific_u.rp.mtime;
			buf.st_ctime =  sysprops.specific_u.rp.ctime;
			buf.st_blksize =  Vas->pagesize();
			buf.st_blocks =  sysprops.csize + sysprops.hsize /
				Vas->pagesize();
			END_NESTED_TRANSACTION
		}
		break;
	FDEPILOGUE
}

int	stat(char *path,struct stat *statbuf)
{
	PATHPROLOGUE(stat)
	case is_unix:
		(void) syscall(SYS_stat,path,statbuf);
		break;

	case is_shore: 
		NOT_IMPLEMENTED;
		break;
	PATHEPILOGUE
}

int	lstat(char *path,struct stat *statbuf)
{
	PATHPROLOGUE(lstat)

	case is_unix:
		(void) syscall(SYS_lstat,path,statbuf);
		break;

	case is_shore: 
		NOT_IMPLEMENTED;
		break;
	PATHEPILOGUE
}

int	fcntl(int fd,int cmd,void *arg)
{
	FDPROLOGUE(fcntl)

	case is_unix:
		(void) syscall(SYS_fcntl,fd,cmd,arg);
		break;

	case is_shore: 
		// commands: F_DUPFD,F_GETFD, F_SETFD
		// F_GETFL, F_SETFL
		switch(cmd) {
		case F_DUPFD:
			int newfd = _fdlist.get_new_fd((int) arg);
			if(newfd!=0)
			break;
		case F_GETFD:
		case F_SETFD:
		case F_GETFL:
		case F_SETFL:
		case F_GETLK:
		case F_SETLK:
		case F_SETLKW:
		case F_GETOWN:
		case F_SETOWN:
		case F_RSETLK:
		case F_RSETLKW:
		case F_RGETLK:
		default:
			NOT_IMPLEMENTED
			break;
		}
		break;

	FDEPILOGUE
}

int	ioctl(int fd, int request, ...)
{
	va_list	args;
	va_start(args, request);
	FDPROLOGUE(ioctl)

	case is_unix:
		// TODO: make this use varargs
		(void) syscall(SYS_ioctl,fd,request,args);
		va_end(args);
		break;

	case is_shore: 
		NOT_IMPLEMENTED
		// _type x;
		// x = va_arg(args,_type);
		va_end(args);
		break;

	FDEPILOGUE
}

int	read(int fd,void *buf,size_t len)
{
	FDPROLOGUE(read)

	case is_unix:
		(void) syscall(SYS_read,fd,buf,len);
		break;

	case is_shore: 
		NOT_IMPLEMENTED
		break;

	FDEPILOGUE
}

int	readv(int fd,struct iovec *iov, size_t iov_len)
{
	FDPROLOGUE(readv)

	case is_unix:
		(void) syscall(SYS_readv,fd,iov,iov_len);
		break;

	case is_shore: 
		NOT_IMPLEMENTED
		break;

	FDEPILOGUE
}

int	write(int fd,void *buf,size_t len)
{
	FDPROLOGUE(write)

	case is_unix:
		(void) syscall(SYS_write,fd,buf,len);
		break;

	case is_shore: 
		NOT_IMPLEMENTED
		break;

	FDEPILOGUE
}
int	writev(int fd,struct iovec *iov, size_t iov_len)
{
	FDPROLOGUE(writev)

	case is_unix:
		(void) syscall(SYS_writev,fd,iov,iov_len);
		break;

	case is_shore: 
		NOT_IMPLEMENTED
		break;

	FDEPILOGUE
}

int	ftruncate(int fd,off_t newsize)
{
	FDPROLOGUE(ftruncate)

	case is_unix:
		(void) syscall(SYS_ftruncate,fd,newsize);
		break;

	case is_shore: 
		dassert(fde->tstart != NoText);
		if(fde->tsize == WholeObject) {
			// TODO add in tstart here
			_DO_(truncObj(fde->lrid,newsize));
		} else {
			NOT_IMPLEMENTED
		}
		break;

	FDEPILOGUE
}

int	readlink(char *path, char *buf, int len)
{
	PATHPROLOGUE(readlink);

	case is_unix:
		(void) syscall(SYS_readlink,path,buf,len);
		break;

	case is_shore: 
		NOT_IMPLEMENTED
		break;

	PATHEPILOGUE
}
int	unlink(char *path)
{
	PATHPROLOGUE(unlink);

	case is_unix:
		(void) syscall(SYS_unlink,path);
		break;

	case is_shore: 
		NOT_IMPLEMENTED
		break;

	PATHEPILOGUE
}

int	chdir(char *path)
{
	PATHPROLOGUE(chdir);

	case is_unix:
		(void) syscall(SYS_chdir,path);
		break;

	case is_shore: 
		NOT_IMPLEMENTED
		break;

	PATHEPILOGUE
}

int	fchdir(int fd)
{
	FDPROLOGUE(fchdir)

	case is_unix:
		(void) syscall(SYS_fchdir,fd);
		break;

	case is_shore: 
		NOT_IMPLEMENTED
		break;

	FDEPILOGUE
}

int fork()
{
	PROLOGUE(fork);

	if(_fdlist._universe == is_unix) {
		return syscall(SYS_fork);
	} else {
		NOT_IMPLEMENTED;
	}
	EPILOGUE;
}

int vfork()
{
	PROLOGUE(vfork);

	if(_fdlist._universe == is_unix) {
		return syscall(SYS_vfork);
	} else {
		NOT_IMPLEMENTED;
	}
	EPILOGUE;
}

off_t lseek(int fd, off_t offset, int whence)
{
	FDPROLOGUE(lseek)

	case is_unix:
		return (off_t) syscall(SYS_lseek,fd,offset,whence);
		break;

	case is_shore:
		NOT_IMPLEMENTED;
		break;

	FDEPILOGUE
}

int	dup2(int fd, int fd2)
{
	FDPROLOGUE(dup2)

	case is_unix:
		return syscall(SYS_dup, fd, fd2);

	case is_shore:
		NOT_IMPLEMENTED;
		break;

	FDEPILOGUE
}

int	dup(int fd)
{
	FDPROLOGUE(dup)

	case is_unix:
		return syscall(SYS_dup,fd);

	case is_shore:
		NOT_IMPLEMENTED;
		break;

	FDEPILOGUE
}

int	pipe(int *fdpair)
{
	PROLOGUE(pipe)

	switch(_fdlist._universe) {

	case is_unix:
		return syscall(SYS_pipe,fdpair);

	case is_shore:
		NOT_IMPLEMENTED;
		break;

	default:
		assert(0);
	}
	EPILOGUE
}

long	sysconf(int name)
{
	PROLOGUE(sysconf)

	switch(_fdlist._universe) {
	case is_unix:
		return (long) syscall(SYS_sysconf,name);
		break;

	case is_shore:
		NOT_IMPLEMENTED;
		break;
	default:
		assert(0);
	}

	EPILOGUE
}

mode_t umask(mode_t mask)
{
	PROLOGUE(umask);

	switch(_fdlist._universe) {
	case is_unix:
		return (mode_t) syscall(SYS_umask,mask);
		break;

	case is_shore: 
		NOT_IMPLEMENTED;
		break;
	default:
		assert(0);
	}

	EPILOGUE
}
int	pathconf(char *path, int name)
{
	PATHPROLOGUE(pathconf);

	case is_unix:
		(void) syscall(SYS_pathconf,path,name);
		break;

	case is_shore: 
		NOT_IMPLEMENTED
		break;

	PATHEPILOGUE
}

int	fpathconf(int fd, int name)
{
	FDPROLOGUE(fpathconf)

	case is_unix:
		(void) syscall(SYS_fpathconf,fd,name);
		break;

	case is_shore: 
		NOT_IMPLEMENTED
		break;

	FDEPILOGUE
}
