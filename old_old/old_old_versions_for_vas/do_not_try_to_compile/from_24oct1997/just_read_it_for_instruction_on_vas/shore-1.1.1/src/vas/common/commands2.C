/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/commands2.C,v 1.47 1996/04/09 20:51:42 nhall Exp $
 */
#include <copyright.h>
#include <iomanip.h>
#include "shell.misc.h"
#include "vasshell.h"
#include "server_stubs.h"

#include    "sm_du_stats.h"

// this stuff is for du/df
enum du_flags
{
	du_none =	0x00,
	du_s	=	0x01,	// summary: don't print for each subdir
	du_a	=	0x02,	// unix du -a: generate entry for each file
	du_sa	=	0x04,	// unix du generate for directories only
	du_o	=   0x08,	// overhead
	du_m	=   0x10,	// SM overhead in detail
	du_b	=   0x20,	// brief (one number)
};

enum df_flags
{
	df_none = 	0x00,
	df_e	=   0x01,	// extents
	df_p	=   0x02,	// pages
	df_b	=   0x04,	// bytes
	df_s	=   0x08, // summary (brief)
	df_a	=   0x07  // all (not brief)
};

class du_args 
{
public : 
	Path			path;  // path name
	lrid_t			oid;   // object id
	du_flags 		flag;  // du flags
	Tcl_Interp*		ip;    // tcl interp

private:
	int				b_sysprops_overhead;  	// (all objects)
	int				b_hdr;  				// (all objects -- should match b_sysprops_overhead)
	int				b_filesystem_overhead;	// overhead for core + heap
											// (sysprops being already counted)
	int 			b_sm_overhead;	    	// bytes USED by sm
	int 			b_unused;	    		// byes UNUSED by sm
	int				b_user_data;        	// user data total (lg + small)
	int				b_body;        			// user data from SM perspective-- should match	
											// b_user_data

	struct sm_du_stats_t  du;               // stats delivered by sm

public:
	du_args(Path p,du_flags f,Tcl_Interp* ip)  // constructor
			: path(p),flag(f), ip(ip),
				oid(lrid_t::null),
				b_sysprops_overhead(0),
			  b_filesystem_overhead(0), b_sm_overhead(0), b_user_data(0)
	{
		memset(&du,0,sizeof(struct sm_du_stats_t));
	} 
	void compute() {
		b_unused = (int) ( 
					du.file.file_pg.free_bs 
				+	du.file.lgdata_pg.unused_bs
				+  	du.file.file_pg.slots_unused_bs
			);
		b_body = (int)  (
					du.file.file_pg.rec_body_bs 
				+	du.file.lgdata_pg.data_bs
			);
		b_hdr = (int)  (
				du.file.file_pg.rec_hdr_bs 
			);
		// don't count unused bytes as "overhead"
		b_sm_overhead = (int)  (
			( 		/* file pages */
					du.file.file_pg.hdr_bs
				+  	du.file.file_pg.slots_used_bs 
				+  	du.file.file_pg.rec_tag_bs
				// 	rec_hdr_bs is counted as sysprops
				+  	du.file.file_pg.rec_hdr_align_bs
				// 	rec_body_bs is counted as user data
				+  	du.file.file_pg.rec_body_align_bs
				+  	du.file.file_pg.rec_lg_chunk_bs
				+  	du.file.file_pg.rec_lg_indirect_bs 
			)
			+
			( 		/* large object pages */
					du.file.lgdata_pg.hdr_bs
			)
		// TODO: have to figure out *which* indexes to count
		// as SM overhead and which to count as user data!!
			+
			( 		/* large index pages -- nothing*/
					du.file.lgindex_pg.used_bs
				+	du.file.lgindex_pg.unused_bs
			)
			+
			( 		/* btree index pages */
					du.btree.leaf_pg.hdr_bs
				+	du.btree.leaf_pg.key_bs
				+	du.btree.leaf_pg.data_bs
				+	du.btree.leaf_pg.entry_overhead_bs
				+	du.btree.leaf_pg.unused_bs

				+	du.btree.int_pg.used_bs
				+	du.btree.int_pg.unused_bs
				+   du.btree.unlink_pg_cnt*sm_page_size

			 		/* rtree index pages */
				+	du.rtree.leaf_pg_cnt*sm_page_size
				+	du.rtree.int_pg_cnt*sm_page_size
				+	du.rtree.unalloc_pg_cnt*sm_page_size

			 		/* rdtree index pages */
				+	du.rdtree.leaf_pg_cnt*sm_page_size
				+	du.rdtree.int_pg_cnt*sm_page_size
				+	du.rdtree.unalloc_pg_cnt*sm_page_size

			)
			)
			;
	}
	void	addsysp(int s) { b_sysprops_overhead += s; }
	void	adduser(int s) { b_user_data +=  s; }
	void	addfs(int s) { b_filesystem_overhead +=  s; }
	friend ostream & operator <<(ostream &out, const du_args dua) ;
	friend void	du_print(Tcl_Interp *, ostream &, const du_args&, 
		const sm_du_stats_t *dfi);

	du_args &operator+=(const du_args b) {

		b_sysprops_overhead += b.b_sysprops_overhead;
		b_filesystem_overhead += b.b_filesystem_overhead;
		b_sm_overhead += b.b_sm_overhead;
		b_user_data += b.b_user_data;

		this->du.add(b.du);
		return *this;
	}

	VASResult diskusage(bool mbroot, svas_base *v) {
		VASResult res;
		du.clear();
		res = v->disk_usage(oid, mbroot, &du);
		if(res == SVAS_OK) {
			compute();
		} else {
			cerr << "UNEXPECTED ERR in disk_usage" 
			<< v->status.vasreason
			<< endl;
		}
		return res;
	}
};

void	du_print(Tcl_Interp *, ostream &, const du_args&, 
	const sm_du_stats_t *dfi=0);

extern char *news;

char *news = "\n\
";


int
cmd_news(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_bugs);
	Tcl_AppendResult(ip, news, 0);
	return TCL_OK;
}

int
cmd_locktable(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	_dumplocks(ip);
	return TCL_OK;
}

int
cmd_threads(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	_dumpthreads(ip);
	return TCL_OK;
}
int 
cmd_config(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_config);
	CHECK(1,1,NULL);
	tclout.seekp(ios::beg);
	dassert(!tclout.bad());
	pconfig(tclout);
	dassert(!tclout.bad());
	Tcl_AppendResult(ip, tclout.str(), 0);
	return TCL_OK;
}

int 
cmd_statistics(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_stats);
	int 	mode = statistics_mode;

	bool	action = FALSE;

	CHECK(1,10,NULL);

	if(ac==1) {
		// print flags
		if(mode & s_autoprint) {
			Tcl_AppendResult(ip, "autoprint ", 0);
		} else {
			Tcl_AppendResult(ip, "noautoprint ", 0);
		}

		if(mode & s_autoclear) {
			Tcl_AppendResult(ip, "autoclear ", 0);
		} else {
			Tcl_AppendResult(ip, "noautoclear ", 0);
		}

		if(mode & s_remote) {
			Tcl_AppendResult(ip, "remote ", 0);
		} else {
			Tcl_AppendResult(ip, "noremote ", 0);
		} 

		return TCL_OK;
	}

	int i=0;
	while(++i < ac) {
		if(strcmp(av[i],"autoprint")==0) {
			mode |= s_autoprint;
		} else if(strcmp(av[i],"noautoprint")==0) {
			mode &= ~s_autoprint;
		} 

		else if(strcmp(av[i],"autoclear")==0) {
			mode |= s_autoclear;
		} else if(strcmp(av[i],"noautoclear")==0) {
			mode &= ~s_autoclear;
		} 

		else if(strcmp(av[i],"remote")==0) {
			mode |= s_remote;
		} else if(strcmp(av[i],"noremote")==0) {
			mode &= ~s_remote;
		} 

		else{
			Tcl_AppendResult(ip, "Unknown statistics command" , 0);
		}

	}
	statistics_mode = mode;

	return TCL_OK;
}

int
cmd_clients(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_clients);
	bool vb=FALSE;
	CHECK(1,2,NULL);
	if(ac > 1) {
		if(strcmp(av[1],"verbose")==0) {
			vb = TRUE;
		}
	}
	pclients(ip, vb);
	CMDREPLY(tcl_ok);
}

int
cmd_disconnect(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_disconnect);
	CHECK(1,1,NULL);
	if(Vas == NULL) {
		Tcl_AppendResult(ip, "Not connected.", 0);
		CMDREPLY(tcl_error);
	}
	delete Vas;

	((interpreter_t *)clientdata)->_vas =0; // Vas = NULL;

	Tcl_AppendResult(ip, "Disconnected.", 0);
    CMDREPLY(tcl_ok);
}

int
cmd_vas(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_vas);
	char		*str = 0;
	int			nsmbytes=-1, nlgbytes=-1;

	CHECK(1,4,NULL);

	if(ac>1) {
		str = av[1];
	}
	if(ac>2) {
		nsmbytes = _atoi(av[2]);
		if(nsmbytes < 0) {
usage:
			Tcl_AppendResult(ip, 
			"Usage: vas hostname [#K-sm-bytes [#K-lg-bytes]].", 0);
			CMDREPLY(tcl_error);
		}
	}
	if(ac>3) {
		nlgbytes = _atoi(av[3]);
		if(nlgbytes < 0) {
			goto usage;
		}
	}

	if(Vas != NULL) {
		Tcl_AppendResult(ip, "Already connected.", 0);
		CMDREPLY(tcl_error);
	}

	DBG(<<"smbytes " << nsmbytes << " lgbytes " << nlgbytes);

	// get a new shore value-added server
	svas_base *nv;
	w_rc_t e = new_svas(&nv, str, nsmbytes, nlgbytes);
	if(e) {
		return cmdreply(clientdata, 
			ip,vasreply(clientdata, ip, SVAS_FAILURE, 
			(int)e.err_num()),
			_fname_debug_);
	}
	dassert(nv!=NULL);
	// stash it in the interpreter structure
	((interpreter_t *)clientdata)->_vas = nv;

	sm_page_size = Vas->page_size();
	if(verbose) {
		Tcl_AppendResult(ip, "Connected to ", str, 0);
	}
    CMDREPLY(tcl_ok);
}

int
cmd_getroot(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_getroot);
	lvid_t		volid;
	lrid_t		dir;

	CHECK(1, 1, NULL);

	CALL(getRootDir(&dir));
	if(res==SVAS_OK) {
		Tcl_AppendLoid(ip, res, dir);
	}
	VASERR(res);
}

int
cmd_setroot(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_setroot);
	lvid_t		volid;
	lrid_t		dir;

	CHECK(2, 2, NULL);
	GET_VID(ip, av[1], volid);

	CALL(setRoot(volid, &dir));
	if(res==SVAS_OK) {
		Tcl_AppendLoid(ip, res, dir);
	}
	VASERR(res);
}

int
cmd_pmount(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_pmount);

    // Path 		dev = av[1];
	Path		mntpt=0;
	bool		writable=true;
	lrid_t		dir;

	DBG(<<"cmd_pmount with " << ac << " args "
		<< " last is " << av[ac-1]
	);
	CHECK(2,3,"<volumeid> <mountpoint(path)>");
	if(ac > 2) {
		mntpt = av[2];
		DBG(<<"cmd_pmount with " << ac << " args "
			<< " last is " << av[ac-1]
		);
	}
	if(ac > 1) {
		GET_VID(ip, av[1], dir.lvid);
	} else {
		// should never have got past CHECK
		assert(0);
	}

	CHECKCONNECTED;

	// For now, we only mount local devices.
	if(ac > 2) {
		CALL(mount( dir.lvid, mntpt, true));
		DBG(<<"cmd_pmount returned  " << res);
		// Tcl_AppendLVid(ip, res, dir.lvid);
	} else {
		// list pmounts on this volume
		char 			buf[1000]; // place to put the char strings
								// to which fname list entries will point
		char 			cwdbuf[200]; // 
		char 			junk1[100];
		ostrstream		dirserialstr(junk1, sizeof(junk1));
		char 			junk2[100];
		ostrstream		targetstr(junk2, sizeof(junk2));

		typedef char * 	charptr;
#define		LISTLEN 10
		charptr 		*fnamelist =  new charptr[LISTLEN];
		lvid_t	 		*targetlist = new lvid_t[LISTLEN];
		serial_t	 	*dirlist = new serial_t[LISTLEN];
		int 			count;
		bool			more = true;

		BEGIN_NESTED_TRANSACTION

		while(more) {
			count = LISTLEN;
			CALL( list_mounts(
				dir.lvid,
				buf, sizeof(buf), 
				dirlist, fnamelist, targetlist, &count, &more) );

			if(res == SVAS_OK) {
				char *_argv[3];
				int	 _argc=0;
				char *temp;

				DBG(<<"returned " << count << " entries, more="
					<< more);

				for(int i=0; i<count; i++) {
					DBG(<<"i=" << i << " count=" << count);
					_argc=0;

					// target volume
					targetstr.seekp(ios::beg);
					dassert(!targetstr.bad());
					targetstr << targetlist[i] << ends;
					_argv[_argc] = targetstr.str();
					DBG(<<"target vol=" << _argv[_argc]);
					_argc++;

					dir.serial = dirlist[i]; // for computing path
					if(verbose) {
						// local directory
						dirserialstr.seekp(ios::beg);
						dassert(!dirserialstr.bad());
						dirserialstr << dirlist[i] << ends;
						DBG(<<"mntpt directory =" << dirlist[i]);
						_argv[_argc++] = dirserialstr.str();
					}

					// full pathname of the mount point
					Vas->gwd(cwdbuf, 200, &dir);
					DBG(<<"mntpt path =" << cwdbuf);
					_argv[_argc] = cwdbuf;
					dassert(strchr(_argv[_argc], ' ') == 0); 
					// not just an error code
					dassert(strchr(_argv[_argc], '/') != 0); 
					_argc++;

					// fname of the link
					tclout.seekp(ios::beg);
					DBG(<<"mntpt filename =" << fnamelist[i]);
					tclout << fnamelist[i] << ends;
					_argv[_argc] = tclout.str();
					// still no blanks
					dassert(strchr(_argv[_argc], ' ') == 0); 
					_argc++;

					temp = Tcl_Merge(_argc, _argv);
					if(temp) {
						Tcl_AppendElement(ip, temp);
						free(temp);
					} else {
						Tcl_AppendElement(ip, "Error in Tcl_Merge");
					}
				}
			} else {
				DBG(<<"error in pmount");
				break;
			}
		}
		END_NESTED_TRANSACTION
	}
	NESTEDVASERR(res, reason);
}
int
cmd_mount(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_mount);

	if( ac ==  1) {
		// print the mount table
		print_mount_table();
		res =  SVAS_OK;
		CMDREPLY(tcl_ok);
	} 

	Tcl_AppendResult(ip, "The mount command is obsolete. Use setroot.",0);
	CMDREPLY(tcl_error);

#ifdef notdef
    // Path 		dev = av[1];
	Path		mntpt = av[2];
	lvid_t		volid;
	bool		writable;

	CHECK(3,4,NULL);
    // dev = av[1];
    mntpt = av[2];
	writable = 0;
	if(ac == 4) {
		if(strcasecmp(av[3],"true")==0) {
			writable = 1;
		} else 
		if(strcasecmp(av[3],"writable")==0) {
			writable = 1;
		} else 
		if(strcasecmp(av[3],"ro")==0) {
			writable = 0;
		} else 
		if(strcasecmp(av[3],"readonly")==0) {
			writable = 0;
		} else 
		if(strcasecmp(av[3],"false")==0) {
			writable = 0;
		} else {
			Tcl_AppendResult(ip, "syntax error in cmd_mount",0);
			CMDREPLY(tcl_error);
		}
	}
	DBG(<<"cmd_mount with " << ac << " args "
		<< " last is " << av[ac-1]
	);

	CHECKCONNECTED;

	// For now, we only mount local devices.
	{
		GET_VID(ip, av[1], volid);
		CALL(mount(NULL, 0, volid, mntpt, writable));
	}

	DBG(<<"cmd_mount returned  " << res);
	Tcl_AppendLVid(ip, res, volid);
	VASERR(res);
#endif
}

int
cmd_verify(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
    CMDFUNC(cmd_verify);

#ifdef USE_VERIFY

    lrid_t	target;	
    CHECK(1, 1, NULL);
	tclout.seekp(ios::beg);
	dassert(!tclout.bad());

    BEGIN_NESTED_TRANSACTION

    GDBM_FILE	whichdb = __nested?v->commit_db:v->active_db;

//    gdbm_i      *dbi = new gdbm_i(whichdb);
    gdbm_i      dbi(&whichdb);
    ovt_data_t  *data;
    char        *buf;
    vec_t       *vec;

//    while ( ( data = (ovt_data_t *)(dbi->next()) ) != NULL ) 
    while ( ( data = (ovt_data_t *)(dbi.next()) ) != NULL )  {
        if(! OID_2_OID(ip, data->_oid(), target)) {
			CMDREPLY(tcl_error);
		}
        buf = new char [ data->_len() + 200 ];
        vec = new vec_t (buf, data->_len() + 200);

        memset(buf, '\0', data->_len() + 200);
        res = readObj(&target, vec, 0, WholeObject, buf);
        if (verbose)  {
            tclout << data->_oid() << " >>" << buf << "<<" << endl;
            tclout << "buf length is " << strlen(buf) << endl;
        }
		dassert(!tclout.bad());
        v->verify(data->_oid(), buf, strlen(buf));

        delete [] buf;
        delete vec;
    }

    END_NESTED_TRANSACTION

    Tcl_AppendResult(ip, tclout.str(), 0);

#else
    /* perhaps this could be ignored? */
    Tcl_AppendResult(ip, "Sorry - shell was not compiled with -DUSE_VERIFY.",0);
#endif
    NESTEDVASERR(res, reason);
}

int
cmd_printovt(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
    CMDFUNC(cmd_printovt);
#ifdef USE_VERIFY
    gdbm_i      *dbi;
    ovt_data_t  *data;

    CHECK(2, 2, NULL);

    switch (av[1][0]) {
        case 'c':
            dbi = new gdbm_i(&(v->commit_db));
            break;
        case 'a':
            if (! v->active_db) {
                Tcl_AppendResult(ip, "Active OVT is empty.", 0);
                CMDREPLY(tcl_ok);
            } else {
                dbi = new gdbm_i(&(v->active_db));
            }
            break;
        case 'p':
            Tcl_AppendResult(ip, "Prepared OVT file is not currently supported."
, 0);
            CMDREPLY(tcl_ok);
            break;
        default:
            Tcl_AppendResult(ip, "Unknown option: ", av[1], 0);
            CMDREPLY(tcl_error);
            break;
    }

    while ( ( data = (ovt_data_t *)(dbi->next()) ) != NULL )  {
		tclout.seekp(ios::beg);
		dassert(!tclout.bad());
        tclout << *data << endl << ends;
		dassert(!tclout.bad());
		Tcl_AppendResult(ip, tclout.str(), 0);
    }
#endif
    CMDREPLY(tcl_ok);
}

#ifdef NOTDEF
extern VASResult	du_traverse(ClientData, Tcl_Interp *, du_args&, bool=false);
#endif

extern void			df_print(ostream&o, const struct sm_du_stats_t&, const df_flags);

// The following functions are for du

int 
cmd_du(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	CMDFUNC(cmd_du);

#ifndef NOTDEF
	//TODO: fix & remove
	Tcl_AppendResult(ip, "du is not yet implemented.",0);
	CMDREPLY(tcl_error);
#else

	du_flags	flag = du_none; // default 

	int 		c;
	extern int 	optind;
	extern int 	opterr;

	// disable error message from getopt
	opterr = 0;   
	optind = 1;     

	// parse all input options
	while ( (c=getopt(ac,av,"saomb")) != -1 ) {
		switch (c) {
			case 's':	flag = (du_flags)(flag | du_s); break;
			case 'a':	flag = (du_flags)(flag | du_a); break;
			case 'o':	flag = (du_flags)(flag | du_o); break;
			case 'm':	flag = (du_flags)(flag | du_m); break;
			case 'b':	flag = (du_flags)(flag | du_b); break;
			case '?':	
				SYNTAXERROR(ip, " in command ", _fname_debug_);
				// returns
		}
	}
	if((flag & (du_a|du_s)) == 0) {
		// directories only
		flag = (du_flags) (flag | du_sa); 
	}

	CHECKCONNECTED;
	BEGIN_NESTED_TRANSACTION

	// if no file names are given, default is "."
	if ( ac == optind ) {
		sm_du_stats_t dfi;
		dfi.clear();
		du_args arg(".",flag,ip);
		CALL( disk_usage(arg.oid.lvid, &dfi) );
		if ( res != SVAS_OK ) {
			Tcl_AppendResult(ip, "Error collecting df stats.",0);
			return TCL_ERROR;
		}
		DBG(<<"calling du_traverse");
		res = du_traverse(clientdata, ip, arg, true);

		// print out result
	  	if ( res != SVAS_OK ) {
			Tcl_AppendResult(ip, "Error ",arg.path,0);
		} else {
			if((flag & (du_a|du_sa)) == 0) {
				// du_traverse didn't print for "."
				DBG(<<"Calling du_print");
				du_print(ip, tclout, arg, &dfi);
			}
		}
	} else {
		for ( int i=optind; i < ac; i++) {
			du_args	arg(av[i], flag,ip);
			bool		mayberoot;

			DBG(<<"calling du_traverse");
			res = du_traverse(clientdata, ip, arg, true);
	  		if ( res != SVAS_OK )  {
				Tcl_AppendResult(ip, "Error ",arg.path,0);
			} else {
				if((flag & (du_a|du_sa)) == 0) {
					// du_traverse didn't print for "."
					DBG(<<"Calling du_print");
					du_print(ip, tclout, arg);
				}
			}
		}
	}
	END_NESTED_TRANSACTION
	NESTEDVASERR(res, reason);
#endif
}

ostream &
operator <<(ostream &out, const du_args dua) 
{
	out << "du_args: ";
	out << dua.path << " " << dua.oid << " flags="  << dua.flag;
	out << " sysp_o: " << dua.b_sysprops_overhead ;
	out << " fs_o: " << dua.b_filesystem_overhead ;
	out << " sm_o: " << dua.b_sm_overhead ;
	out << " user_o: " << dua.b_user_data ;
	out << endl;
	return out;
}

#ifdef NOTDEF

// the path name  of the object shud be in args
VASResult 
du_traverse(ClientData clientdata, Tcl_Interp *ip, du_args&  arg, bool mbroot)
{	
	// also calls du_print if du_a or du_sa && this is a dir
	CMDFUNC(du_traverse);
#ifdef notdef
	SysProps	sysprops;
	VASResult	recursive_dir_descent(ClientData , Tcl_Interp *, du_args &);
	VASResult	get_pool_size(ClientData clientdata, du_args& );

	// map path name to oid w/o following link/xrefs
	if( !PATH_OR_OID_2_OID_NOFOLLOW(ip, arg.path, arg.oid, 0, TRUE) ) {
		VASERR(Vas->status.vasresult);
		// returns
	}
	if(arg.oid.serial.is_null()) {
		Tcl_AppendResult(arg.ip, arg.path, "No such file.", 0);
		// fake it
		Vas->status.vasreason = SVAS_NotFound;
		return (Vas->status.vasresult = SVAS_FAILURE);
	}
	DBG(<<"du traverse\n\t" <<  arg);

	// get properties of this object: stat object.
	// this works for both registered & anonymous objects.
	// does not follow links or xrefs : vas.h object.C
	int overhead;
	CALL( sysprops(arg.oid,&sysprops,TRUE,SH,0,&overhead));

	// check error
	if ( res != SVAS_OK) return res;

	DBG(<<"du traverse : sysprops uses  " <<  overhead << "bytes");
	arg.addsysp(overhead);

	switch ( sysprops.tag ) {
		case KindRegistered:
    		if(ReservedSerial::_Directory==sysprops.type)   {
				res =  recursive_dir_descent(clientdata, ip, arg);
				arg.addfs(sysprops.csize + sysprops.hsize); 
				if(mbroot) {
					du_args		rootargs(arg.path, arg.flag, arg.ip); 
					rootargs.oid = arg.oid;

					if(rootargs.diskusage(mbroot,Vas)==SVAS_OK) {
						arg += rootargs;
					}
				}

			} else if(ReservedSerial::_Symlink==sysprops.type) {
				arg.addfs(sysprops.csize + sysprops.hsize); 

			} else if(sysprops.type == ReservedSerial::_Xref) {
				arg.addfs(sysprops.csize + sysprops.hsize); 

			} else if(sysprops.type == ReservedSerial::_UnixFile) {
				arg.adduser(sysprops.csize + sysprops.hsize); 

			} else if(ReservedSerial::_Pool==sysprops.type)     {
				arg.addfs(sysprops.csize + sysprops.hsize); 
				res = get_pool_size(clientdata, arg);

			}
			break;

		case KindAnonymous: 
			// this case should not arise because only registered
			// objects are accessible through the directory tree.
			dassert(0);
			res = SVAS_FAILURE;
			break;
		default: 
			dassert(0);
			res = SVAS_FAILURE;
			break;
	}

	DBG(<<"du traverse\n\t" <<  arg);

	if(	(arg.flag & du_a) || 
		(ReservedSerial::_Directory==sysprops.type) && (arg.flag & du_sa) ){
		DBG(<<"Calling du_print");
		du_print(ip, tclout, arg);
	}
	return res;
#else	
	return SVAS_OK;
#endif notdef
}

void 
du_print(Tcl_Interp *ip, ostream &out, const du_args& arg, 
	const sm_du_stats_t *dfi)
{

#ifdef notdef
#ifdef PERCENT
#undef PERCENT
#endif
#define PERCENT(x,y) x, (float)(100*(x))/y, y

	FUNC(du_print);
	static char *totalformat 	=  "%-25s : %10d\n";

	bool	counted_all=false;

	DBG(<<"du_print flag=" << arg.flag);
	out.seekp(ios::beg);


	int total_overhead = 
		arg.b_sysprops_overhead +  arg.b_filesystem_overhead +
										 arg.b_sm_overhead;

	//
	//  This is predicated on the notion that all the other
	//  SM overhead is in indexes
	//
	if(dfi && dfi->no_file_p == arg.du.u.fi.p_small) {
		counted_all = true;
	} else {
			// must not be root-- didn't calculate pages for 
			// registered file
		out
		<< "Warning: page counts do not include registered objects file."
		<< endl;
	}
	dassert(!tclout.bad());

	static char *summaryformat = "%10d %s";
#define SUMM(i) ::form(summaryformat, i, arg.path) << endl;

	if((arg.flag & (du_o|du_m))==0) {
		// interested in total bytes consumed
		// calculated by du_traverse, not by sm_du_stats_t from file pages

		out << SUMM( arg.b_user_data + total_overhead);
	} 

	dassert(!tclout.bad());
	if(arg.flag & du_o) {
		// interested in details of overhead
		if(arg.flag & du_b) {
			out << SUMM(total_overhead);
		} else {
			static char *percentformat1 =  
								"%-25s : %10d (%02.2f%% of %d)\n";
			static char *percentformat2 = 
								"%-25s : %10d (%02.2f%% of %d, %02.2f%% of %d)\n";

			int total_bytes_used =  arg.b_user_data + total_overhead;

			out 
			<<	arg.path << endl
			<< ::form(totalformat,"Total bytes used",  total_bytes_used)
			;

			if(total_bytes_used > 0) {
				out << ::form(percentformat1, 
					"   User data", PERCENT(arg.b_user_data, total_bytes_used)) 

				<< ::form(percentformat1, 
					"   Overhead", PERCENT(total_overhead, total_bytes_used)) 

#define TPERCENT(x,y) (float)(100*(x))/y, y
#define TWOPERCENTS(_a_)\
	_a_, TPERCENT(_a_,total_overhead), TPERCENT(_a_,total_bytes_used)

				<< ::form(percentformat2,
					"      -sysprops",
					TWOPERCENTS(arg.b_sysprops_overhead))

				<< ::form(percentformat2,
					"      -file system",
					TWOPERCENTS(arg.b_filesystem_overhead)) 

				<< ::form(percentformat2,
					"      -storage manager",
					TWOPERCENTS(arg.b_sm_overhead))
				;
			}
		}
		out << endl;
	}

	dassert(!tclout.bad());
	if (arg.flag & du_m) {
		// interested in details of sm overhead
		if(arg.flag & du_b) {
			out << SUMM(arg.b_sm_overhead);
		} else {
			out << arg.path << endl;

			static char *intformat     =  "%-40s: %10d\n";
			static char *percentformat =  "%-40s: %10d (%02.2f%% of %d)\n";

			// tfp == total file pages for pools & indexes
			int tfp = arg.du.u.fi.p_unused_sm + arg.du.u.fi.p_unused_lg + arg.du.u.fi.p_used;

			if(counted_all) {
				out << ::form(intformat, "All pages", tfp);
			} else {
				out << ::form(intformat, "Pool and index pages", tfp);
			}

			if(tfp>0) {
				int i = arg.du.u.fi.p_unused_sm + arg.du.u.fi.p_unused_lg;
				out << ::form(percentformat, "-- unallocated pages", 
					PERCENT(i, tfp));
				if(i>0) { 	// unallocated details

					out << ::form(percentformat, "---- file pages",
						PERCENT(arg.du.u.fi.p_unused_sm, i));
					out << ::form(percentformat, "---- large object pages",
						PERCENT(arg.du.u.fi.p_unused_lg, i));
				}/*unallocated details */

				out << ::form(percentformat, "-- allocated pages",
					PERCENT(arg.du.u.fi.p_used, tfp));
				{	// allocated details

				out << ::form(percentformat, "---- large object index pages ",
					PERCENT(arg.du.u.fi.p_large_index, tfp));
				if(arg.du.u.fi.p_large_index > 0)
				{ 	// allocated lg index pages detail

					int tib = arg.du.u.fi.p_large_index * sm_page_size;
					out << ::form(percentformat, "------ bytes overhead",
					PERCENT(tib, tib));

				} /* lg index pgs detail */

				out << ::form(percentformat, "---- file pages ",
					PERCENT(arg.du.u.fi.p_small, tfp));
				{ 	// allocated file pages detail
					int tfb = arg.du.u.fi.p_small * sm_page_size;
					if(tfb>0) {
					out << ::form(percentformat, "------ bytes user data ",
						PERCENT(arg.du.u.fi.b_sm_user_data, tfb));

					out << ::form(percentformat, "------ bytes unused",
						PERCENT(arg.du.u.fi.b_sm_unused, tfb));
					}

					int to = arg.du.u.fi.b_rectag +
							arg.du.u.fi.b_align +
							arg.du.u.fi.b_slothdr;

					if(to>0) {
					out << ::form(percentformat, "------ bytes overhead",
						PERCENT(to, tfb));

					out << ::form(percentformat, "-------- slot array + pg hdr",
						PERCENT(arg.du.u.fi.b_slothdr, to));
					out << ::form(percentformat, "-------- alignment",
						PERCENT(arg.du.u.fi.b_align, to));
					out << ::form(percentformat, "-------- rec hdrs",
						PERCENT(arg.du.u.fi.b_rectag, to));
					}
				}/*file pages detail */

				{	// large object data pages (allocated) details
					out << ::form(percentformat, "---- large object data pages ",
						PERCENT(arg.du.u.fi.p_large, tfp));
					{
						int tlb = arg.du.u.fi.p_large * sm_page_size;
						if(tlb>0) {

						out << ::form(percentformat, "------ bytes user data ",
							PERCENT(arg.du.u.fi.b_lg_user_data, tlb));

						out << ::form(percentformat, "------ bytes unused",
							PERCENT(arg.du.u.fi.b_lg_unused, tlb));

						out << ::form(percentformat, "------ bytes overhead(hdrs)",
							PERCENT(arg.du.u.fi.b_lg_hdr, tlb));
						}
					}
				}/* lg object data pages detail */
				}/* allocated details */
			}/*tfp > 0*/

			int t_o = 
				arg.du.u.fi.b_rectag +
				arg.du.u.fi.b_align +
				arg.du.u.fi.b_slothdr +
				arg.du.u.fi.b_lg_hdr +
				(arg.du.u.fi.p_large_index * sm_page_size);

			dassert(t_o == arg.b_sm_overhead);

			/* unallocated small + large pages */
			int t_ua = (arg.du.u.fi.p_unused_sm + arg.du.u.fi.p_unused_lg)*sm_page_size;

			/* unused bytes (in allocated pages) */
			int t_un = arg.du.u.fi.b_sm_unused + arg.du.u.fi.b_lg_unused;

			/* user data */
			int t_ud = arg.du.u.fi.b_sm_user_data + arg.du.u.fi.b_lg_user_data;
			int t_a = t_o + t_un + t_ud;
			int t = t_a + t_ua;

			out << ::form(intformat, "Total bytes ", t_ua + t_a);
			if(t > 0) {
				out << ::form(percentformat, "Total bytes unallocated", 
							PERCENT(t_ua, t));
				out << ::form(percentformat, "Total bytes allocated", 
							PERCENT(t_a, t));
			}

			if(t_a>0) {
				out << ::form(percentformat, "Total bytes overhead", 
							PERCENT(t_o, t_a));
				out << ::form(percentformat, 
					"Total bytes unused", 
							PERCENT(t_un, t_a));
				out << ::form(percentformat, "Total bytes user data",
							PERCENT(t_ud, t_a));
			}
		}
		out << endl;
	}
	tclout << ends;
	dassert(!tclout.bad());
	Tcl_AppendResult(ip, out.str(),  0);
#endif notdef

}/*du_print*/

VASResult 
recursive_dir_descent(ClientData clientdata, Tcl_Interp *ip, du_args&  arg)
{	
	char	pathname[MAXPATHLEN];
    VASResult   res;
	Cookie 		cookie;
	char 		*Dirent = NULL, *dirent;
	int			nentries=0, nbytes=1024;
	_entry		*se;
	lvid_t		oid;
	FUNC(recursive_dir_descent);

	Dirent = dirent = new char[1024]; 
	if(!Dirent) {
		Tcl_AppendResult(ip, "cannot calloc a buffer that big.",0);
		return(TCL_ERROR);
	}
	if(!arg.path) {
		DBG(<<"");
		Tcl_AppendResult(ip, "\".\": No such directory.", 0);
		res = TCL_ERROR; goto done;
	}

	cookie = (Cookie)NoSuchCookie;
	do 
	{
		dirent = Dirent;
		// re-user dirent space
		CALL( getDirEntries(arg.oid, dirent,nbytes,&nentries,
														&cookie));

		DBG(
			<< "Vas->getDirEntries returned cookie" << 
			::hex((u_long)cookie) << ", Res" << res
			<< "nentries" << nentries << 
			", nbytes" << nbytes
		)
		if ( res != SVAS_OK )
		{
				DBG(<< "UNEXPECTED ERR" << Vas->status.vasreason);
				break;
		}

		// getDirEnt     suceeded 
		if(nentries == 0) 
		{
			// no more entries in this directory
			break;
		}

		int 	i;
		char	*p;

		// for all entries in this directory
		for( p=dirent,i=0; i<nentries; i++, p += se->entry_len) {
			se = (_entry *)p;

			DBG(<<"i=" << i
					<< "p=" << ::hex((unsigned int)p)
					<< "se=" << ::hex((unsigned int)se)
					<< "nentries=" << nentries
					);

			// skip entries for "." and ".."
			if(se->name == '.') 
			{
				char *c = &(se->name);
				c++;
				if( ((*c=='.')&&(*(c+1)=='\0')) || (*c=='\0')) 
					 continue;
			}

			// create a new object 
			strcpy(pathname,arg.path);
			strcat(pathname, "/");
			strcat(pathname, &(se->name));

			du_args    newarg(pathname, arg.flag, arg.ip);
			DBG(<<"calling du_traverse");
 			res = du_traverse(clientdata, ip, newarg);
			if ( res != SVAS_OK ) break;

			arg += newarg;
		}
	} while(cookie != (Cookie)TerminalCookie);

done:
	DBG(<<"return from recursive_dir_descent");
	delete [] Dirent;
	VASERR(res);
}

VASResult 
get_pool_size(ClientData clientdata, du_args&	arg)
{
	VASResult	res;
	Cookie      cookie = NoSuchCookie;
	bool      eof = FALSE;
	SysProps	sys;
	lrid_t		loid;
	int			overhead;
	vec_t		emptyvec;
	ObjectSize	used=0, more=0;

	du_args		poolargs(arg.path, arg.flag, arg.ip); // stats are 0

	// open scan on pool
	CALL( openPoolScan(arg.oid, &cookie) );
	if ( res != SVAS_OK) return res;

	// scan all elements in the pool
	while ( 1 ) {
		
		CALL( nextPoolScan(&cookie, &eof, &loid,
				0, 0, emptyvec, &used, &more, SH, &sys, &overhead) );

		dassert(more==0);
		dassert(used==0);

		if ( res != SVAS_OK ) {
			CALL(closePoolScan(cookie));
			return res;
		}
		if ( eof ) break;

		arg.addsysp(overhead);
		arg.adduser(sys.hsize+sys.csize);

		{
			// collect sm layer  overhead  for each index in object
			du_args		anonargs(arg.path, arg.flag, arg.ip); 
			anonargs.oid = loid;
			if( anonargs.diskusage(false,Vas) == SVAS_OK) {
				arg += anonargs;
			}
		}
	}
	CALL(closePoolScan(cookie));

	// collect sm layer  overhead  for pool
	poolargs.oid = arg.oid;
	if(poolargs.diskusage(false,Vas)==SVAS_OK) {
		arg += poolargs;
	}

	return SVAS_OK;
} /* get_pool_size */
#endif /*NOTDEF*/


int 
cmd_df(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
//TODO: try this under purify to see if we need to free
// anything allocated by Tcl_SplitList


	CMDFUNC(cmd_df);
	df_flags		flags= df_none;
	struct sm_du_stats_t	data;

	int 		c;
	extern int 	optind;
	extern int 	opterr;

	// disable error message from getopt
	opterr = 0;   
	optind = 1;     

	// parse all input options
	while ( (c=getopt(ac,av,"epbas")) != -1 ) {
		switch (c) {
			case 's':	flags = (df_flags)(flags |df_s); break; // summary
			case 'e':	flags = (df_flags)(flags |df_e); break;
			case 'p':	flags = (df_flags)(flags |df_p); break;
			case 'b':	flags = (df_flags)(flags |df_b); break;
			case 'a':	flags = (df_flags)(flags |df_a); break;
			case '?':	
				SYNTAXERROR(ip, " in command ", _fname_debug_);
				//returns
		}
	}
	if(flags == df_none || flags == df_s) {
		// default 
		flags = (df_flags)((unsigned)df_a | (unsigned)df_s);
	}

	CHECKCONNECTED;
	BEGIN_NESTED_TRANSACTION

	int 	no_vols=0;
	lvid_t	*lvid_list=0;
	typedef char *namep;
	namep	*name_list=0;

	// optind is the argv index of the next argument to be processed
	// after all the flag arguments have been processed
	// ac==optind means there are no more arguments after the flags

	if ( ac == optind ) {
		char 	**vollist;
		//
		// get all volume ids that are mounted
		// first, turn off verbose if necessary
		//
		bool save_verbose = verbose;

		verbose = 0;
		if(Tcl_Eval(ip, "getmnt 1000 root") != TCL_OK) {
			return TCL_ERROR;
		}
		verbose = save_verbose;
		if(Tcl_SplitList(ip, ip->result, &no_vols, &vollist)!=SVAS_OK) {
			free((char *)vollist);
			return TCL_ERROR;
		}
		// wipe out the old result
		Tcl_ResetResult(ip);

		DBG(<< no_vols << " are mounted");
		lvid_list = new lvid_t[no_vols];
		name_list = new namep[no_vols];

		for(int i=0; i<no_vols; i++) {
			dassert(isdigit(*vollist[i]));
			GET_VID(ip, vollist[i], lvid_list[i]);

			if(Tcl_VarEval(ip, "pwd ", vollist[i], 0) != TCL_OK) {
				no_vols=0;
				break;
				// return TCL_ERROR;
			}
			name_list[i] = new char[strlen(ip->result)+1];
			strcpy(name_list[i], ip->result);
			Tcl_ResetResult(ip);
			DBG(<< "name_list[" << i << "]=" << name_list[i]);
			DBG(<< "lvid_list[" << i << "]=" << lvid_list[i]);
		}
	} else {
		// some filenames were given
		int i,j;

		// count the number of arguments left (volumeids or paths)
		for (i=optind; i< ac; i++,no_vols++);

		DBG(<< no_vols << " volums in request");

		lvid_list = new lvid_t[no_vols];
		name_list = new namep[no_vols];

		for (j=0, i=optind; i< ac; i++,j++) {
			GET_VID(ip, av[i], lvid_list[j]);
			name_list[j] = av[i];
			DBG(<< "name_list[" << j << "]=" << name_list[j]);
			DBG(<< "lvid_list[" << j << "]=" << lvid_list[j]);
		}
	}

	{
		for ( int i=0; i < no_vols; i++) {
			DBG(<<"volume i=" << i);
			DBG(<<"name_list[i]=" << name_list[i]);
			DBG(<<"lvid_list[i]=" << lvid_list[i]);

			Tcl_AppendResult(ip, name_list[i], ":\n", 0);
			// Tcl_AppendElement(ip, name_list[i]);

			data.clear();

			CALL(disk_usage(lvid_list[i],&data));
			if ( res != SVAS_OK ) {
				break;
			}
			tclout.seekp(ios::beg);
			dassert(!tclout.bad());
			df_print(tclout, data, flags);
			dassert(!tclout.bad());
			tclout << ends;
			Tcl_AppendResult(ip, tclout.str(), 0);
			//Tcl_AppendElement(ip, tclout.str());
		}
	}
	delete lvid_list;
	if ( ac == optind ) {
		for(int i=0; i<no_vols; i++) {
			free( name_list[i] );
		}
	}
	delete name_list;

	END_NESTED_TRANSACTION
	NESTEDVASERR(res, reason);
} /* cmd_df */

//
// this is called when we *know* we've got info for the
// entire volume
void 
df_print(ostream &o,
	const struct sm_du_stats_t&	space, const df_flags flag)
{
	DBG(<<"flags for printing" << flag );

#ifdef JUNK
	cerr << "**************************" << endl;
	cerr << space << endl;
	cerr << "**************************" << endl;
#endif
	int		t;

	dassert(!o.bad());
	int a = 
			space.volume_map.store_directory.leaf_pg_cnt +
			space.volume_map.store_directory.int_pg_cnt +

			space.volume_map.root_index.leaf_pg_cnt +
			space.volume_map.root_index.int_pg_cnt +

			space.volume_map.lid_map.leaf_pg_cnt +
			space.volume_map.lid_map.int_pg_cnt +

			space.volume_map.lid_remote_map.leaf_pg_cnt +
			space.volume_map.lid_remote_map.int_pg_cnt +

			space.btree.leaf_pg_cnt +
			space.btree.int_pg_cnt +

			space.rtree.leaf_pg_cnt +
			space.rtree.int_pg_cnt +
			space.rdtree.leaf_pg_cnt +
			space.rdtree.int_pg_cnt +

			space.file.file_pg_cnt +
			space.file.lgdata_pg_cnt +
			space.file.lgindex_pg_cnt +

			space.small_store.btree_cnt
			;

	int u = (

			space.volume_map.store_directory.unlink_pg_cnt +
			space.volume_map.store_directory.unalloc_pg_cnt +

			space.volume_map.root_index.unlink_pg_cnt +
			space.volume_map.root_index.unalloc_pg_cnt +

			space.volume_map.lid_map.unlink_pg_cnt +
			space.volume_map.lid_map.unalloc_pg_cnt +

			space.volume_map.lid_remote_map.unlink_pg_cnt +
			space.volume_map.lid_remote_map.unalloc_pg_cnt +

			space.btree.unlink_pg_cnt +
			space.btree.unalloc_pg_cnt + 

			space.rtree.unalloc_pg_cnt +
			space.rdtree.unalloc_pg_cnt +
			
			space.file.unalloc_file_pg_cnt + 
			space.file.unalloc_large_pg_cnt +

			space.small_store.unalloc_pg_cnt
			);

		// more categories that we'll split off:
		int	align =0;
		int	sysp=0;
		int	userdata=0;
		int	unused=0;
		int	smother=0;
		int	otherother=0;
		int	slots=0;
		int	lgobjov=0;

/* for totals */
#define TOTAL_IFORM(x) ::form("%10s %10s %10d"," "," ",(x))
#define CS2T_IFORM(x) ::form("%10s %10d %10d"," ",(x),(x))
/* for subtotals */
#define SUBTOT_IFORM(x) ::form("%10s %10d"," ",(x))
#define CI2S_IFORM(x) ::form("%10d %10d",(x),(x))
/* for items */
#define ITEM_IFORM(x) ::form("%10d",(x))

#define FLTFORM(x) ::form("%8.3f",(x))
#define YYY(xxx,ttt)\
	ITEM_IFORM(xxx) << "  (" << FLTFORM((float)(xxx *100)/ttt) << "% of " << ITEM_IFORM((int)ttt) <<")\n";
#define PERCENTFORM(xxx,ttl) YYY(xxx,ttl)

	{
		o << "FILES:" << endl;
		o   
		<< "Files                 : " << TOTAL_IFORM(space.file_cnt) <<  endl;
		o
		<< "1-page B-trees        : " << TOTAL_IFORM(space.small_store.btree_cnt) <<  endl;
		o
		<< "B-trees               : " << TOTAL_IFORM(space.btree_cnt) <<  endl;
		o
		<< "R-trees               : " << TOTAL_IFORM(space.rtree_cnt) <<  endl;
		o
		<< "RD-trees              : " << TOTAL_IFORM(space.rdtree_cnt) <<  endl;
		o<<endl;
	}
	if ( flag & df_e ) {
		o << "EXTENTS:" << endl;

		t = (space.volume_hdr.alloc_ext_cnt + space.volume_hdr.unalloc_ext_cnt + space.volume_hdr.hdr_ext_cnt);
		o   
			<< "Pages per extent      : " << TOTAL_IFORM(space.volume_hdr.extent_size) <<  endl
			;
		o
			<< "Extents in volume     : " << TOTAL_IFORM(t) 
			<< endl
			;

		if((flag & df_s)==0) {
			// expands extents in volume:
			o 
			<< "   vol hdr, bitmap ext: " << SUBTOT_IFORM(space.volume_hdr.hdr_ext_cnt) << endl
			<< "   free extents       : " << SUBTOT_IFORM(space.volume_hdr.unalloc_ext_cnt) << endl
			<< "   allocated extents  : " << SUBTOT_IFORM(space.volume_hdr.alloc_ext_cnt) << endl
			;

		}
		o<<endl;
	}

	// -p flag : pages
	if ( flag & df_p) {
		o << "PAGES:" << endl;
		t = (a + u + 
			((space.volume_hdr.hdr_ext_cnt + space.volume_hdr.unalloc_ext_cnt) *
			space.volume_hdr.extent_size));
		o 
			<< "Total pages in volume : " << 
				TOTAL_IFORM(t)  << endl ;

		if((flag & df_s)==0) {
			o
			<< "      small obj pgs   : " << ITEM_IFORM(space.file.file_pg_cnt) << endl
			<< "      large data pgs  : " << ITEM_IFORM(space.file.lgdata_pg_cnt) << endl
			<< "      large index pgs : " << ITEM_IFORM(space.file.lgindex_pg_cnt) << endl
			<< "      btree leaf pgs  : " << ITEM_IFORM(space.btree.leaf_pg_cnt) << endl
			<< "      btree intern pgs: " << ITEM_IFORM(space.btree.int_pg_cnt) << endl
			<< "      rtree leaf pgs  : " << ITEM_IFORM(space.rtree.leaf_pg_cnt) << endl
			<< "      rtree intern pgs: " << ITEM_IFORM(space.rtree.int_pg_cnt) << endl
			<< "      rdtree leaf pgs : " << ITEM_IFORM(space.rdtree.leaf_pg_cnt) << endl
			<< "      rdtree intern pg: " << ITEM_IFORM(space.rdtree.int_pg_cnt) << endl
			<< "      store-dir lf pg : " << ITEM_IFORM(space.volume_map.store_directory.leaf_pg_cnt) << endl
			<< "      store-dir int pg: " << ITEM_IFORM(space.volume_map.store_directory.int_pg_cnt) << endl
			<< "      root-idx lf pg  : " << ITEM_IFORM(space.volume_map.root_index.leaf_pg_cnt) << endl
			<< "      root-idx int pg : " << ITEM_IFORM(space.volume_map.root_index.int_pg_cnt) << endl
			<< "      lid-map lf pg   : " << ITEM_IFORM(space.volume_map.lid_map.leaf_pg_cnt) << endl
			<< "      lid-map int pg  : " << ITEM_IFORM(space.volume_map.lid_map.int_pg_cnt) << endl
			<< "      lidR-map lf pg  : " << ITEM_IFORM(space.volume_map.lid_remote_map.leaf_pg_cnt) << endl
			<< "      lidR-map int pg : " << ITEM_IFORM(space.volume_map.lid_remote_map.int_pg_cnt) << endl
			<< "      1-page btree pg : " << ITEM_IFORM(space.small_store.btree_cnt) << endl
		    << "    used alloc pgs    : " << SUBTOT_IFORM(a) << endl
		    << "    unused alloc pgs  : " << SUBTOT_IFORM(u) << endl
			;
		}
		if(space.volume_hdr.alloc_ext_cnt * space.volume_hdr.extent_size
			!= a + u) {
			DBG(<<"FIX!");
		}
		o   << "Pages in alloc extents: " << TOTAL_IFORM(u+a) << endl;
		o   << "Pages in unalloc exts : " 
					<< TOTAL_IFORM(space.volume_hdr.unalloc_ext_cnt*space.volume_hdr.extent_size) << endl;
		o   << "Pages in hdr, bmap ext: " 
					<< TOTAL_IFORM(space.volume_hdr.hdr_ext_cnt*space.volume_hdr.extent_size) << endl;
		o << endl;
	}
	// -b flag : bytes

	if ( flag & df_b) {
		o  << "BYTES:" << setprecision(3) << endl;
		t = (space.volume_hdr.alloc_ext_cnt + space.volume_hdr.unalloc_ext_cnt +
			 space.volume_hdr.hdr_ext_cnt);
		int bytesinvol =  (t * space.volume_hdr.extent_size) * sm_page_size;
		int abytes = a * sm_page_size;

		if(flag & df_s) {
			o << "Bytes in volume       : "
				<< TOTAL_IFORM(bytesinvol)
				<< endl;
			o << "Bytes per page        : "
				<< TOTAL_IFORM(sm_page_size)
				<< endl;
			o << "In pages unused       : " 
				<< TOTAL_IFORM(u*sm_page_size) 
				<< endl;
			o << "In pages used         : " 
				<< TOTAL_IFORM(abytes) 
				<< endl;
			o << "In pages unallocated  : " 
				<< TOTAL_IFORM(space.volume_hdr.unalloc_ext_cnt*space.volume_hdr.extent_size*sm_page_size) 
				<< endl;

		} else { /* not a summary */
			o << "Bytes in volume                    : "
				<< TOTAL_IFORM(bytesinvol)
				<< endl;
			o << "Bytes per page                     : "
				<< TOTAL_IFORM(sm_page_size)
				<< endl;

			int	tot = u * sm_page_size;
			o << "In pages unused                    : " 
				<< TOTAL_IFORM(tot)
				<< endl;
			if(tot > 0) {
					t = space.file.unalloc_file_pg_cnt;
				o << "         unused small obj pgs      : " << PERCENTFORM(t,tot)
					t = space.file.unalloc_large_pg_cnt;
				o << "         unused large obj pgs      : " << PERCENTFORM(t,tot)
					t = space.btree.unlink_pg_cnt;
				o << "         unlinked btree pgs        : " << PERCENTFORM(t,tot)
					t = space.btree.unalloc_pg_cnt;
				o << "         unused btree pgs          : " << PERCENTFORM(t,tot)
					t = space.rtree.unalloc_pg_cnt;
				o << "         unused rtree pgs          : " << PERCENTFORM(t,tot)
					t = space.rdtree.unalloc_pg_cnt;
				o << "         unused rdtree pgs         : " << PERCENTFORM(t,tot)
			}
			o << endl;

			o << "In pages used                      : " 
				<< TOTAL_IFORM(abytes) 
				<< endl;



			{ /**OBJECTS ********************************************/
					int tobjbytes = space.file.total_bytes();
				o << "In object pages                    : " << PERCENTFORM(tobjbytes,abytes)

					userdata =  space.file.file_pg.rec_body_bs +
								 space.file.lgdata_pg.data_bs;
				o << "     user data in records          : " << PERCENTFORM(userdata,tobjbytes)
				if(userdata>0) {
						t = space.file.file_pg.rec_body_bs;
					o << "         small object data         : " << PERCENTFORM(t,userdata)
						t = space.file.lgdata_pg.data_bs;
					o << "         large object data         : " << PERCENTFORM(t,userdata)
				}
				o <<endl;

					sysp =  space.file.file_pg.rec_hdr_bs;
				o << "     record headers                : "  << PERCENTFORM(sysp,tobjbytes)
				o <<endl;

					unused = space.file.file_pg.free_bs +
						space.file.lgdata_pg.unused_bs +
						space.file.lgindex_pg.unused_bs;
				o << "     unused in allocated pages     : " << PERCENTFORM(unused,tobjbytes)
				if(unused>0) {
						t =  space.file.file_pg.free_bs;
					o << "         in small object pages     : " << PERCENTFORM(t,unused)
						t =  space.file.lgdata_pg.unused_bs;
					o << "         in large object leaf pgs  : " << PERCENTFORM(t,unused)
						t =  space.file.lgindex_pg.unused_bs;
					o << "         in large obj interior pgs : " << PERCENTFORM(t,unused)
				}
				o <<endl;

					align = space.file.file_pg.rec_hdr_align_bs +
						space.file.file_pg.rec_body_align_bs;
				o << "     bytes lost in alignment       : " << PERCENTFORM(align,tobjbytes)
				if(align>0) {
						t = space.file.file_pg.rec_hdr_align_bs;
					o << "         for sysprops (headers)    : " << PERCENTFORM(t,align)
						t = space.file.file_pg.rec_body_align_bs;
					o << "         for user data             : " << PERCENTFORM(t,align)
				}
				o <<endl;

					smother =  space.file.file_pg.hdr_bs +
						space.file.lgdata_pg.hdr_bs;
				o << "     SM page headers               : " << PERCENTFORM(smother,tobjbytes)
					t = space.file.file_pg.hdr_bs;
				o << "         for small obj pgs         : " << PERCENTFORM(t,smother)
					t = space.file.lgdata_pg.hdr_bs;
				o << "         for lg obj data pgs       : " << PERCENTFORM(t,smother)
				o <<endl;

					int otherother = space.file.file_pg.rec_tag_bs +
						space.file.file_pg.slots_unused_bs +
						space.file.file_pg.slots_used_bs +
						space.file.file_pg.rec_lg_chunk_bs +
						space.file.file_pg.rec_lg_indirect_bs +
						space.file.lgindex_pg.used_bs;
				o << "     other SM overhead             : " << PERCENTFORM(otherother,tobjbytes)
					t = space.file.file_pg.rec_tag_bs;
				o << "         for rec tags              : " << PERCENTFORM(t,otherother)

					slots = space.file.file_pg.slots_unused_bs +
						space.file.file_pg.slots_used_bs ;
				o << "         for slots                 : " << PERCENTFORM(slots,otherother)
				if(slots > 0) {
					t = space.file.file_pg.slots_used_bs;
				o << "             slots used            : " << PERCENTFORM(t,slots)
					t = space.file.file_pg.slots_unused_bs;
				o << "             slots unused          : " << PERCENTFORM(t,slots)
				}

					lgobjov = 
						space.file.file_pg.rec_lg_chunk_bs +
						space.file.file_pg.rec_lg_indirect_bs +
						space.file.lgindex_pg.used_bs;
				o << "         for large object overhead : " << PERCENTFORM(lgobjov,otherother)
				if(lgobjov > 0) {
						t = space.file.file_pg.rec_lg_chunk_bs;
					o << "             chunks                : " << PERCENTFORM(t,lgobjov)
						t= space.file.file_pg.rec_lg_indirect_bs;
					o << "             indirect              : " << PERCENTFORM(t,lgobjov)
						t=	space.file.lgindex_pg.used_bs;
					o << "             index pg              : " << PERCENTFORM(t,lgobjov)
				}
				o <<endl;

					smother += otherother;
					smother += align;
			} /* end objects */

			{ /**INDEXES ********************************************/
				/**           SM **************************************/
				{
						t = space.volume_map.total_bytes();
						float tsmidx = (float)t;
					if(tsmidx>0) {
					o << "In SM indexes                      : " << PERCENTFORM(t,abytes)
				/**           SM store directory **************************/

						t = space.volume_map.store_directory.total_bytes();
						float tstore = (float)t;
					o << "In SM store directory index        : " << PERCENTFORM(t,tsmidx)
					t = space.volume_map.store_directory.leaf_pg.hdr_bs +
					space.volume_map.store_directory.leaf_pg.entry_overhead_bs +
					space.volume_map.store_directory.int_pg.used_bs;
					o << "     overhead                      : " << PERCENTFORM(t,tstore)
						t = space.volume_map.store_directory.leaf_pg.key_bs;
					o << "     keys                          : " << PERCENTFORM(t,tstore)
						t = space.volume_map.store_directory.leaf_pg.data_bs;
					o << "     values                        : " << PERCENTFORM(t,tstore)
						t = space.volume_map.store_directory.leaf_pg.unused_bs
						 +  space.volume_map.store_directory.int_pg.unused_bs;
					o << "     unused                        : " << PERCENTFORM(t,tstore)

				/**           SM root directory **************************/
						t = space.volume_map.root_index.total_bytes();
						float troot = (float)t;
					o << "In SM root directory index         : " << PERCENTFORM(t,tsmidx)
						t = space.volume_map.root_index.leaf_pg.hdr_bs +
							space.volume_map.root_index.leaf_pg.entry_overhead_bs +
							space.volume_map.root_index.int_pg.used_bs;
					o << "     overhead                      : " << PERCENTFORM(t,troot)
						t =space.volume_map.root_index.leaf_pg.key_bs;
					o << "     keys                          : " << PERCENTFORM(t,troot)
						t =space.volume_map.root_index.leaf_pg.data_bs;
					o << "     values                        : " << PERCENTFORM(t,troot)
						t =space.volume_map.root_index.leaf_pg.unused_bs
							+space.volume_map.root_index.int_pg.unused_bs;
					o << "     unused                        : " << PERCENTFORM(t,troot)
				/**           SM lid map **************************/
						t = space.volume_map.lid_map.total_bytes();
						float tlidmap = (float)t;
					o << "In SM lid map index                : " << PERCENTFORM(t,tsmidx)
						t = space.volume_map.lid_map.leaf_pg.hdr_bs +
							space.volume_map.lid_map.leaf_pg.entry_overhead_bs +
							space.volume_map.lid_map.int_pg.used_bs;
					o << "     overhead                      : " << PERCENTFORM(t,tlidmap)
						t =space.volume_map.lid_map.leaf_pg.key_bs;
					o << "     keys                          : " << PERCENTFORM(t,tlidmap)
						t =space.volume_map.lid_map.leaf_pg.data_bs;
					o << "     values                        : " << PERCENTFORM(t,tlidmap)
						t =space.volume_map.lid_map.leaf_pg.unused_bs
							+space.volume_map.lid_map.int_pg.unused_bs;
					o << "     unused                        : " << PERCENTFORM(t,tlidmap)

				/**           SM lid map-remote  **************************/
						t = space.volume_map.lid_remote_map.total_bytes();
						float tlidremote = (float)t;
					o << "In SM remote lid map index         : " << PERCENTFORM(t,tsmidx)
						t = space.volume_map.lid_remote_map.leaf_pg.hdr_bs +
							space.volume_map.lid_remote_map.leaf_pg.entry_overhead_bs +
							space.volume_map.lid_remote_map.int_pg.used_bs;
					o << "     overhead                      : " << PERCENTFORM(t,tlidremote)
						t =space.volume_map.lid_remote_map.leaf_pg.key_bs;
					o << "     keys                          : " << PERCENTFORM(t,tlidremote)
						t =space.volume_map.lid_remote_map.leaf_pg.data_bs;
					o << "     values                        : " << PERCENTFORM(t,tlidremote)
						t =space.volume_map.lid_remote_map.leaf_pg.unused_bs
						  +space.volume_map.lid_remote_map.int_pg.unused_bs;
					o << "     unused                        : " << PERCENTFORM(t,tlidremote)
					o << endl;
				} /* if tsmidx > 0*/
				}
				/**           USER ************************************/
				{
						t = space.btree.total_bytes();
						float tuidx = (float)t;
					if(tuidx > 0) {
					o << "In user indexes                    : " << PERCENTFORM(t,abytes);

						t = space.btree.leaf_pg.hdr_bs +
							space.btree.leaf_pg.entry_overhead_bs +
							space.btree.int_pg.used_bs;

						t += space.small_store.btree_lf.hdr_bs +
							space.small_store.btree_lf.entry_overhead_bs;

						smother += t;
					o << "     overhead                      : " << PERCENTFORM(t,tuidx)

						t = space.btree.leaf_pg.key_bs;
					o << "     keys                          : " << PERCENTFORM(t,tuidx)
						userdata += t;

						t = space.btree.leaf_pg.data_bs;
					o << "     values                        : " << PERCENTFORM(t,tuidx)
						userdata += t;

						t = space.btree.leaf_pg.unused_bs+
							space.btree.int_pg.unused_bs;
					o << "     unused                        : " << PERCENTFORM(t,tuidx)
						unused += t;

					o<<endl;
					}/* if tuidx > 0 */
				}
			} /* end indexes */

			o << "In pages unallocated               : " 
				<< TOTAL_IFORM(space.volume_hdr.unalloc_ext_cnt*space.volume_hdr.extent_size*sm_page_size) 
				<< endl;
			o<<endl;

		o << "SUMMARY(combined indices & objects): " << endl;
		t = userdata + smother + sysp;
		o << "Used bytes                         : " << PERCENTFORM(t,abytes)
		o << "      user data + keys + values    : " << PERCENTFORM(userdata,t)
		o << "      sysprops                     : " << PERCENTFORM(sysp,t)
		o << "      all overhead, incl alignment : " << PERCENTFORM(smother,t)
		o << endl;

		o << "Unused bytes in used pages         : " << PERCENTFORM(unused,abytes)
		t = userdata + unused + smother + sysp;
		// t and abytes should match
		o << "Total bytes in used pages          : " << PERCENTFORM(t,abytes)
		o << "         \"                         : " << PERCENTFORM(t,bytesinvol)

		o << "Bytes of unused pgs in alloc extent: " << PERCENTFORM(u*sm_page_size,bytesinvol)

		t = space.volume_hdr.unalloc_ext_cnt*space.volume_hdr.extent_size*sm_page_size;
		o << "Bytes of unallocated extents       : " << PERCENTFORM(t,bytesinvol)

		t = space.volume_hdr.hdr_ext_cnt*space.volume_hdr.extent_size*sm_page_size;
		o << "Bytes of vol hdr and bitmaps       : " << PERCENTFORM(t,bytesinvol)

		o << endl;
		} /* not summary */

	} /* end -b option */

	DBG(<<"end: flags for printing" << flag );
	o << ends;

	dassert(!o.bad());

} /* df_print */
