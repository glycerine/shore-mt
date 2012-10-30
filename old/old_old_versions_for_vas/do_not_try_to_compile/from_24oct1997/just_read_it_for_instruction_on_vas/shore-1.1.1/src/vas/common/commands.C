/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/commands.C,v 1.61 1997/09/06 22:40:17 solomon Exp $
 */
#include <copyright.h>
#include "shell.misc.h"
#include "vasshell.h"
#include "server_stubs.h"

#define BS 200000

static void
fill(
	lrid_t 		loid,
	char 		*buf,
	ObjectSize 	len
)
{
	FUNC(fill);
	int 	i;
	char 	*b = buf;
	int		rnum = rand() % 26;
	char	fillchar = 'A' + rnum;

	DBG(
		<< "Filling " << loid << " with " 
		<< len << " characters; fillchar=`" << fillchar << "`."
	)
	for(i=0, b = buf;i<len; i++,b++) {
		*b = fillchar;
	}
}
static void
display(
	ostream 	&out,
	lrid_t 		loid,
	char		*buf,
	ObjectSize 	len,
	bool		more
)
{
	FUNC(display);
	int 	i,lastrun;
	char 	*b = buf;
	char	lastchar;

	out << "Display of  " << loid << ": ";
	if(more) {
		out << "portion of length " << len << endl;
	} else {
		out << "size " << len  << endl;
	}
	for( lastchar = '\0', lastrun=0, i=0, b = buf;i<len; i++,b++) {
		if((*b != lastchar) || (lastrun==0)) {
			if(lastrun > 0) {
				out
					<< "[" << lastrun << "] "
				<< endl;
			}
			// print new char
			if(isprint(*b)) {
				out << *b << "     ";
			} else {
				out << "'\\x" << ::hex(((uint4)(*b))&0xff) << "'";
			}
			lastchar = *b;
			lastrun = 1;
		}else {
			lastrun ++;
		}
	}
	out
		<< "[" << lastrun << "] "
	<< endl;
	out
		<< "End of  " << loid << "."
	<< endl;
}

static char *
printperm(mode_t mode)
{
	static char x[10];
	register int i;

	for(i=0; i<(sizeof(x)-1); i++) {
		x[i] = '-';
	}
	x[sizeof x]='\0';

	if(mode & Permissions::Xpub) 	x[8] = 'x';
	if(mode & Permissions::Wpub) 	x[7] = 'w';
	if(mode & Permissions::Rpub) 	x[6] = 'r';
	if(mode & Permissions::Xgrp) 	x[5] = 'x';
	if(mode & Permissions::Wgrp) 	x[4] = 'w';
	if(mode & Permissions::Rgrp) 	x[3] = 'r';
	if(mode & Permissions::Xown) 	x[2] = 'x';
	if(mode & Permissions::Wown) 	x[1] = 'w';
	if(mode & Permissions::Rown) 	x[0] = 'r';
	if(mode & Permissions::SetGid) 	{
		x[5]= (x[5] == 'x')?'s':'S'; 
	}
	if(mode & Permissions::SetUid) 	{
		x[2]= (x[2] == 'x')?'s':'S'; 
	}
	if(mode & Permissions::Sticky) 	{
		x[8]= (x[8] == 'x')?'t':'T'; 
	}
	return x;
}

static
const char *
When(const c_time_t &w) 
{
	static char 			timestr[26];
	strcpy(timestr,ctime(&w));
	// replace the newline with a blank
	assert(timestr[24]=='\n');
	timestr[24] = ' ';
	return timestr;
}

static
const char *
Now() 
{
	static char 			timestr[26];
	static struct timeval   _Now;
	struct timezone         _Zone;
	if(gettimeofday( &_Now, &_Zone)<0) {
		catastrophic("gettimeofday");
	}
	strcpy(timestr,ctime((const long *)&_Now.tv_sec));
	// replace the newline with a blank
	assert(timestr[24]=='\n');
	timestr[24] = ' ';
	return timestr;
}

struct timeval &
Yesterday() 
{
	static struct timeval   _Now;
	struct timezone         _Zone;
	if(gettimeofday( &_Now, &_Zone)<0) {
		catastrophic("gettimeofday");
	}
	_Now.tv_sec -= 86400;
	return _Now;
}

static char 
typc(Ref type, smsize_t tstart)
{
	// These are all the possible types of registered objects:
		// UNIX uses
		//	d:directory b:block special c:char special
		//	l:symbolic link s:socket  -:plain file
		//
		// We'll use d,l,- 	and add x,p,m.i,u
		//
	DBG(<<"type == " << type );
	if(type == ReservedSerial::_Directory) 			return 'd';
	else if(type==ReservedSerial::_Symlink)		return 'l';
	else if(type == ReservedSerial::_Xref)			return 'x';
	else if(type==ReservedSerial::_Pool)			return 'p';
	else if(type==ReservedSerial::_UserDefined)	return 'u';
	else if(tstart!=NoText) return '-';

	return '?'; 

}

static void 
psizeinfo(ostream &o,
	const SysProps &sysprops
) 
{
	o << " " << sysprops.csize << "+" << sysprops.hsize
					<< "+" << sysprops.nindex << "i";
}

static void 
ptextinfo(ostream &o,
	const SysProps &sysprops
) 
{
	if(sysprops.tstart != NoText) {
		o << ":" << sysprops.tstart  ;
	} else {
		o << "!" ;
	}
}

class entry_args {
public:
	Tcl_Interp* 	_ip;
	ostream	 		&out;
	lsflags 		flags;
	const char		*msg;
	int				depth;
	lrid_t			oid;
	SysProps 		sysprops;
private:
	char 			*_name;
	bool 			_name_is_dynamic;
public:
	char 			*name() const { return _name; }

private:
	void freename() {
		if(_name != 0 && _name_is_dynamic) {
			delete[] _name;
			_name = 0;
		}
	}
public:
	void copyname(char *n) {
		copyname(n, strlen(n));
	}
	void copyname(char *n, int len) {
		freename();
		_name = new char[len+1];
		if(_name==0) {
			cerr << "malloc failed" << endl;
			exit(1);
		}
		strncpy(_name, n, len);
		_name[len] = '\0';
		_name_is_dynamic = true;
	}
	void setname(char *n, bool dynamic=false) {
		freename();
		_name_is_dynamic = dynamic;
		_name = n;
	}

public:
	~entry_args () {
		freename();
	}

	entry_args (
		Tcl_Interp* 	__ip,
		ostream	 		&ostr
	) : // KEEP IN ORDER OF APPEARANCE IN THE CLASS
		_ip(__ip), 
		out(ostr),
		flags(ls_none), 
		msg(0), 
		depth(0), 
		oid(lrid_t::null),
		_name(0)
	{
		// oid.lvid = lvid_t::null;
		// oid.serial = serial_t::null;
		memset(&sysprops, '\0', sizeof(sysprops));
	}

	entry_args(
		const entry_args &from
	): // KEEP IN ORDER OF APPEARANCE IN THE CLASS
		_ip(from._ip), 
		out(from.out), 
		flags(from.flags), 
		msg(0), 
		depth(from.depth +1), 
		oid(from.oid), 
		sysprops(from.sysprops),
		_name(0)
	{
			copyname(from.name());
			rmflags((unsigned int)ls_z);
	}
#define RMFLAGS(x,y) \
		x  = (lsflags) (((unsigned int)x) & ~((unsigned int)(y))) 

#define ADDFLAGS(x,y) \
		x = (lsflags) (((unsigned int)x) | (unsigned int)(y))

	void addflags(unsigned int y) {
		ADDFLAGS(flags,y);
	}
	void rmflags(unsigned int y) {
		RMFLAGS(flags,y);
	}
};
typedef void (*entryfunc)(ClientData, entry_args &arg );

static gde(ClientData clientdata, entryfunc, entry_args &);

static void
statitem( 
	ClientData		clientdata,
	entry_args		&arg
)
{
	CMDFUNC(statitem);
	const char		*_msg;

	c_time_t	notime = 0;
 	const char 	*timestr = When(notime); // get ptr to static data area(grot)

	arg.out << "OID " << arg.oid << endl;
	arg.out << "volume " << arg.sysprops.volume  << endl;
	arg.out << "ref    " << arg.sysprops.ref << endl;
	arg.out << "type   " <<  arg.sysprops.type << endl ;
	arg.out << "csize  " <<  arg.sysprops.csize << endl ;
	arg.out << "hsize  " <<  arg.sysprops.hsize << endl ;
	arg.out << "tstart " <<  arg.sysprops.tstart << endl ;
	arg.out << "nindex " <<  arg.sysprops.nindex << endl ;
	arg.out << "tag    " <<  arg.sysprops.tag << endl ;

	switch(arg.sysprops.tag) {

	case	KindTransient:
			break;

	case	KindAnonymous:
			arg.out << "pool   " <<  arg.sysprops.anon_pool << endl ;
			break;

	case	KindRegistered:
			arg.out << "nlink  " << arg.sysprops.reg_nlink  << endl;
			arg.out << "mode   " << arg.sysprops.reg_mode << endl;
			arg.out << "uid    " << arg.sysprops.reg_uid  << endl;
			arg.out << "gid    " << arg.sysprops.reg_gid  << endl;
			(void) When(arg.sysprops.reg_atime);
			arg.out << "atime  " << timestr << endl;
			(void) When(arg.sysprops.reg_mtime);
			arg.out << "mtime  " << timestr << endl;
			(void) When(arg.sysprops.reg_ctime);
			arg.out << "ctime  " << timestr << endl;
			break;
	}
	DBG(<<"leaving statitem depth " << arg.depth );
}

static void
lsitem( 
	ClientData		clientdata,
	entry_args		&arg
)
{
	CMDFUNC(lsitem);
	const char		*_msg;

	c_time_t	notime = 0;
 	const char 	*timestr = When(notime); // get ptr to static data area(grot)

	DBG(<<"lsitem depth=" << arg.depth 
		<< " flags=" << ::hex((unsigned int)arg.flags)
		<< " name=" << arg.name()
		);

	if(arg.flags & ls_g)  {
		arg.addflags((unsigned int)ls_l);
	}

	if(((arg.flags & ls_a)==0) && 
		(ReservedSerial::_Directory!=arg.sysprops.type) &&
		(*arg.name()=='.'))  return;

	// if it's a directory..
	_msg = arg.msg;
	if(ReservedSerial::_Directory==arg.sysprops.type) {
		if (arg.flags & ls_d) {
			_msg = 0;
		}
		if(arg.flags & ls_z) {
			_msg = 0;
		}
	} 
	if(_msg){
		arg.out << _msg << ":" << endl ;
	}
	if((arg.flags & (ls_z|ls_R)) &&
		(ReservedSerial::_Directory==arg.sysprops.type)) {
		entry_args	newarg(arg);

		DBG(<<"recursive gde with name " << arg.name());
		(void) gde(clientdata, lsitem, newarg);
		if(arg.flags & ls_z) return;
		if(/*(arg.flags & ls_R)&&*/ (res!=SVAS_OK)) return;

	}
	if(arg.flags & (ls_i|ls_ii|ls_iii)) {
		if(arg.flags & ls_iii){
			// most gory detail, including network address
			arg.out << arg.oid << "\t" ;
		} else if(arg.flags & ls_ii){
			// volume & serial
			arg.out 	
				<< arg.oid.lvid.low << "." 
				<< arg.oid.serial << "\t" ;
		} else {
			arg.out 	<< arg.oid.serial << "\t" ;
		}
	}
	if(arg.flags & ls_l) {
		arg.out
			<< " " << typc(arg.sysprops.type, arg.sysprops.tstart)
			<< printperm(arg.sysprops.reg_mode) 
			<< " " << arg.sysprops.reg_nlink ;
			;
		if(arg.flags & ls_n) {
			arg.out
				<< " \t" << arg.sysprops.reg_uid  ;
		} else {
			arg.out
				<< " \t" << uid2uname(arg.sysprops.reg_uid) ;
		}
		if(arg.flags & ls_g) {
			if(arg.flags & ls_n) {
				arg.out << " \t" << arg.sysprops.reg_gid  ;
			} else {
				arg.out << " \t" << gid2gname(arg.sysprops.reg_gid)  ;
			}
		}
		if(arg.flags & ls_s) {
			int x = (int)(arg.sysprops.csize + arg.sysprops.hsize);
			x = x/1024;
			if(x==0) x = 1;
			arg.out << " " << ::form("%11d",x) ;
		} else {
			psizeinfo(arg.out, arg.sysprops);
		}

		// fill in the timestr (static area)
		if(arg.flags & ls_u) {
			(void) When(arg.sysprops.reg_atime);
		} else if(arg.flags & ls_c) {
			(void) When(arg.sysprops.reg_ctime);
		} else {
			(void) When(arg.sysprops.reg_mtime);
		}
		arg.out << " \t" << timestr  ;
	}
	arg.out << " " << arg.name() ;

	if(arg.flags & ls_F) {
		switch( typc(arg.sysprops.type,arg.sysprops.tstart)) {
		case 'd': // dir
			arg.out << "/"  ;
			break;
		case 'l': // symlink
			arg.out << "@" ;
			if (arg.flags & ls_l) {
				ObjectSize  symlen = 2000;
		/*NEW*/	char		*buf = new char[symlen];
				vec_t  		sym(buf,symlen);

				arg.out << " -> ";
				if( Vas->readLink(arg.name(), sym, &symlen) != SVAS_OK)  break;
				buf[symlen] = '\0';
				arg.out << buf ;
		/*DEL*/	delete [] buf;
			}
			break;
		case 'x': // xref
			arg.out << "#"  ;
			if (arg.flags & ls_l) {
				lrid_t	contents;
				arg.out << " -> ";
				if(Vas->readRef(arg.name(), &contents)!=SVAS_OK) break;
				arg.out << contents ;
			}
			break;
		case '-': // unixfile
		case 'u': // user-defined type
			ptextinfo(arg.out, arg.sysprops);
			break;
		default:
			break;
		}
	}
	arg.out<< endl ;
	DBG(<<"leaving lsitem depth " << arg.depth );
}

static VASResult
apply(
	ClientData 	clientdata,
	entryfunc 	func,
	entry_args	&arg
) 
{
	CMDFUNC(apply);

	// takes path or oid.
	if(IS_OID(arg._ip, arg.name(), arg.oid)) {
		CALL(sysprops(arg.oid, &arg.sysprops, TRUE, SH));
	} else if(arg.oid.serial != serial_t::null) {
		CALL(sysprops(arg.oid, &arg.sysprops, TRUE, SH));
	} else {
		CALL(sysprops(arg.name(), &arg.sysprops));
		arg.oid.lvid = arg.sysprops.volume;
		arg.oid.serial = arg.sysprops.ref;
	}
	DBG(
		<< "apply depth=" << arg.depth << " res = " << res
	)
	if(res == SVAS_OK)  {
		if((func==statitem) || (arg.sysprops.tag == KindRegistered))  {
			(*func)(clientdata, arg);
		} else if((func == lsitem) &&
			(arg.sysprops.tag == KindAnonymous))  {
			arg.out 	
				<< arg.oid
				<< " \t" << typc(arg.sysprops.type,arg.sysprops.tstart)
				<< " \t" ;

			psizeinfo(arg.out, arg.sysprops);

			arg.out << arg.name();

			ptextinfo(arg.out, arg.sysprops);
			arg.out << endl ;
		} else {
			arg.out 	<< "unknown kind (tag) or bad func"
				<< " at line " << __LINE__ 
				<< " of file " << __FILE__  
				<< endl ;
		}
	}
	DBG(
		<< "leaving apply depth=" << arg.depth << " res = " << res
	)
	return res;
}

static int
_gde(	
	ClientData clientdata,
	int amt, 
	entryfunc efunc,
	entry_args &arg
)
{	
	Cookie 		cookie= NoSuchCookie;
	char 		*Dirent = NULL, *dirent;
	int			nentries=0, nbytes=1000;
	_entry		*se;
	_entry		*se_saved_for_delete;
	Tcl_Interp* 	ip= arg._ip;
	CMDFUNC(gde);

	CHECKCONNECTED;
	
/*NEW*/
	// NB: have to make sure this malloc-ed
	// space is aligned properly, so we'll
	// allocated it as an array of _entries,
	// then cast to a char *:
	se = new _entry[amt/sizeof(_entry)];
	se_saved_for_delete = se;
	Dirent = dirent = (char *)se;
	if(!Dirent) {
		Tcl_AppendResult(ip, "cannot calloc a buffer that big.",0);
		return(TCL_ERROR);
	}

	if(!arg.name()) {
		DBG(<<"");
		Tcl_AppendResult(ip, "\".\": No such directory.", 0);
		goto fail;
	}
	// error if not found
	bool found;
	CALL(lookup(arg.name(), &arg.oid, &found));
	if(res !=SVAS_OK) {
		DBG(<<"");
		goto vasfail;
	}
	// SVAS_OK
	if(!found) {
		DBG(<<"");
		Tcl_AppendResult(ip, "\".\": No such directory.", 0);
		goto fail;
	}
	dassert(! arg.oid.serial.is_null() );

	DBG(<<"depth " << arg.depth << " dir is " << arg.oid);

	res = SVAS_OK;
	do {
		dirent = Dirent;
		DBG(
			<< "calling Vas->getDirEntries with cookie" 
			<< ::hex((u_long)cookie) 
			<< ", nbytes" 
			<< nbytes
		)
		// re-user dirent space
		CALL(getDirEntries(arg.oid, dirent,  nbytes, &nentries, &cookie));

#ifdef DEBUG
		if(cookie == TerminalCookie) {
			DBG(<< "TERMINAL");
		} else {
			DBG(<< "NOT TERMINAL");
		}
#endif
		DBG(
			<< "Vas->getDirEntries returned cookie" << 
			::hex((u_long)cookie) << ", Res" << res
			<< "nentries" << nentries << 
			", nbytes" << nbytes
		)
		if(res == SVAS_OK) {
			int 	i;
			char	*p;

			if(nentries == 0) {
				arg.out << "<end of directory>.\n" ; 
				break;
			}

			for( p=dirent, i=0; 
				i< nentries; 
				i++, p += se->entry_len) {

				entry_args newarg(arg);

				se = (_entry *)p;
#ifdef DEBUG
				{
					int alignment = sizeof(serial_t);
					alignment -= 1;
					// check alignment:
					assert( (((unsigned int)se) &  alignment)==0 );
				}
#endif

				DBG(<<"i=" << i
					<< " p=" << ::hex((unsigned int)p)
					<< " se=" << ::hex((unsigned int)se)
					<< " nentries=" << nentries
					);
				DBG(<<"entry: magic=" << se->magic
					<< " serial=" << se->serial
					<< " entry_len=" << se->entry_len
					<< " string_len=" << se->string_len
					<< " name=" << se->name
				);

				DBG(<<"newarg.flags=" << ::hex((unsigned int)newarg.flags));

				// unless the a flag is on, skip "." files
				if(se->name == '.') {
					// don't print names that begin with "."
					if((newarg.flags & ls_a)==0)  {
						DBG(<<" skipping " << se->name);
						continue;
					}

					// don't recursively print "." or ".."
					if(newarg.flags & ls_R) {
						char *c = &(se->name);
						c++;
						if(*c=='.' || *c=='0') {
							DBG(<<" skipping " << se->name);
							continue;
						}
					}
				}

				// newarg.name() = &(se->name);
				newarg.copyname(&(se->name),se->string_len);

				// ok-- name and oid are set in newarg.
				// Now get the sysprops info so we can list the entry

				// newarg.oid.volume = arg.volume;
				newarg.oid.serial = se->serial;
				DBG(<<"calling sysprops on " << newarg.oid);

				if(Vas->sysprops(newarg.oid, &newarg.sysprops) != SVAS_OK) {
					cerr << "error getting sysprops for " << newarg.oid << endl;
					continue;
				}
				if(newarg.sysprops.volume != newarg.oid.lvid) {
					DBG(<<"SNAPPED!");
					if((newarg.oid.serial != ReservedSerial::_RootDir) 
						&&
						(newarg.sysprops.ref != ReservedSerial::_RootDir.guts))
					{
					/*
						cerr << "snapped " << newarg.oid <<  // fatal error
							" to " << newarg.sysprops.volume
							<< "." << newarg.sysprops.ref
							<< endl;
						// registered objects should never be remote
						// but this could be a special case for root
						cerr << "MOUNT POINT?" << endl;
					*/
					}
				}
				(*efunc) (clientdata, newarg);
				DBG(<<"returned from efunc");
			}
		} else if(Vas->status.vasreason == OS_NotADirectory) {
			DBG(<< "not a dir");
		} else {
			DBG(<< "UNEXPECTED ERR" << Vas->status.vasreason);
		}
		DBG(<<"while loop");
	} while((res == SVAS_OK)  && cookie != TerminalCookie);
	DBG(<<"end _gde depth=" << arg.depth);

vasfail:
/*DEL*/ 
	if(se_saved_for_delete) {
		delete [] se_saved_for_delete;
	}
	VASERR(res);
fail:
/*DEL*/ 
	if(se_saved_for_delete) {
		delete [] se_saved_for_delete;
	}
	return TCL_ERROR;
}

static int
gde(
	ClientData clientdata,
	entryfunc efunc,
	entry_args	&arg
)
{
	return _gde(clientdata, 1000, efunc, arg);
}

/*************************    cmd_*    **************************************/

// The result of this ***MUST*** be deleted by the caller
// if it's non-null.
enum printMntOptions {
	vol_id, mnt_id, root_id, all_info
};

char *
printMnt(FSDATA *fsd, int nstructs, bool _verbose, printMntOptions options)
{
	FUNC(printMnt);
	int 	i;
	char 	*format ="%6d %6d %6d %6d %6d ";
	char 	*tformat="\n%6.6s %6.6s %6.6s %6.6s %6.6s RootOid<-->Mounted on";

	tclout.seekp(ios::beg);
	dassert(!tclout.bad());
	if(nstructs == 0) {
		return NULL;
	}
	switch(options) {
		case all_info:
			// !verbose is the same as verbose
			tclout << ::form( tformat,
				"Tsize", "Bsize",
				"Blocks", "Free", "Avail")
			<< endl ;
			dassert(!tclout.bad());
			break;
		case  mnt_id:
			if(_verbose) { tclout << "Mounted on " << endl;  }
			break;
		case  vol_id:
			if(_verbose) { tclout << "Volume " << endl ; }
			break;
		case  root_id:
			if(_verbose) { tclout << "Volume root " << endl ;}
			break;
	}
	for(i=0; i< nstructs; i++, fsd++) {
		dassert(!tclout.bad());
		switch (options) {
		case all_info:
			// !verbose is the same as verbose
			tclout << ::form( format,
				fsd->tsize, fsd->bsize,
				fsd->blocks, fsd->bfree, fsd->bavail
			//	, fsd->files, fsd->ffree
				) ;
			tclout << fsd->root  << "<-->" << fsd->mnt  ;
			break;
		case  mnt_id:
			tclout << fsd->mnt << " "  ;
			break;
		case  vol_id:
			// print only volids of mounted filesystems
			// -- is input for dismount
			tclout << fsd->root.lvid << " "  ;
			break;
		case  root_id:
			tclout << fsd->root << " "  ;
			break;
		}
		tclout << endl;
	}
	tclout<< ends;
	return tclout.str();
}

int
cmd_getmnt(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	// getmnt [nbytes] [all=default | mnt | volid | root]
	CMDFUNC(cmd_getmnt);
#define QTY 10
	Cookie 		cookie = NoSuchCookie;
	FSDATA		*fsd;
	int			nstructs=0, nbytes=(QTY*sizeof(FSDATA));
	printMntOptions options = all_info;

	CHECK(1, 3, NULL);
	if(ac > 1)  {
		nbytes  = _atoi(av[1]);
	} 
	if(ac > 2)  {
		if(strcmp(av[2], "all")==0) {
			options = all_info;
		} else if(strcmp(av[2], "mnt")==0) {
			options = mnt_id;
		} else if(strcmp(av[2], "volid")==0) {
			options = vol_id;
		} else if(strcmp(av[2], "root")==0) {
			options = root_id;
		}
	}
/*NEW*/
	{
		fsd = new FSDATA[nbytes/sizeof(FSDATA)];  
		if(!fsd) {
			Tcl_AppendResult(ip, "Cannot calloc a buffer that big.",0);
			CMDREPLY(tcl_error);
		}
	}
	CHECKCONNECTED;

	res = SVAS_OK;
	do {
		DBG(<<"getMnt cookie=" << ::hex((unsigned int) cookie));
		CALL(getMnt(fsd,  nbytes, &nstructs, &cookie));

		if(res== SVAS_OK) {
			Tcl_AppendResult(ip, 
				printMnt(fsd, nstructs, verbose, options), 0);
		} else {
			Tcl_AppendResult(ip, "Error in getMnt", 0);
		}
	} while ((res == SVAS_OK) && (cookie != NoSuchCookie)) ;
/*DEL*/
	delete [] fsd;
	VASERR(res);
}

int 
cmd_pwd(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_pwd);
	static char		workingdir[200];
	lrid_t		obj;

	CHECK(1, 2, NULL);
	CHECKCONNECTED;
	if(ac>1) {
		SysProps s;

		DBG(<<"cmd_pwd argument is " << av[1]);

		BEGIN_NESTED_TRANSACTION

		if( !PATH_OR_OID_2_OID(ip, av[1], obj, 0, TRUE) ) {
			VASERR(Vas->status.vasresult);
		}
		if( Vas->sysprops(obj, &s)!=SVAS_OK) {
			Tcl_AppendResult(ip, "Error in sysprops().", 0);
			RETURN TCL_ERROR;
		}
		if(s.type != ReservedSerial::_Directory) {
			Tcl_AppendResult(ip, "Object is not a directory.", 0);
			RETURN TCL_ERROR;
		}
		Tcl_AppendResult(ip, Vas->gwd(workingdir, 100, &obj), 0);

		END_NESTED_TRANSACTION

	} else {
		BEGIN_NESTED_TRANSACTION
			Tcl_AppendResult(ip, Vas->gwd(workingdir, 100, 0), 0);
		END_NESTED_TRANSACTION
	}

	NESTEDVASERR(SVAS_OK, 0);
}

int 
cmd_cd(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_cd);
	// cd "path"

	CHECK(2, 2, NULL);
	CHECKCONNECTED;
	DBG( << "current working directory is: " << Vas->cwd())
	BEGIN_NESTED_TRANSACTION
		CALL(chDir(av[1]));
	DBG(<<"Vas returns with " << res);
	END_NESTED_TRANSACTION
	DBG( << "current working directory is: " << Vas->cwd())
	NESTEDVASERR(res, reason);
}

int 
dirisempty(
	ClientData clientdata,
	lrid_t  target, // of dir
	bool	&isempty
)
{
	CMDFUNC(dirisempty);
	Cookie 		cookie = NoSuchCookie;

	char 		*Dirent = NULL, *dirent;
	int			nentries=0, nbytes=1000;

	dirent 	= 	new char[nbytes];
	dassert(dirent);

	CALL(getDirEntries(target, dirent,  nbytes, &nentries, &cookie));
	if(res == SVAS_OK) {
		if(nentries > 2) {
			isempty = false;
		} else if(nentries == 2) {
			isempty = true;
		} else {
			dassert(nentries < 2);
			dassert(0);
		}
	}
	delete[] dirent;
	DBG(<<"dir " << (char *)(isempty?"is":"is not") 
		<< " empty" );
	return res;
}

int 
poolisempty(
	ClientData clientdata,
	lrid_t  target, // of pool
	bool	&isempty
)
{
	CMDFUNC(poolisempty);
	Cookie 		cookie = NoSuchCookie;
	int 		nobjs;
	bool		eof = FALSE; 
	lrid_t		loid;

	CALL(openPoolScan(target, &cookie));
	if(res != SVAS_OK) {
		return res;
	}
	DBG(<<"scan opened; first cookie =" << cookie);
	nobjs=0;
	while(1) {
		DBG(<<"cookie =" << cookie);
		CALL(nextPoolScan(&cookie, &eof, &loid));
		if(res != SVAS_OK) break;
		if(eof) break;
		nobjs++;
	}
	CALL(closePoolScan(cookie));
	isempty = (nobjs == 0)? true : false;
	return res;
}

int 
cmd_shorefile(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_shorefile);
	SysProps 	sysp;
	lrid_t		obj;

	bool		have_access=false;
	bool		do_access;
	AccessMode  access_wanted=access_none;
	char		typec = 'z';// illegit value
	bool		not_found_ok = false;

	static char *error_results[] = {
		"Not a pool or directory",
		"Option to shorefile not implemented",
	};

	enum		{
		// obvious:
		_atime, _mtime, _ctime,

		// oid of the object named by path (or oid)
		// in which case it's the identity func
		_oid, _lvid, _serial,

		// oid or object named by path exists
		// (does not follow links)
		_exists, 

		// exists and is anonymous
		// error if doesn't exist
		_isanonymous, 

		// is pool|dir and is empty
		// not error if not pool or dir
		_isempty, 

		// exists and is such an object
		// error if doesn't exist
		// doesn't follow links
		_isxref, _ispool, _isdirectory, _isfile, _isother,
		_islink,

		// returns mode, group or owner
		_mode, _group, _owner, 

		// true if group or owner is me
		_gowned, _owned, 

		// as in access(2)
		_readable, _writable, _exec,

		// heap + core
		_size, 

		// number of links
		_nlinks, 

		// oid of file or directory
		// error if not a directory or file
		_fileid,

		// oid of type of object
		_type,

		// not implemented
		_dirname,
		_extension,
		_rootname,
		_tail,
	} which;

	CHECK(3, 3, NULL);

	DBG(<<"cmd_shorefile " << av[1] );

	if(strcmp(av[1],"atime")==0) {
		which = _atime;
	} else if(strcmp(av[1],"ctime")==0) {
		which = _ctime;
	} else if(strcmp(av[1],"mtime")==0) {
		which = _mtime;
	} else if(strcmp(av[1],"serial")==0) {
		which = _serial;
	} else if(strcmp(av[1],"lvid")==0) {
		which = _lvid;
	} else if(strcmp(av[1],"oid")==0) {
		which = _oid;
	} else if(strcmp(av[1],"exists")==0) {
		which = _exists;
		not_found_ok = true;
	} else if(strcmp(av[1],"isanonymous")==0) {
		which = _isanonymous;
		not_found_ok = true;
	} else if(strcmp(av[1],"isempty")==0) {
		which = _isempty;
	} else if(strcmp(av[1],"isxref")==0) {
		not_found_ok = true;
		which = _isxref;
	} else if(strcmp(av[1],"ispool")==0) {
		not_found_ok = true;
		which = _ispool;
	} else if(strcmp(av[1],"isdirectory")==0) {
		not_found_ok = true;
		which = _isdirectory;
	} else if(strcmp(av[1],"isfile")==0) {
		not_found_ok = true;
		which = _isfile;
	} else if(strcmp(av[1],"isother")==0) {
		not_found_ok = true;
		which = _isother;
	} else if(strcmp(av[1],"islink")==0) {
		not_found_ok = true;
		which = _islink;
	} else if(strcmp(av[1],"mode")==0) {
		which = _mode;
	} else if(strcmp(av[1],"group")==0) {
		which = _group;
	} else if(strcmp(av[1],"owner")==0) {
		which = _owner;
	} else if(strcmp(av[1],"gowned")==0) {
		which = _gowned;
	} else if(strcmp(av[1],"owned")==0) {
		which = _owned;
	} else if(strcmp(av[1],"readable")==0) {
		which = _readable;
	} else if(strcmp(av[1],"writable")==0) {
		which = _writable;
	} else if(strcmp(av[1],"exec")==0) {
		which = _exec;
	} else if(strcmp(av[1],"size")==0) {
		which = _size;
	} else if(strcmp(av[1],"fileid")==0) {
		which = _fileid;
	} else if(strcmp(av[1],"type")==0) {
		which = _type;
	} else if(strcmp(av[1],"nlinks")==0) {
		which = _nlinks;
	} else {
		// default
		which = _size; // just to keep compiler happy
		Tcl_AppendResult(ip, "shorefile ", av[1], " is not implemented.",0);
		return TCL_ERROR;
	}
	CHECKCONNECTED;

	switch(which) {
		case _readable: 
			access_wanted = read_ok;
			do_access = true;
			break;
		case _writable:
			access_wanted = write_ok;
			do_access = true;
			break;
		case _exec: 
			access_wanted = exec_ok;
			do_access = true;
			break;
		default:
			do_access = false;
	}
	if(do_access) {
		DBG(<<"access");
		CALL(access(av[2], access_wanted, &have_access));
	} else if(PATH_OR_OID_2_OID_NOFOLLOW(ip, av[2], obj, 0, FALSE)) {
		DBG(<<"sysprops");
		CALL(sysprops(obj, &sysp));
	} else  if(not_found_ok) {
		// arg is not a path and not an oid
		// of anything known
		Tcl_AppendResult(ip, "0",0);
		VASERR(SVAS_OK); // returns
	} else {
		DBG(<<"lookup failed, command isn't _exists");
		res = Vas->status.vasresult;
	}
	if(res==SVAS_OK)  {
		DBG(<<"good");
		if(!do_access) {
			typec = typc(sysp.type,sysp.tstart);
		}

		switch(which) {

		case _fileid:
			{
				lrid_t result;
				CALL(fileOf(obj,&result));
				if(res == SVAS_OK) {
					obj = result;
					Tcl_AppendLoid(ip, (int)res, obj);
				} else {
					Tcl_AppendResult(ip, error_results[0] ,0);
				}
			}
			break;

		case _serial:
			obj.serial 	= sysp.ref;
			Tcl_AppendSerial(ip, res, obj.serial);
			break;

		case _nlinks:
			Tcl_AppendResult(ip, ::form("%d\0",sysp.reg_nlink), 0 );
			break;

		case _lvid:
			obj.lvid 	= sysp.volume;
			Tcl_AppendLVid(ip, res, obj.lvid);
			break;

		case _oid:
			obj.serial  = sysp.ref;
			obj.lvid 	= sysp.volume;
			Tcl_AppendLoid(ip, res, obj);
			break;

		case _type:
			obj.serial  = sysp.type;
			obj.lvid 	= sysp.volume;
			Tcl_AppendLoid(ip, res, obj);
			break;

		case _ctime:
			Tcl_AppendResult(ip, When(sysp.reg_ctime), 0);
			break;

		case _atime:
			Tcl_AppendResult(ip, When(sysp.reg_atime), 0);
			break;

		case _mtime:
			Tcl_AppendResult(ip, When(sysp.reg_mtime), 0);
			break;

		case _isempty:
			{ 	bool empty;
				char *answer = "0";

				if( typec=='p')  {
					if((poolisempty(clientdata, obj,empty)==SVAS_OK) && empty) {
						answer = "1";
					}
				} else if( typec=='d')  {
					if((dirisempty(clientdata, obj,empty)==SVAS_OK) && empty) {
						answer = "1";
					}
				} else {
					answer = error_results[0];
				}
				Tcl_AppendResult(ip, answer, 0);
			}
			break;

		case _isxref:
			Tcl_AppendResult(ip, ((typec=='x')?"1":"0"), 0);
			break;

		case _ispool:
			Tcl_AppendResult(ip, ((typec=='p')?"1":"0"), 0);
			break;

		case _isanonymous:
			Tcl_AppendResult(ip, ((sysp.tag==KindAnonymous) ?"1":"0"), 0);
			break;

		case _islink:
			Tcl_AppendResult(ip, ((typec=='l')?"1":"0"), 0);
			break;

		case _isdirectory:
			Tcl_AppendResult(ip, ((typec=='d')?"1":"0"), 0);
			break;

		case _isother:
			Tcl_AppendResult(ip, ((typec=='u')?"1":"0"), 0);
			break;
			
		case _isfile:
			Tcl_AppendResult(ip, ((typec=='-')?"1":"0"), 0);
			break;

		case _mode:
			Tcl_AppendResult(ip, form("0%o",sysp.reg_mode),0);
			break;

		case _owner:
			Tcl_AppendResult(ip, form("%d",sysp.reg_uid),0);
			break;

		case _group:
			Tcl_AppendResult(ip, form("%d",sysp.reg_gid),0);
			break;

		case _gowned:
			Tcl_AppendResult(ip, ((sysp.reg_gid == getgid())?"1":"0"), 0);
			break;

		case _owned:
			Tcl_AppendResult(ip, ((sysp.reg_uid == getuid())?"1":"0"), 0);
			break;

		case _readable: 
		case _writable:
		case _exec:
			Tcl_AppendResult(ip,  (have_access?"1":"0") , 0);
			break;

		case _size:
			Tcl_AppendResult(ip,::form("%d",sysp.csize+sysp.hsize),0);
			break;

		case _exists:
			Tcl_AppendResult(ip, "1",0);
			break;

		default:
			Tcl_AppendResult(ip, 
				"Unknown or unimplemented option for file command",0);
			break;
		}
	} else  if(not_found_ok) {
		// arg is an oid but no such object exists
		Tcl_AppendResult(ip, "0",0);
		res = SVAS_OK;
	} else {
		VASERR(res);
	}
	CMDREPLY(tcl_ok);
}

int 
cmd_stat(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_stat);
	tclout.seekp(ios::beg);
	dassert(!tclout.bad());
	entry_args	arg(ip, tclout);

	CHECK(2, 2, NULL);
	CHECKCONNECTED;

	arg.setname(av[1]);
	arg._ip 	 = 	ip;
	arg.flags = (lsflags)(ls_i|ls_l|ls_d|ls_a);
	arg.msg = 0;
	arg.depth =0;

	// apply does the sysprops()
	apply(clientdata, statitem,arg);
	if((res = Vas->status.vasresult) != SVAS_OK) {
		VASERR(res);
	} else {
		tclout << ends;
		dassert(!tclout.bad());
		Tcl_AppendResult(ip, tclout.str(), 0);
		CMDREPLY(tcl_ok);
	}
}

int 
cmd_ls(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_ls);
	int			_flags = ls_none;
	int			i;
	bool		explicit_path;

	ostrstream big; // let it be malloced as necessary

	big.seekp(ios::beg);
	dassert(!big.bad());
	big << "";
	entry_args	args(ip, big);

	// ls [-lnuFgiii] 
	// ls [options] [path [path]* ]
	// all options must precede first path

	CHECK(1, 3, NULL);
	for(explicit_path = FALSE, i=1; i<ac; i++) {
		char *p = av[i];
		if(*p == '-') while(*p)  {
			switch(*p) {
			case '-': 
				break;
//			case 'L': 
//				_flags |= ls_L; 
//				break;
			case 'c': 
				_flags |= ls_c; 
				break;
			case 'R': 
				Tcl_AppendResult(ip, "Recursive ls is not implemented.", 0);
				// _flags |= ls_R; 
				break;
			case 'd': 
				// turn off Recursive flag if -d is on
				_flags |= ls_d; 
				RMFLAGS(_flags,ls_R);
				break;
			case 's': 
				_flags |= ls_s; 
				break;
			case 'a': 
				_flags |= ls_a; 
				break;
			case 'l': 
				_flags |= ls_l; 
				break;
			case 'i': 
				if(_flags & ls_ii) _flags |= ls_iii;
				if(_flags & ls_i) _flags |= ls_ii;
				_flags |= ls_i; 
				break;
			case 'g': 
				_flags |= ls_g; 
				break;
			case 'F': 
				_flags |= ls_F; 
				break;
			case 'n': 
				_flags |= ls_n; 
				break;
			case 'u': 
				_flags |= ls_u; 
				break;
			
			default:
				Tcl_AppendResult(ip, "Unknown ls flag: ", p, 0);
			}
			p++;
		} else {
			// must be a path
			DBG(
				<< "explicit path at i=" << i << " av[i]=" << av[i] << "|"
			)
			explicit_path = TRUE;
			break;
		}
	}
	if((_flags & (ls_d|ls_R))==0) {
		ADDFLAGS(_flags,ls_z); 
	}

	CHECKCONNECTED;

	BEGIN_NESTED_TRANSACTION

	args.flags = (lsflags) _flags;
	args._ip = ip;
	args.depth = 0;

	if(explicit_path) {
		bool print_name=FALSE;
		args.msg = print_name?av[i]:0;
		DBG(
			<< "doing explicit from " << i << " to " << ac
		)
		if((ac-i > 1) || (_flags & ls_R)) {
			// multiple ones -- print hdr
			print_name=TRUE;
		} else if((_flags & ls_d)==0){
			args.addflags(ls_z); // one-level recursive
		}
		for(; i<ac; i++) {
			args.setname(av[i]);
			if((res = apply(clientdata, lsitem, args)) != SVAS_OK) break;
		}
	} else {
		DBG(
			<< "doing implicit \".\" "
		)
		args.setname(".");
		args.msg = 0;
		if((_flags & ls_d)==0){
			args.addflags(ls_z); // one-level recursive
		}
		res = apply(clientdata, lsitem, args);
	}
	END_NESTED_TRANSACTION
	big << ends;

	if(Vas->status.vasresult == SVAS_OK) {
		char *strbig = big.str();
		Tcl_AppendResult(ip, strbig, 0);
		delete strbig;
		CMDREPLY(tcl_ok);
	} else {
		// in this case, the error has
		// already been set once
		Tcl_ResetResult(ip);
		NESTEDVASERR(res, reason);
	}
}

int 
cmd_getdirentries(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_getdirentries);
	int nbytes;
	tclout.seekp(ios::beg);
	dassert(!tclout.bad());
	entry_args	args(ip, tclout);

	CHECK(1, 2, NULL);
	if(ac == 2)  {
		nbytes  = _atoi(av[1]);
	} 
	DBG( << "current working directory is: " << Vas->cwd())
	if(Vas->cwd().serial.is_null()) {
		DBG(<<"");
		Tcl_AppendResult(ip, "\".\": No such directory.", 0);
	}
	args._ip = ip;
	args.setname(".");
	args.flags = ls_iii;
	args.depth = 0;
	// args.lid = Vas->cwd();

	res = gde(clientdata, lsitem, args);
	tclout << ends;
	dassert(!tclout.bad());
	Tcl_AppendResult(ip, tclout.str(), 0);
	VASERR(res);
}

#ifdef USE_VERIFY
void pattern1(char *buf, int size)
{
    static int init = 0;

    if (init == 0) {
        srand(time(0) % 4000);
        init = 1;
    }

    for (int i = 0; i < size; ++i) {
        buf[i] = 'a' + (int)(rand() % 26);
    }
}
#endif

int 
cmd_statmindex(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_statmindex);
	// statmindex <path|oid>,int
	IndexId 	iid;

	CHECK(2, 2, NULL);
	if(!INDEXID(ip, av[1], iid)) {
		SYNTAXERROR(ip, " in command ", _fname_debug_);
	}
	indexstatinfo statbuf;
	CALL(statIndex(iid, &statbuf));
	if(res == SVAS_OK) {
		tclout.seekp(ios::beg);
		dassert(!tclout.bad());
		tclout << "index id=" << statbuf.fid.lvid << "." 
			<< statbuf.fid.serial << ", "
			<< statbuf.nentries << " entries, "
			<< statbuf.npages << " pages, " 
			<< statbuf.nbytes << " bytes." << ends;
		dassert(!tclout.bad());
		Tcl_AppendResult(ip, tclout.str(), 0);
	}
	VASERR(res);
}

int 
cmd_dropmindex(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_dropmindex);
	// dropmindex <path|oid>,int
	IndexId 	iid;

	CHECK(2, 2, NULL);
	if(!INDEXID(ip, av[1], iid)) {
		SYNTAXERROR(ip, " in command ", _fname_debug_);
	}
	CALL(dropIndex(iid));
	VASERR(res);
}
int 
cmd_addmindex(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_addmindex);
	// addmindex <path|oid>,int kind
	IndexKind	indexKind = BTree; // default
	IndexId 	iid;

	CHECK(3, 3, NULL);
	if(!INDEXID(ip, av[1], iid)) {
		SYNTAXERROR(ip, " in command ", _fname_debug_);
	}
	if(strcasecmp(av[2],"btree")==0) {
		indexKind = BTree;
	} else if(strcasecmp(av[2],"lhash")==0) {
		indexKind = LHash;
	} else if(strcasecmp(av[2],"rtree")==0) {
		indexKind = RTree;
	} else {
		Tcl_AppendResult(ip, "Unknown index kind", av[2], 0);
		CMDREPLY(tcl_error);
	}
	CALL(addIndex(iid, indexKind));
	VASERR(res);
}
int 
__cmd_mkanon(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(__cmd_mkanon);
	bool		isReg;
	lrid_t		loid, pool, pooloid;
	char		*cbuf, *hbuf;
	int			nindexes;
	enum		{ noref, withref } which;

	int			newoidarg, i;
	ObjectSize	csize, hsize;
	ObjectOffset	tstart;
	int			sizearg;
	int			typearg;
	bool		zeroed=false;

	// mkanon poolname/pooloid size [type]
	// mkanonwithref poolname/pooloid newoid size [type]

	if(strcasecmp(av[0],"mkanon")==0) {
		which = noref;
		CHECK(3, 4, NULL);
		newoidarg = 0;
		sizearg = 2;
		typearg = 3;
	}else if(strcasecmp(av[0],"mkzanon")==0) {
		which = noref;
		CHECK(3, 4, NULL);
		newoidarg = 0;
		sizearg = 2;
		typearg = 3;
		zeroed=true;
	}else if(strcasecmp(av[0],"mkanonwithref")==0) {
		which = withref;
		CHECK(4, 5, NULL);
		newoidarg = 2;
		sizearg = 3;
		typearg = 4;
	} else {
		Tcl_AppendResult(ip, "internal error -- mismatched cmd", av[0], 0);
		CMDREPLY(tcl_error);
	}
	DBG(<<"looking up path or oid");

	if( !PATH_OR_OID_2_OID(ip, av[1], pool, &isReg, TRUE) ) {
		DBG(<<"problem with path or oid");
		VASERR(Vas->status.vasresult);
	}

	// loid SHOULD have null serial # now
	DBG(
		<< "loid before mkanon: " << loid
	)

	if(newoidarg>0) {
		DBG(<<"given  oid with arg " << newoidarg <<"  -->" << av[newoidarg] << "<--");
		DBG(<<"av[2]=" << av[2]);
		DBG(<<"av[3]=" << av[3]);
		DBG(<<"av[4]=" << av[4]);
		if(!OID_2_OID(ip, av[newoidarg], loid)) {
			DBG(<<"problem with ");
			CMDREPLY(tcl_error);
		}
	}
	if(ac>typearg) {
		Tcl_AppendResult(ip, 
		"User-defined types are not yet supported: ", av[3], " ignored.", 0);
		CMDREPLY(tcl_error);
	}

	GET_SIZE(av[sizearg], &csize, &hsize, &tstart, &nindexes);

	if(!zeroed) {
		/*NEW*/
		cbuf = new char[csize]; 
		hbuf = new char[hsize];  

#ifdef USE_VERIFY
		pattern1(cbuf, (int)csize);
		pattern1(hbuf, (int)hsize);
#else
		for(i=0; i<(int)csize; i++) {
			cbuf[i] = 'c'; // for "core"
		}
		for(i=0; i<(int)hsize; i++) {
			hbuf[i] = 'h'; // for "heap"
		}
#endif
		if(tstart != NoText) {
			// overwrite the text part
			int hts; // first heap byte that's text
			if(tstart < csize) {
				for(i=tstart; i < (int)csize; i++) {
					cbuf[i] = 't'; // for "text"
				}
				hts = 0; // whole heap
			} else {
				hts = tstart - csize;
			}
			for(i=hts; i < (int)hsize; i++) {
				hbuf[i] = 't'; // for "text"
			}
		}
	}

	CHECKCONNECTED;

	if(isReg) {
		bool found=true;
		CALL(lookup(av[1], &pool, &found));
		if(res !=SVAS_OK) {
			VASERR(res);
		}
		if(!found) {
			Tcl_AppendResult(ip, "No such pool", av[1], 0);
			CMDREPLY(tcl_error);
		}
	}
	if(zeroed) {
		CALL(mkAnonymous(pool, ReservedOid::_UserDefined, 
			csize, hsize, tstart, nindexes, &loid));
	} else {
		vec_t		core(cbuf,(int)csize);
		vec_t		heap(hbuf,(int)hsize);
		CALL(mkAnonymous(pool, ReservedOid::_UserDefined, 
			core, heap, tstart, nindexes, &loid));
	}

	pooloid = pool;

#ifdef USE_VERIFY
	CHECK_TRANS_STATE;

	ostrstream m;
	m << loid << ends;
	tclout << pooloid << ends;
	dassert(!tclout.bad());
	char *strm = m.str();
	char *strn = tclout.str();
	char *vdata = new char [ csize + hsize + 1 ];

/*
	strncpy(vdata, cbuf, csize);
	strncat(vdata, hbuf, hsize);
	DBG( << "in mk_anon: oid is " << loid );
	DBG( << "in mk_anon: pooloid is " << pooloid );
	DBG( << "in mk_anon: vdata is " << vdata );
	DBG( << "in mk_anon: (csize + hsize) is " << (csize + hsize) );
*/
	for (i = 0; i < csize; ++i)  {
		vdata[i] = cbuf[i];
	}
	for (i = csize; i < csize + hsize; ++i)  {
		vdata[i] = hbuf[i - csize];
	}
	vdata[csize + hsize] = '\0';

	
	// not including text yet
	tclout << ends;
	dassert(!tclout.bad());
	v->create(strm, tclout.str(), vdata, csize + hsize);

	delete strm;
	delete [] vdata;

#endif

	DBG(
		<< "loid after mkanon: " << loid
	)
	Tcl_AppendLoid(ip, res, loid);
	DBG(
		<< "made in pool: " << pooloid
	)

	if(zeroed) {
		/*DEL*/
		delete [] cbuf;
		delete [] hbuf;
	}
	VASERR(res);
}
int 
cmd_mkanon(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_mkanon);
	return __cmd_mkanon(clientdata, ip, ac, av);
}
int
cmd_mkzanon(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_mkzanon);
	return __cmd_mkanon(clientdata, ip, ac, av);
}
int 
cmd_mkanonwithref(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_mkanonwithref);
	return __cmd_mkanon(clientdata, ip, ac, av);
}

int 
cmd_rmanon(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_rmanon);
	lrid_t		target;

	if( ! OID_2_OID(ip, av[1], target) ) {
		VASERR(Vas->status.vasresult);
	}
	CHECKCONNECTED;

	CALL(rmAnonymous(target));

	VASERR(res);
}

int 
cmd_mkdir(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_mkdir);
	lrid_t		loid;
	mode_t		mode = 0777;

	CHECK(2, 3, NULL);
	CHECKCONNECTED;
	if(GETMODE(1, 2, ac)!= TCL_OK) CMDREPLY(tcl_error);

	CALL(mkDir(av[1], mode, &loid));
	Tcl_AppendLoid(ip, res, loid);
	VASERR(res);
}

int 
cmd_mkpool(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_mkpool);
	lrid_t		loid;
	mode_t		mode = 0777;

	CHECK(2, 3, NULL);
	CHECKCONNECTED;
	if(GETMODE(1, 2, ac)!= TCL_OK) CMDREPLY(tcl_error);

	CALL(mkPool(av[1], mode, &loid));
	Tcl_AppendLoid(ip, res, loid);
	VASERR(res);
}

int 
cmd_mklink(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_mklink);
	mode_t		mode = 0777;

	// mklink oldpath newpath 

	CHECK(3, 4, NULL);
	CHECKCONNECTED;
	if(GETMODE(2, 3, ac)!= TCL_OK) CMDREPLY(tcl_error);

	if(ac==4) {
		Tcl_AppendResult(ip, "Mode not used for mklink", av[0], 0);
		CMDREPLY(tcl_error);
	}
	CALL(mkLink(av[1], av[2]));

	VASERR(res);
}

int 
cmd_mksymlink(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_mksymlink);
	lrid_t		loid;
	mode_t		mode = 0777;

	// mksymlink oldpath newpath [mode]

	CHECK(3, 4, NULL);
	CHECKCONNECTED;
	if(GETMODE(2, 3, ac)!= TCL_OK) CMDREPLY(tcl_error);

	CALL(mkSymlink(av[2], av[1], mode, &loid));
	Tcl_AppendLoid(ip, res, loid);
	VASERR(res);
}

int 
cmd_readlink(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_readlink);
	mode_t		mode; // unused but needed for "GETMODE"

	//readref name
	CHECK(2, 2, NULL);
	CHECKCONNECTED;
	if(GETMODE(0, 0, ac)!= TCL_OK) CMDREPLY(tcl_error);

	char buf[MAXPATHLEN+1];
	smsize_t  buflen = sizeof(buf);
	vec_t vec(buf,buflen);
	CALL(readLink(av[1], vec, &buflen));
	if(res == SVAS_OK) {
		buf[buflen] = '\0';
		Tcl_AppendResult(ip, buf, 0);
	}
	VASERR(res);
}
int 
cmd_readref(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_readref);
	lrid_t		loid;
	mode_t		mode; // unused but needed for "GETMODE"

	//readref name
	CHECK(2, 2, NULL);
	CHECKCONNECTED;
	if(GETMODE(0, 0, ac)!= TCL_OK) CMDREPLY(tcl_error);

	CALL(readRef(av[1], &loid));
	Tcl_AppendLoid(ip, res, loid);
	VASERR(res);
}

int
cmd_mkxref(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_mkxref);
	lrid_t		loid, target ;
	mode_t		mode = 0777;

	//mkxref targetoid name [mode]
	CHECK(3, 4, NULL);
	CHECKCONNECTED;
	if(GETMODE(2, 3, ac)!= TCL_OK) CMDREPLY(tcl_error);

	if( ! PATH_OR_OID_2_OID(ip, av[1], target, 0, TRUE) ) {
		VASERR(Vas->status.vasresult);
	}

	CALL(mkXref(av[2], mode, target, &loid));
	Tcl_AppendLoid(ip, res, loid);
	VASERR(res);
}

enum	objecttype {
	t_dir,
	t_hardlink,
	t_pool,
	t_unixfile,
	t_userdef,
	t_userdefnodata, 
	t_symlink,
	t_xref
	};

static int 
mkuserdef(ClientData clientdata,
	objecttype which, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(mkuserdef);
	lrid_t		loid;
	mode_t		mode = 0777;
	ObjectSize	csize, hsize;
	ObjectOffset tstart;
	char		*cbuf;
	char 		*hbuf;
	int			nindexes;
	int 		i;

	// mkuserdef  filename size [mode] (makes w/o initializing data)

	CHECK(3, 4, NULL);

	CHECKCONNECTED;
	if(GETMODE(1, 3, ac)!= TCL_OK) CMDREPLY(tcl_error);

	if(which == t_unixfile) {
		hsize = _atoi(av[2]);
		csize =0;
		tstart = 0;
	} else {
		GET_SIZE(av[2], &csize, &hsize, &tstart, &nindexes);
	}

/*NEW*/
	cbuf = new char[csize]; 
	hbuf = new char[hsize]; 

	for(i=0; i<csize; i++) {
		cbuf[i] = 'c'; // for "core"
	}
	for(i=0; i<hsize; i++) {
		hbuf[i] = 'h'; // for "heap"
	}
	if(tstart != NoText) {
		for(i=0; i<hsize; i++) {
			hbuf[tstart+i] = 't'; // for "text"
		}
	}
	vec_t		core(cbuf, (int)csize);
	vec_t		heap(hbuf, (int)hsize);

	switch(which) {
	case t_dir:
	case t_hardlink:
	case t_pool:
	case t_symlink:
	case t_xref:
		assert(0);
		break;

	case t_unixfile:
		DBG(<<" unix file ");
		// 		in {client,server}/stubs.C:
		res =  mkunixfile(ip,Vas,av[1],mode,heap,&loid);
		break;

	case t_userdef: 
		DBG(<<" user defined");
		CALL(mkRegistered(av[1], mode, 
			USER_DEF_TYPE,
			core, heap, tstart, nindexes, &loid));
		break;

	case t_userdefnodata: 
		CALL(mkRegistered(av[1], mode, 
			USER_DEF_TYPE,
			// TODO: need more detailed tests to control
			// coresize, heapsize, textinfo
			csize, hsize, tstart, nindexes, &loid));
		break;
	}

	Tcl_AppendLoid(ip, res, loid);
/*DEL*/
	delete [] hbuf;
	delete [] cbuf;
	VASERR(res);
}
int 
cmd_mkunixfile(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_mkunixfile);
	return mkuserdef(clientdata, t_unixfile, ip, ac, av);
}
int 
cmd_mkuserdefnodata(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_mkuserdefnodata);
	return mkuserdef(clientdata, t_userdefnodata, ip, ac, av);
}
int 
cmd_mkuserdef(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_mkuserdef);
	return mkuserdef(clientdata, t_userdef, ip, ac, av);
}

int 
unlinkcmd(ClientData clientdata,
	objecttype which, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(unlinkcmd);
	lrid_t		loid;
	// unlink "filename"

	bool	must_remove=false;

	CHECK(2, 2, NULL);
	CHECKCONNECTED;

	// let the Vas discover if the item is really there.
	switch(which) {

	case t_dir:
		CALL(rmDir(av[1]));
		break;

	case t_pool:
		CALL(rmPool(av[1]));
		break;

	case t_hardlink:
	case t_unixfile:
	case t_userdef:
	case t_symlink:
	case t_xref:
	case t_userdefnodata:
		CALL(rmLink1(av[1], &loid, &must_remove));
		if(res != SVAS_OK) goto bye;
		if(must_remove) {
			CALL(rmLink2(loid));
		} else {
			DBG(
				<< "Links exist; object not removed"
			)
		}
		break;

	// case t_symlink:
		// CALL(rmSymlink(av[1]));
		// break;

	// case t_xref:
		// CALL(rmXref(av[1]));
		// break;

	}
bye:
	VASERR(res);
}

int 
cmd_unlink(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_unlink);
	return unlinkcmd(clientdata, t_hardlink, ip, ac, av);
}
int 
cmd_rmsymlink(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_rmsymlink);
	return unlinkcmd(clientdata, t_symlink, ip, ac, av);
}
int 
cmd_rmxref(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_rmxref);
	return unlinkcmd(clientdata, t_xref, ip, ac, av);
}
int 
cmd_rmpool(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_rmpool);
	return unlinkcmd(clientdata, t_pool, ip, ac, av);
}
int 
cmd_rmdir(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_rmdir);
	return unlinkcmd(clientdata, t_dir, ip, ac, av);
}

int 
cmd_snapref(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_snapref);
	lrid_t		target;
	lrid_t		snapped;
	int			len;

	// snapref oid|path

	CHECK(2, 2, NULL);
	len = strlen(av[0]);
	CHECKCONNECTED;

	if(ac<=1) {
		Tcl_AppendResult(ip, "Missing argument.", 0);
		(void) append_usage_cmdi(ip, __cmd_snapref__);
		CMDREPLY(tcl_error);
	} else {
		if( ! PATH_OR_OID_2_OID(ip, av[1], target, 0, TRUE) ) {
			VASERR(Vas->status.vasresult);
		}
	}
	DBG( << "snapping: " << target )
	CALL(snapRef(target, &snapped));

	if(res == SVAS_OK) {
		Tcl_AppendLoid(ip, res, snapped);
	}
	VASERR(res);
}

int 
cmd_offvolref(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_offvolref);
	lrid_t		target;
	lrid_t		newref;
	int			len;

	// offvolref oid|path

	CHECK(2, 2, NULL);
	len = strlen(av[0]);
	CHECKCONNECTED;

	if(ac<=1) {
		Tcl_AppendResult(ip, "Missing argument.", 0);
		(void) append_usage_cmdi(ip, __cmd_offvolref__);
		CMDREPLY(tcl_error);
	} else {
		if( ! PATH_OR_OID_2_OID(ip, av[1], target, 0, TRUE) ) {
			VASERR(Vas->status.vasresult);
		}
	}
	DBG( << "offvolref(vol: " << Vas->cwd().lvid )
	CALL(offVolRef(Vas->cwd().lvid, target, &newref));
	if(res == SVAS_OK) {
		Tcl_AppendLoid(ip, res, newref);
	}
	VASERR(res);
}

int 
cmd_mkvolref(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_ref);
	lrid_t		snapped;
	int			len;

	// mkvolref number

	CHECK(2, 2, NULL);
	len = strlen(av[0]);
	CHECKCONNECTED;
	if(ac<=1) {
		len = 1;
	} else {
		len = _atoi(av[1]);
	}
	DBG( << "mkvolref(vol: " << Vas->cwd().lvid )
	CALL(mkVolRef(Vas->cwd().lvid, &snapped, len));
	if(res == SVAS_OK) {
		Tcl_AppendLoid(ip, res, snapped);
		DBG( << "mkvolref res = " << res  <<"  snapped= " << snapped);
		if(snapped.serial != serial_t::null) {
			if(strlen(ip->result)<4) {
				assert(0);
			}
		}
	}
	VASERR(res);
}

int 
cmd_lock(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_lock);
	lrid_t			target;
	LockMode		mode = NL;
	RequestMode		block = Blocking;

	// lock oid/name [NL | IS | IX | S | SIX | U | X] [Nonblocking|Blocking]

	CHECK(2, 4, NULL);
	CHECKCONNECTED;
	if(ac>2) {
		mode = getLockMode(ip, av[2]);
		if(ac>3) {
			block = getRequestMode(ip, av[3]);
		} 
	} 

	if( ! PATH_OR_OID_2_OID(ip, av[1], target, 0, TRUE) ) {
		VASERR(Vas->status.vasresult);
	}
	CALL(lockObj(target, mode, block));
	VASERR(res);
}

int 
cmd_umask(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	// umask mask 
	// umask 

	CMDFUNC(cmd_umask);
	uint		mask;

	CHECK(1, 2, NULL);

	if(ac>1) {
		mask = _atoi(av[1]);
		DBG(<<"setting umask to " <<  ::form("0%o", mask));
		CALL(setUmask(mask));
	} else {
		CALL(getUmask(&mask));
	}
	if(res == SVAS_OK) {
		Tcl_AppendResult(ip, ::form("0%o", mask), 0);
	} 
	VASERR(res);
}

enum permcommand { c_chmod, c_chown, c_chgrp };

static  int 
permcmd(ClientData clientdata,
	permcommand which, Tcl_Interp* ip, int ac, char* av[])
{
	// "chmod mode path",	mode = number
	// "chown owner path",	owner = uid
	// "chgrp group path",  group = gid

	CMDFUNC(permcmd);
	char		*path=0;
	uid_t		user=0;
	gid_t		group=0;
	mode_t		mode=0;

	CHECK(2, 3, NULL);
	CHECKCONNECTED;

	tclout.seekp(ios::beg);
	dassert(!tclout.bad());

	switch(which) {
	case c_chmod:
		DBG(<<"chmod");
		if(isdigit(*av[1])) {
			mode = (mode_t) _atoi(av[1]);
		} else {
			tclout 
			<< "Sorry -- this shell's chmod recognizes only octal modes." 
			<< ends;
			Tcl_AppendResult(ip, tclout.str(), 0);
			// fake it
			Vas->status.vasreason = SVAS_BadParam1;
			Vas->status.vasresult = SVAS_FAILURE;
		}
		DBG(<<"mode=" << ::oct((unsigned int)mode));
		path = av[2];
		DBG(<<"path=" << path);
		CALL(chMod(path,mode));
		break;

	case c_chown:
		DBG(<<"chown");
		path = av[2];
		DBG(<<"path=" << path);

		if(isdigit(*av[1])) {
			user = (uid_t) _atoi(av[1]);
		} else {
			user = uname2uid(av[1]);
		}
		DBG(<<"user=" << user);

		CALL(chOwn(path,user));
		break;

	case c_chgrp:
		DBG(<<"chgrp");
		path = av[2];
		DBG(<<"path=" << path);

		if(isdigit(*av[1])) {
			group = (gid_t) _atoi(av[1]);
		} else {
			group = gname2gid(av[1]);
		}
		DBG(<<"group=" << group);
		CALL(chGrp(path,group));
		break;
	}

	if(res == SVAS_OK) {
		switch(which) {
		case c_chown:
			tclout << ::dec(user) ;
			break;
		case c_chgrp:
			tclout << ::dec(group) ;
			break;
		case c_chmod:
			tclout << form("0%o",mode) ;
			break;
		default:
			tclout << "internal error at line " << __LINE__;
		}
		tclout << ends;
		dassert(!tclout.bad());
		Tcl_AppendResult(ip, tclout.str(), 0);
	}
	VASERR(res);
}
int 
cmd_chgrp(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_chgrp);
	return permcmd(clientdata, c_chgrp, ip, ac, av);
}
int 
cmd_chown(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_chown);
	return permcmd(clientdata, c_chown, ip, ac, av);
}
int 
cmd_chmod(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_chmod);
	return permcmd(clientdata, c_chmod, ip, ac, av);
}


int 
cmd_sutimes(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	// "sutimes <oid|path> ",
	CMDFUNC(cmd_sutimes);
	lrid_t		target;

	CHECK(1, 1, NULL);
	CHECKCONNECTED;

	if(ac>1) {
		// get oid or path
		if( ! PATH_OR_OID_2_OID(ip, av[1], target, 0, TRUE) ) {
			VASERR(Vas->status.vasresult);
		}
		timeval *at, *mt;
		at = mt = &Yesterday();
		CALL(utimes(target, at, mt)); // set values to "yesterday"
		Tcl_AppendResult(ip, When((time_t)mt->tv_sec), 0);
	} else {
		// error
		SYNTAXERROR(ip, " in command ", _fname_debug_);
	}

	VASERR(res);
}

int 
cmd_utimes(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	// "utimes <oid|path> ",
	CMDFUNC(cmd_utimes);
	lrid_t		target;

	CHECK(2, 2, NULL);
	CHECKCONNECTED;

	if(ac>1) {
		// get oid or path
		if( ! PATH_OR_OID_2_OID(ip, av[1], target, 0, TRUE) ) {
			VASERR(Vas->status.vasresult);
		}
		CALL(utimes(target, 0, 0)); // set values to "now"
		Tcl_AppendResult(ip, Now(), 0);
	} else {
		// error
		SYNTAXERROR(ip, " in command ", _fname_debug_);
	}

	VASERR(res);
}

static int
readObj(ClientData clientdata,
	lrid_t *target, vec_t *data, ObjectOffset start,
	ObjectSize len, char *buf)
{
	CMDFUNC(readObj)
    lrid_t	snapped;
    ObjectSize  more = len, left2read = len, increment = 0;
    char		*bufoff;
    smsize_t	minibuflen;

    snapped = *target;
	while ( ((unsigned int)left2read) >0 ) {
		minibuflen = data->size();
		bufoff = buf;

		while( ((unsigned int)more) >0 && 
			((unsigned int)left2read) > 0 &&
			minibuflen > 0) {
			DBG(<<"pseudo-readObj more= " << more
				<< " left2read "  << left2read
			);
			vec_t data1(bufoff, minibuflen);
			// vec_t data1(bufoff, left2read==WholeObject?BS:left2read);
			more = 0;
			dassert(data1.size() <= data->size());
			dassert(data1.size() == minibuflen);
			DBG(
			<< "SHELL CALLING readObj start=" << start 
			<< " #bytes (left2read) =" << left2read
			<< " into buf+" << (bufoff-buf)
			<< " whose size is " << data1.size()
			);
			CALL(readObj(*target, start, left2read, NL,
				data1, &increment, &more, &snapped));

			dassert(increment <= minibuflen);

			DBG(
				<< "readObj RETURNED TO SHELL: bytes read ="
			<< increment
				<< " more = " << more
			)
			if(res != SVAS_OK) {
			DBG(<< "error in readObj" );
			break;
			}
			start += increment; 
			dassert((int)increment <= data1.size());
			bufoff += increment;
			dassert((int)(bufoff-buf) <= data->size());
			minibuflen -= increment;
			left2read = more;
		} // inner while

		if(res == SVAS_OK) {
			dassert((int)(bufoff-buf) <= data->size());
			if(verbose) {
				display(tclout, snapped, buf, (bufoff-buf), more);
				dassert(!tclout.bad());
			}
		} else {
			break;
		}
	}   // outer while
    return res;
}

int 
cmd_read(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
    CMDFUNC(cmd_read);
    lrid_t	    target;
    char	    *buf;
    ObjectOffset    start;
    ObjectSize	    len;

    // read oid/name [start [len]]

    CHECK(2, 4, NULL);
    CHECKCONNECTED;

    start = 0;
    len = WholeObject;

    if(ac>2) {
		int s;
		s = _atoi(av[2]);

		if(s<0) {
			Tcl_AppendResult(ip, "Argument 2 must be an integer >=0\n", 0);
			(void) append_usage_cmdi(ip, __cmd_read__);
			CMDREPLY(tcl_error);
		} else  {
			start = s;
		}

		if(ac>3) {
			int l;
			l = _atoi(av[3]);
			if(strcmp(av[3],"end")==0) {
				len = WholeObject;
			} else if (l < -1) {
				Tcl_AppendResult(ip,
				"Argument 3 must be an integer >= 0.",0);
				(void) append_usage_cmdi(ip, __cmd_read__);
				CMDREPLY(tcl_error);
			} else {
				len = l<0?WholeObject:l;
			}
		} 
	}

	if( ! PATH_OR_OID_2_OID(ip, av[1], target, 0, TRUE) ) {
		res = Vas->status.vasresult;
		DBG(<<"res=" << res << ", reason=" << Vas->status.vasreason);
		VASERR(res);
	}

/*NEW*/
    if(len == WholeObject) {
		buf = new char[BS];
    } else {
		buf = new char[ len ];
    }

    vec_t data(buf, len==WholeObject?BS:len);

	tclout.seekp(ios::beg);
	dassert(!tclout.bad());
	res = readObj(clientdata, &target, &data, start, len, buf);		
	tclout << ends;
	dassert(!tclout.bad());
	Tcl_AppendResult(ip, tclout.str(), 0);

/*DEL*/
    delete [] buf;
    VASERR(res);
}

int 
cmd_write(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
    CMDFUNC(cmd_write);
    lrid_t	    target;
    lrid_t	    snapped;
    char	    *buf;
    ObjectOffset    start;
    ObjectSize	    len;

    // write oid/name [start [len]]

    CHECK(2, 4, NULL);
    CHECKCONNECTED;

    start = 0;
    len = WholeObject;

	if(ac>2) {
		int s;
		s = _atoi(av[2]);
		if(s<0) {
			Tcl_AppendResult(ip, "Argument 2 must be an integer >=0\n",0);
			(void) append_usage_cmdi(ip, __cmd_write__);
			CMDREPLY(tcl_error);
		} else 
			start = s;

		if(ac>3) {
			int l;
			l = _atoi(av[3]);
			if(strcmp(av[3],"end")==0) {
				len = WholeObject;
			} else if (l < -1) {
				Tcl_AppendResult(ip, "Argument 2 must be an integer >=0\n",0);
				(void) append_usage_cmdi(ip, __cmd_write__);
				CMDREPLY(tcl_error);
			} else {
				len = l<0?WholeObject:l;
			}
		} 
	} 

	if( ! PATH_OR_OID_2_OID(ip, av[1], target, 0, TRUE) ) {
		VASERR(Vas->status.vasresult);
	}

    if(len == WholeObject) {
	    SysProps sysp;
	    // can't write "to-the-end" so we have to figure out
	    // what its size is
	    CALL(sysprops(target, &sysp));
	    if(res != SVAS_OK) {
	    	Tcl_AppendResult(ip, "Could not stat object.",0);
			CMDREPLY(tcl_error);
	    }
	    len = sysp.csize + sysp.hsize;
	    // try it even if len == 0
	    if(len == 0) {
			DBG( << "Warning- object has size 0" );
	    }
/*NEW*/
		buf = new char[BS];
    } else {
		buf = new char[ len ];
	}

    vec_t data(buf, len==WholeObject?BS:len);

	fill(target, buf, len);
	CALL(writeObj(target, start, data));

/*DEL*/
    delete [] buf;
    VASERR(res);
}

#ifdef ZZZZ

#include "zvec_t.h"

int 
cmd_zwrite(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
    CMDFUNC(cmd_write);
    lrid_t	    target;
    lrid_t	    snapped;
    ObjectOffset    start;
    ObjectSize	    len;

    // write oid/name [start [len]]

    CHECK(2, 4, NULL);
    CHECKCONNECTED;

    start = 0;
    len = WholeObject;

	if(ac>2) {
		int s;
		s = _atoi(av[2]);
		if(s<0) {
			Tcl_AppendResult(ip, "Argument 2 must be an integer >=0\n",0);
			(void) append_usage_cmdi(ip, __cmd_zwrite__);
			CMDREPLY(tcl_error);
		} else 
			start = s;

		if(ac>3) {
			int l;
			l = _atoi(av[3]);
			if(strcmp(av[3],"end")==0) {
				len = WholeObject;
			} else if (l < -1) {
				Tcl_AppendResult(ip, "Argument 2 must be an integer >=0\n",0);
				(void) append_usage_cmdi(ip, __cmd_zwrite__);
				CMDREPLY(tcl_error);
			} else {
				len = l<0?WholeObject:l;
			}
		} 
	} 

	if( ! PATH_OR_OID_2_OID(ip, av[1], target, 0, TRUE) ) {
		VASERR(Vas->status.vasresult);
	}

    if(len == WholeObject) {
	    SysProps sysp;
	    // can't write "to-the-end" so we have to figure out
	    // what its size is
	    CALL(sysprops(target, &sysp));
	    if(res != SVAS_OK) {
	    	Tcl_AppendResult(ip, "Could not stat object.",0);
			CMDREPLY(tcl_error);
	    }
	    len = sysp.csize + sysp.hsize;
	    // try it even if len == 0
	    if(len == 0) {
			DBG( << "Warning- object has size 0" );
	    }
	}

    zvec_t data(len==WholeObject?BS:len);
	CALL(writeObj(target, start, data));

/*DEL*/
    VASERR(res);
}
#endif /*ZZZZ*/

int 
cmd_mv(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
    CMDFUNC(cmd_mv);
    // mv p1 p2

    CHECK(3, 3, NULL);
    CHECKCONNECTED;

	CALL(reName(av[1], av[2]));
    VASERR(res);
}

int 
cmd_trunc(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_trunc);
	lrid_t		target;
	ObjectSize	len;

	// trunc oid ultimate-len [newtstart]

	CHECK(3, 4, NULL);
	CHECKCONNECTED;

	int l = _atoi(av[2]);
	len = l<0?WholeObject:l;

	// TODO: test ALL 3 interfaces to each func

	if(len==WholeObject) {
		Tcl_AppendResult(ip, 
			"Argument 2 must be an integer >=0\n", 0);
		(void) append_usage_cmdi(ip, __cmd_trunc__);
		CMDREPLY(tcl_error);
	}
	if( ! PATH_OR_OID_2_OID(ip, av[1], target, 0, TRUE) ) {
		VASERR(Vas->status.vasresult);
	}

	if (ac == 3)  {
	    CALL(truncObj(target, len));
	} else if (ac == 4)  {
	    ObjectSize newtstart = _atoi(av[3]);
	    CALL(truncObj(target, len, newtstart));
	} else	{ // ac == 5
	    ObjectSize newtstart = _atoi(av[3]);
	    CALL(truncObj(target, len, newtstart));
	}

#ifdef USE_VERIFY
	CHECK_TRANS_STATE;

	tclout.seekp(ios::beg);
	dassert(!tclout.bad());
	tclout << target << ends;
	v->truncate(tclout.str(), len);
#endif

	VASERR(res);
}

int 
cmd_append(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_append);
	lrid_t		target;
	char		*buf;
	ObjectSize	len;

	// append oid bytes [newtstart]

	CHECK(3, 4, NULL);
	CHECKCONNECTED;

	int l = _atoi(av[2]);
	len = l<0?WholeObject:l;

	// TODO: test ALL 3 interfaces to each func

	if(len==WholeObject) {
		Tcl_AppendResult(ip, "Argument 2 must be an integer >=0",0);
		(void) append_usage_cmdi(ip, __cmd_append__);
		CMDREPLY(tcl_error);
	}
	if( ! PATH_OR_OID_2_OID(ip, av[1], target, 0, TRUE) ) {
		VASERR(Vas->status.vasresult);
	}

	/*NEW*/
	buf = new char[ len ];
	vec_t	data(buf, len);
	fill(target, buf, len);

	if (ac == 3)  {
	    CALL(appendObj(target, data));
	} else if (ac == 4)  {
	    ObjectSize newtstart = _atoi(av[3]);
	    CALL(appendObj(target, data, newtstart));
	} else	{ // ac == 5
	    ObjectSize newtstart = _atoi(av[3]);
	    CALL(appendObj(target, data, newtstart));
	}

#ifdef USE_VERIFY
		CHECK_TRANS_STATE;
		tclout.seekp(ios::beg);
		dassert(!tclout.bad());
		tclout << target << ends;
		dassert(!tclout.bad());
		v->append(tclout.str(), buf, len);
#endif

	/*DEL*/
		delete [] buf;
	VASERR(res);
}

#ifdef ZZZZ
int 
cmd_zappend(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_append);
	lrid_t		target;
	ObjectSize	len;

	// append oid bytes [newtstart]

	CHECK(3, 4, NULL);
	CHECKCONNECTED;

	int l = _atoi(av[2]);
	len = l<0?WholeObject:l;

	// TODO: test ALL 3 interfaces to each func

	if(len==WholeObject) {
		Tcl_AppendResult(ip, "Argument 2 must be an integer >=0",0);
		(void) append_usage_cmdi(ip, __cmd_zappend__);
		CMDREPLY(tcl_error);
	}
	if( ! PATH_OR_OID_2_OID(ip, av[1], target, 0, TRUE) ) {
		VASERR(Vas->status.vasresult);
	}
	zvec_t	data(len);

	if (ac == 3)  {
	    CALL(appendObj(target, data));
	} else if (ac == 4)  {
	    ObjectSize newtstart = _atoi(av[3]);
	    CALL(appendObj(target, data, newtstart));
	} else	{ // ac == 5
	    ObjectSize newtstart = _atoi(av[3]);
	    CALL(appendObj(target, data, newtstart));
	}

	VASERR(res);
}
#endif /*ZZZZ*/

enum		index_command {i_insert,i_remove,i_removeall,i_find};

static int 
indexcmd(ClientData clientdata, 
	index_command which, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(indexcmds);
	char		*buf;
	bool		malloced;
	int			len;
	int 		number = 0;
	bool		found= FALSE;
	bool		isReg;

	// insert 		name/oid,int key val
	// remove 		name/oid,int key val
	// removeall	name/oid,int key 
	// find 		name/oid,int key 
	CHECK(3,4, NULL);
	CHECKCONNECTED;

	IndexId		target;
	if(!INDEXID(ip, av[1], target)) {
		SYNTAXERROR(ip, " in command ", _fname_debug_);
	}

	vec_t   key(av[2],strlen(av[2]));
	vec_t	value;
	
	if(ac>3) {
		if(which == i_insert || which==i_remove) {
			// ok
		}else {
			Tcl_AppendResult(ip, "Extra argument.", 0);
			(void) append_usage_cmdi(ip, 
				which == i_removeall? __cmd_removeall__:
				__cmd_find__
			);
			CMDREPLY(tcl_error);
		}
		buf = av[3];
		malloced=FALSE;
		len = strlen(buf);
	} else {
		if(which == i_removeall || which==i_find) {
			// ok
		} else  {
			Tcl_AppendResult(ip, "Two arguments required: key and value",0);
			(void) append_usage_cmdi(ip, 
				which == i_insert? __cmd_insert__: __cmd_remove__
			);
			CMDREPLY(tcl_error);
		}
		len = 4048; // maximum elt size
/*NEW*/ buf = new char[len];
		malloced=TRUE;
	}
	value.put(buf, len);

	tclout.seekp(ios::beg);
	dassert(!tclout.bad());

	switch (which) {
	case i_insert:

		{
			CALL(insertIndexElem(target, key, value));
		}
		break;
	case i_remove:
		{
			CALL(removeIndexElem(target, key, value));
		}
		break;
	case i_removeall: 
		{
			CALL(removeIndexElem(target, key, &number));
		}
		if(res == SVAS_OK) {
			if(verbose) {
				tclout << number << " elements removed " << ends;
			} else {
				tclout << number << ends;
			}
		}
		break;
	case i_find: 
		{
			ObjectSize l = len;
			{
				CALL(findIndexElem(target, key, value, &l, &found));
			}
			if(res == SVAS_OK) {
				if(found) {
					buf[l]='\0';
	
					if(verbose) {
						tclout << "found value= " << buf << ends;
					} else {
						tclout << buf << ends;
					}

				} else {
					tclout << "not found" << ends;
				}
			}
			break;
		}
	default:
		assert(0);
	}
	if(res!=SVAS_OK) {
		Tcl_ResetResult(ip);
	} else {
		Tcl_AppendResult(ip, tclout.str(),  0);
	}

/*DEL*/	
	if(malloced) delete [] buf;
	VASERR(res);
}

int 
cmd_removeall(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_removeall);
	return indexcmd(clientdata, i_removeall, ip, ac, av);
}
int 
cmd_remove(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_remove);
	return indexcmd(clientdata, i_remove, ip, ac, av);
}
int 
cmd_insert(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_insert);
	return indexcmd(clientdata, i_insert, ip, ac, av);
}
int 
cmd_find(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_find);
	return indexcmd(clientdata, i_find, ip, ac, av);
}

int 
cmd_scanpool(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_scanpool);
	Cookie 		cookie = NoSuchCookie;
	bool		isReg;
	lrid_t		target;
	bool		eof = FALSE; 
	bool 		do_sysprops = FALSE;
	SysProps 	sysp, *sp=0;
	int			do_read = 0, nobjs;
	lrid_t		loid;
	ObjectSize		used, more;
	static char	buf[4048]; // maximum value size
	vec_t		data(buf, sizeof(buf));

	// scanpool name/oid  [scandata|readdata|count|remove] [sysprops]

	tclout.seekp(ios::beg);
	dassert(!tclout.bad());

	CHECK(2, 4, NULL);
	CHECKCONNECTED;
	if( !PATH_OR_OID_2_OID(ip, av[1], target, &isReg, TRUE)) {
		VASERR(Vas->status.vasresult);
	}
	if(ac>3) {
		if(strcasecmp(av[3],"sysprops")==0) {
			do_sysprops = TRUE;
			sp = &sysp;
		} else {
			Tcl_AppendResult(ip, "unknown option for scan: ", av[3]);
			// fake it
			Vas->status.vasreason = SVAS_BadParam3;
			res = Vas->status.vasresult = SVAS_FAILURE;
			VASERR(res);
		}
	}
	if(do_sysprops) {
		DBG(<<"Getting sysprops while we're at it");
	}
	if(ac>2) {
		do_read = 0; // 0 means count only 
		if(strcasecmp(av[2],"scandata")==0) {
			do_read = 1;
		} else if(strcasecmp(av[2],"data")==0) {
			do_read = 1;
		} else if(strcasecmp(av[2],"readdata")==0) {
			do_sysprops = FALSE;
			do_read = 2;
		} else if(strcasecmp(av[2],"read")==0) {
			do_sysprops = FALSE;
			do_read = 2;
		} else if(strcasecmp(av[2],"remove")==0) {
			do_read = -1;
		}
	}
	DBG(<<"before open scan; first cookie =" << (int)cookie);
	// open scan
	if(isReg) {
		CALL(openPoolScan(av[1], &cookie));
	} else{
		CALL(openPoolScan(target, &cookie));
	}
	if(res != SVAS_OK) {
		VASERR(res);
	}
	DBG(<<"scan opened; first cookie =" << (int)cookie);
	nobjs=0;
	while(eof == FALSE) {
		dassert(!tclout.bad());
		DBG(<<"cookie =" << (int)cookie);
		switch(do_read) {

		case 2:	 // do explicit read of the object
			CALL(nextPoolScan(&cookie, &eof, &loid));
			DBG(<<"loid =" << loid << " eof=" << eof);
			if((res==SVAS_OK) && !eof) {
				CALL(readObj(loid, 
					0, WholeObject, 
					NL, data, 
					&used, &more));
				if(more && verbose) {
					tclout << "WARNING---skipping data for loid " << loid <<
					endl ;
				}
			}
			break;

		case 1: // read with the scan function
			CALL(nextPoolScan(&cookie, &eof, &loid,
				0, WholeObject, data, &used, &more, NL, sp));
			DBG(<<"loid =" << loid << " eof=" << eof);
			if(more && verbose) {
				tclout << "-skipped data for loid " << loid << endl ;
			}
			break;

		case 0: // don't read the data at all
			do_sysprops = FALSE;
			CALL(nextPoolScan(&cookie, &eof, &loid));
			DBG(<<"loid =" << loid << " eof=" << eof);
			break;

		case -1: 
			 // DESTROY the data
			CALL(nextPoolScan(&cookie, &eof, &loid));
			DBG(<<"loid =" << loid << " eof=" << eof);
			if((res==SVAS_OK) && !eof) {
				lrid_t pooloid;
				CALL(rmAnonymous(loid, &pooloid));
				dassert(target == lrid_t::null
					|| target == pooloid || eof);
			}
			break;
		}
		if(res == SVAS_OK ) {
			if(eof) {
				if(verbose) {
					tclout << "-end-of-file-" << endl;
				}
			} else {
				nobjs++;
				if(verbose) {
					tclout << loid;
					dassert(!tclout.bad());
					if(do_read>1) {
						tclout << " [" << used << "] " << buf << ends;
					} 
					tclout << endl;
				}
				if(do_sysprops) {
					dassert(sp);
				} else {
					if(sp!=0) {
						SYNTAXERROR(ip, " in command ", _fname_debug_);
					}
				}
				if(do_sysprops) {
					dassert(sp->tag == KindAnonymous);
					tclout 	
						<< loid.serial
						<< " \t" << 
						typc(sp->type,sp->tstart)
						<< " \t" << 
						sp->csize << "+" 
						<< sp->hsize << " ";
						tclout << loid ;
						ptextinfo(tclout, *sp) ;
					tclout	<< endl;
				}
			}
		}
		if(do_read != 0) {
			Tcl_AppendResult(ip, tclout.str(),  0);
			tclout.seekp(ios::beg);
			dassert(!tclout.bad());
		}
		DBG(<<" eof = " << eof);
	}
	CALL(closePoolScan(cookie));
	tclout << nobjs << ends;
	dassert(!tclout.bad());
	Tcl_AppendResult(ip, tclout.str(),  0);
	VASERR(res);
}

#define IS_EQ_OP(i) (i & 0x1)
#define IS_EQONLY_OP(i) ((((int)i)&0x7)==0x1)
#define IS_LW_OP(i) (i & 0x2)
#define IS_UP_OP(i) (i & 0x4)

int 
cmd_scanindex(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_scanindex);
	Cookie		cookie = NoSuchCookie;
	CompareOp	lc, uc, cop;
	char		keybuf[100];
	char		valuebuf[200];
	ObjectSize			keylen = sizeof(keybuf);
	ObjectSize 		valuelen = sizeof(valuebuf);
	vec_t		key(keybuf, sizeof(keybuf));
	vec_t		value(valuebuf, sizeof(valuebuf));
	bool		eof;
	int			entries=0;

	// scanindex name/oid,i  [cmp lbound] [cmp hbound]
	tclout.seekp(ios::beg);
	dassert(!tclout.bad());

	CHECK(2, 6, NULL);
	CHECKCONNECTED;

	int i=0; // index into argv

	IndexId		target;
	if(!INDEXID(ip, av[++i], target)) {
		SYNTAXERROR(ip, " in command ", _fname_debug_);
	}
	vec_t		lbound, *lb=0;
	vec_t		ubound, *ub=0;

	// default arguments

	while(++i < ac) {
		vec_t		*_bound=0, **_b=0;
		char		*_str=0;
		
		GET_CMPOP(av[i], cop);

		if(IS_LW_OP(cop)) {
			_bound = &lbound;
			_b = &lb;
			lc = cop;
			_str = "lower";
		} else if(IS_UP_OP(cop)) {
			_bound = &ubound;
			_b = &ub;
			uc = cop;
			_str = "upper";
		} else if (IS_EQ_OP(cop)) {
			_bound = &ubound;
			_b = &ub;
			uc = cop;
			_str = "upper";
		} else {
			assert(0);
		}
		DBG(<< av[i] << " will be " << _str << " compare op" );

		if(++i < ac) {
			DBG(<< av[i] << " will be " << _str  << " value" );
			if(*_b) {
				Tcl_AppendResult(ip, av[i], " : replacing previous ",
					_str, " bound! ",0);
			}
			_bound->put(av[i], strlen(av[i]));
			*_b = & (*_bound);

			dassert(
				(!lb && (ub == &ubound))
				||
				(!ub && (lb == &lbound))
				||
				(ub && lb && (lb == &lbound) && (ub == &ubound)) );

		} else {
			Tcl_AppendResult(ip, 
				"Comparison operator is missing an operand!", 0);
			SYNTAXERROR(ip, " in command ", _fname_debug_);
		}
	} 

	// use defaults if no arguments given
	if(!lb) {
		if ((ub != 0) && IS_EQONLY_OP(ub)) {
			// if they seem to want ==, make both args be the same
			lb = ub;
			lc = uc;
		} else {
			GET_CMPOP(">=", lc);
			DBG(<<"lower bound is " << lc << "-inf");
			lb = &vec_t::neg_inf;
		}
	}
	if(!ub) {
		GET_CMPOP("<=", uc);
		ub = &vec_t::pos_inf;
		DBG(<<"upper bound is " << lc << "+inf");
	}

	DBG(<<"openIndexScan("
		<< lc << " |"
		<< (char *)(lb->is_neg_inf()?"-inf":lb->is_pos_inf()?"+inf":lb->ptr(0)) 
		<< "| "
		<< " && "
		<< uc << " |"
		<< (char *)(ub->is_neg_inf()?"-inf":ub->is_pos_inf()?"+inf":ub->ptr(0)) 
		<< "|)"
	);

	CALL(openIndexScan(target, lc, *lb, uc, *ub, &cookie));
	if(res != SVAS_OK) {
		VASERR(res);
	}
	eof = FALSE;
	while(!eof) {
		keylen = sizeof(keybuf);
		valuelen = sizeof(valuebuf);
		CALL(nextIndexScan(&cookie,
				key, &keylen, value, &valuelen, &eof));
		if(res!=SVAS_OK) {
			break;
		}
		if(!eof) {
			entries++;
			if(verbose) {
				keybuf[keylen]='\0';
				valuebuf[valuelen]='\0';

				// tclout << "key= " << keybuf << ", value=" << valuebuf << ends;
				// Tcl_AppendElement(ip, tclout.str());
				// tclout.seekp(ios::beg);
				// dassert(!tclout.bad());
				char *_argv[2] = { keybuf, valuebuf };
				char *temp = Tcl_Merge(2, _argv);
				if(temp) {
					Tcl_AppendElement(ip, temp);
					free(temp);
				} else {
					Tcl_AppendElement(ip, "Error in Tcl_Merge");
				}
			}
		}
	}
	CALL(closeIndexScan( cookie ));
	if(!verbose) {
		tclout << entries << ends;
		Tcl_AppendResult(ip, tclout.str(),  0);
	}
	VASERR(res);
}

int 
cmd_checkserver(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	if(Vas && Vas->connected()) {
		Tcl_AppendResult(ip, "0",0);
	} else {
		Tcl_AppendResult(ip, "1",0);
	} 
	return TCL_OK;
}
