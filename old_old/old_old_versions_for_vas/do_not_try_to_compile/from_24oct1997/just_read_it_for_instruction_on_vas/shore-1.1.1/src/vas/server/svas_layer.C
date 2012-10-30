/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
* $Header: /p/shore/shore_cvs/src/vas/server/svas_layer.C,v 1.51 1996/03/26 17:10:17 nhall Exp $
*/
//
// vaslayer.C -- stuff for initializing
// vas layer; and all global variables needed for shore vas layer
//
#include <vas_internal.h>
#include "svas_service.h"
#include "rpc_service.h"
#include "tclshell_service.h"
#include "cltab.h"
#include <uname.h>

BEGIN_EXTERNCLIST
	int 	getrlimit(int, struct rlimit *);
#if	!defined(_SC_OPEN_MAX) && !defined(RLIMIT_NOFILE)
	int 	getdtablesize();
#endif
	void 	finish_mount_table();
	void 	init_mount_table(int);
END_EXTERNCLIST

#ifdef __GNUG__
template class w_ref_t<swapped_hdrinfo>;
#endif

svas_layer_init ShoreVasLayer;

const gid_t	svas_layer_init::RootGid=0;
const uid_t	svas_layer_init::RootUid=0;
int svas_layer_init::initialized=0;

extern w_error_info_t svas_error_info[];
extern w_error_info_t os_error_info[];

const char 			*
svas_layer_init::configuration_file_name() const
{
	return config_file_name;
}

void
svas_layer_init::set_configuration_file_name() 
{
	config_file_name = getenv("SHORE_RC");
	if(!config_file_name) {
		config_file_name = ".shoreconfig";
	}
	if(strlen(config_file_name)<1) {
		catastrophic("Bad value for $SHORE_RC.");
	}
}

svas_layer_init::svas_layer_init() 
{
	FUNC(svas_layer_init::svas_layer_init);

	// everything that can get initialized statically goes here
	nfs_service=0;
	mount_service=0;
	client_service=0;
	remote_service=0;
	shell_service=0;

	options=0;

	opt_mounttab_size=0;;

	opt_nfsd_port=0;;
	opt_mountd_port=0;
	opt_client_port=0;
	opt_remote_port=0;

	opt_reuseaddr=0;

	opt_nfsd_log=0;
	opt_mountd_log=0;
	opt_client_log=0;
	opt_log=0;

	opt_nfsd_log_level=0;
	opt_mountd_log_level=0;
	opt_client_log_level=0;
	opt_log=0;

	opt_shellrc=0;
	opt_tcllib=0;

	// boolean options
	opt_no_shm=0;
	opt_print_user_stats =0;
	opt_clear_user_stats =0;
	opt_print_user_errors = 0;
	opt_rpc_unregister = 0;
	opt_nfsd_pmap = 0;
	opt_mountd_pmap = 0;
	opt_remote_pmap = 0;
	opt_client_pmap = 0;
	opt_sysp_cache_size = 0;
	opt_root_volume = 0;

	opt_serve_array = 0;
	opt_serve_array_alloced=0;
	opt_serve_array_used=0;

	log = 0;
	Sm = 0;

	set_configuration_file_name();
}

void
svas_layer_init::initialize() 
{
	// for error messages
	char b[200];
	ostrstream msg(b, sizeof(b));

	// Initialize the error codes
	if (! (w_error_t::insert(
		"SVAS",
		svas_error_info, SVAS_ERRMAX - SVAS_ERRMIN + 1))) {
		catastrophic("Cannot initalize SVAS error codes.");
	}

	if (! (w_error_t::insert(
		"SVAS",	 // Unix compatibility service
		os_error_info, OS_ERRMAX - OS_ERRMIN + 1))) {
		catastrophic("Cannot initalize OS error codes.");
	}

	///////////////////////////////////////////////////
	// Create the log so that we can use it for
	// error reporting in the rest of this...
	///////////////////////////////////////////////////
	{
		const char *slf = ShoreVasLayer.opt_log? ShoreVasLayer.opt_log->value():0;
		ShoreVasLayer.log = new 
			ErrLog("shore", log_to_unix_file, (void *)slf);

		if(! ShoreVasLayer.log) {
			w_rc_t tmp_rc = RC(errno);
			catastrophic("Cannot open log file.", tmp_rc);
		}
		ShoreVasLayer.log->setloglevel(_loglevel);
	}
	//////////////////////////////////////////////////////////
	// ShoreVasLayer.log->clog is now a usable ostream-style log
	//////////////////////////////////////////////////////////


	//////////////////////////////////////////////////////////////
	// Configuration options:
	// "production" mode -- you must have an account
	//     for the shore database administrator, called "shoreadm",
	//     or whatever is defined for SHORE_ADM
	//     If configured thus, it's an error to run the server
	// 	   as any other user.
	//     shoreadm needs r/w perms on log and volumes 
	//
	// "experimental" mode --
	// 	   anyone can run the server, as long as the user has
	//	   r/w perms on the log and volumes
	//     For the duration of a server process, the Shore 
	// 	   database administrator is the user under which the
	//     server is run, thus the privilege goes with the
	//     user running the server.  This could give unexpected
	//     results because a user might sometimes be able to 
	//     do privileged operations, sometimes not, depending
	//     on who's running the server.
	//
	//////////////////////////////////////////////////////////////

	ShoreUid = getuid();
	DBG(<<"ShoreUid=" << ShoreUid);
	ShoreGid = getgid();
	DBG(<<"ShoreGid=" << ShoreGid);

#ifdef SHOREADM
	bool shoreadm_required = true;
#else
	bool shoreadm_required = false;
#define SHOREADM "shoreadm"
#endif

	{
		char 				*uname = NULL;
		int					usernamelen=0;

		if((uname = uid2uname(getuid()))==NULL) {
			perror("uid2uname");
			catastrophic("Cannot initialize ShoreVasLayer.");
		}
		usernamelen = strlen(uname);
		ShoreUser = new char [usernamelen+1];
		if(!ShoreUser) {
			catastrophic("Malloc failure: Cannot initialize ShoreVasLayer.");
		}
		strcpy(ShoreUser, uname);
		dassert(ShoreUser != 0);
	}
	if(strcmp(ShoreUser,SHOREADM)!=0) {
		if(shoreadm_required) {
			catastrophic(
			"This server can be run only by the shore administrator." ); 
		} else {
			msg << "Warning: no database administrator -- "
			<< "running under userid "
			<< ShoreUid << ", groupid " << ShoreGid << ends;

			logmessage(msg.str());
		}
	}
	DBG(<<"ShoreUser=" << ShoreUser);

	{ // get values for system limits:

#	define GETRLIMIT(z,q) {\
		struct rlimit buf;\
		if(getrlimit(z, &buf)<0) {\
			catastrophic("getrlimit(z)");\
		}\
		q = buf.rlim_cur;\
		if( q < buf.rlim_max ) {\
			msg << "Warning: " << #z << \
			"(" << q << ") is less than hard limit (" \
			<< buf.rlim_max << ")" << ends;\
			logmessage(msg.str()); \
		}\
	}

	/********* OpenMax ***************************************/

#		if defined(_SC_OPEN_MAX)
		OpenMax = (int) sysconf(_SC_OPEN_MAX);
#		elif defined(RLIMIT_NOFILE)
		GETRLIMIT(RLIMIT_NOFILE, OpenMax);
#		else  
		OpenMax = getdtablesize();
#		endif
#		ifdef notdef
			// could also try:
#			if defined(NOFILE)
			OpenMax = NOFILE;
#			endif
#		endif
		DBG(<<"OpenMax=" << OpenMax);

	/********* FileSizeMax ***************************************/
#		if defined(RLIMIT_FSIZE)
		GETRLIMIT(RLIMIT_FSIZE, FileSizeMax);
#		else
		FileSizeMax = 0x3ffffff; // TODO something more reasonable
#		endif

	/********* LinksMax ***************************************/
#		ifdef _PC_LINK_MAX
		LinksMax = (int)  pathconf("/",_PC_LINK_MAX);
#		elif defined(MAXLINK)
		LinksMax =  MAXLINK;
#		endif
		DBG(<<"LinksMax=" << LinksMax);

	/********* SymlinksMax ***************************************/
#		ifdef  MAXSYMLINKS
		SymlinksMax = MAXSYMLINKS; // max to be expanded in a single namei
#		else
		SymlinksMax = 20; // something's better than nothing
#		endif

		DBG(<<"SymlinksMax = " << SymlinksMax);

	/********* PathMax ***************************************/
#		if defined(_PC_PATH_MAX)
		PathMax = (int)  pathconf("/",_PC_PATH_MAX);
#		elif defined(MAXPATHLEN)
		PathMax = MAXPATHLEN; // max after expanding symlinks
#		else
		PathMax = 1024; 
#		endif
		DBG(<<"PathMax=" << PathMax);

	/********* NameMax ***************************************/
#		if defined(_PC_NAME_MAX)
		NameMax = (int)  pathconf("/",_PC_NAME_MAX);
#		else
		NameMax = (int)  255; // better than nothing
#		endif
		DBG(<<"NameMax=" << NameMax);
	}
}


svas_layer_init::~svas_layer_init() 
{
	options = 0; // it's up to the caller to delete this

#define _delete_(x) if (x) { x = 0; }

	_delete_(opt_shellrc);
	_delete_(opt_tcllib);
	_delete_(opt_nfsd_port);
	_delete_(opt_mountd_port);
	_delete_(opt_client_port);
	_delete_(opt_remote_port);
	_delete_(opt_nfsd_log);
	_delete_(opt_mountd_log);
	_delete_(opt_client_log);
	_delete_(opt_log);
	_delete_(opt_shell_log);
	_delete_(opt_nfsd_log_level);
	_delete_(opt_mountd_log_level);
	_delete_(opt_client_log_level);
	_delete_(opt_log_level);
	_delete_(opt_shell_log_level);
	_delete_(opt_print_user_stats);
	_delete_(opt_clear_user_stats);
	_delete_(opt_print_user_errors);
	_delete_(opt_make_tcl_shell);
	_delete_(opt_no_shm);
	_delete_(opt_root_volume);
	_delete_(opt_rpc_unregister);
	_delete_(opt_sysp_cache_size);
	_delete_(opt_nfsd_pmap);
	_delete_(opt_mountd_pmap);
	_delete_(opt_remote_pmap);
	_delete_(opt_client_pmap);
	_delete_(opt_mounttab_size);
#undef _delete_

#define _delete_(x) if (x) { delete x; x = 0; }
	_delete_(nfs_service);
	_delete_(mount_service);
	_delete_(client_service);
	_delete_(remote_service);
	_delete_(shell_service);
	_delete_(ShoreUser);
#undef _delete_

	initialized = 0;
	Sm = 0;

	cleanup_pwstuff();
	cleanup_netdb();
}

w_rc_t
svas_layer_init::set_value_device_path(option_t *opt,
	const char *value,
	ostream *err_stream)
{
	return ShoreVasLayer._set_value_device_path(opt,value,err_stream);
}

w_rc_t
svas_layer_init::_set_value_device_path(option_t *opt,
	const char *value,
	ostream *err_stream)
{
	typedef char * devname;

	DBG(<<"got value |" << value << "|" );
	if(value != 0) {
		if(!opt_serve_array) {
			opt_serve_array = new devname[3];
			opt_serve_array_alloced=3;
			opt_serve_array_used=0;
		}
		if(opt_serve_array_used >= opt_serve_array_alloced) {
			// realloc
			devname *temp = new devname[3+opt_serve_array_alloced];
			memcpy(temp, opt_serve_array, 
				sizeof(devname) * opt_serve_array_alloced);
			delete opt_serve_array;
			opt_serve_array = temp;
			opt_serve_array_alloced += 3;
		}
		dassert(opt_serve_array_used < opt_serve_array_alloced);

		
		char 		*devicename = new char[strlen(value)+1];
		DBG(<<"parsing " << value);
		(void) strcpy(devicename, value);
		opt_serve_array[opt_serve_array_used] = devicename;
		opt_serve_array_used++;
	} 

	return RCOK;
}

w_rc_t
svas_layer_init::serve_devices(svas_server *tmp)
{
	return ShoreVasLayer._serve_devices(tmp);
}
w_rc_t
svas_layer_init::_serve_devices(svas_server *temp)
{
	char b[200];
	ostrstream lmsg(b, sizeof(b));

	dassert(temp);

	if(opt_serve_array==0) return RCOK;

	int i;
	w_rc_t rc;
	char *devicename=0;

	for(i=0; i<opt_serve_array_used; i++) {
		devicename = opt_serve_array[i];
		dassert(devicename != 0);

		DBG(<<"about to serve " << devicename);
		if(temp->serve(devicename) != SVAS_OK) {
			rc = RC(OPT_BadValue);
			lmsg << "svas_serve: " << devicename << ": " << rc << endl << ends;
		}
		delete devicename;
		opt_serve_array[i] = 0;
	}
	delete opt_serve_array;
	opt_serve_array_used =0;
	opt_serve_array_alloced =0;

	if(rc) {
		logmessage(lmsg.str());
	}
	return rc;
}

w_rc_t
svas_layer_init::set_value_log_level(option_t *opt,
	const char *value,
	ostream *err_stream)
{
	FUNC(svas_layer_init::set_value_log_level);
	w_rc_t	e;
	bool	ok=true;
	LogPriority p;

	p = ErrLog::parse(value, &ok);
	if(ok) {
		e= option_t::set_value_charstr(opt, value, err_stream);

		// figure out which value to set
		if(opt == ShoreVasLayer.opt_nfsd_log_level) {
			DBG(<<"setting log level for nfsd to " << p);
			nfsd_loglevel = p;
		} else if(opt == ShoreVasLayer.opt_mountd_log_level) {
			DBG(<<"setting log level for mountd to " << p);
			mountd_loglevel = p;
		} else if(opt == ShoreVasLayer.opt_client_log_level) {
			DBG(<<"setting log level for clients to " << p);
			client_loglevel = p;
		} else if(opt == ShoreVasLayer.opt_shell_log_level) {
			DBG(<<"setting log level for shell to " << p);
			shell_loglevel = p;
		} else if(opt == ShoreVasLayer.opt_log_level) {
			DBG(<<"setting log level for the rest to " << p);
			_loglevel = p;
		}
	} else {
		e =  RC(OPT_BadValue);
	}
	return e;
}

w_rc_t
svas_layer::setup_options(option_group_t *options)
{ 
	FUNC(svas_layer::setup_options);
	{
		ShoreVasLayer.options = options;

#ifdef W_DO
#undef W_DO
#endif
#define W_DO(x) {\
    w_rc_t __e = (x);					\
    if (__e) { return __e; }\
}

		//
		// ports -- 
 	 	// marked as required but only because there are defaults 
	 	//
		W_DO(options->add_option("svas_nfsd_port", "1024 < integer < 65535", 
			"2999", "port for NFS service",
			false, 
			option_t::set_value_long, ShoreVasLayer.opt_nfsd_port));

		W_DO(options->add_option("svas_mountd_port", "1024 < integer < 65535", 
			"2997", "port for MOUNT service",
			false, 
			option_t::set_value_long, ShoreVasLayer.opt_mountd_port));

		W_DO(options->add_option("svas_client_port", "1024 < integer < 65535", 
			"2999", "port for communication with clients",
			false, 
			option_t::set_value_long, ShoreVasLayer.opt_client_port));

		W_DO(options->add_option("svas_remote_port", "1024 < integer < 65535", 
			"2998", "port for communication between SVASs",
			false, 
			option_t::set_value_long, ShoreVasLayer.opt_remote_port));

		//
		// logging destinations --
		// marked as required but only because there are defaults 
		//
		W_DO(options->add_option("svas_nfsd_log", "string",
			"-",
			"- (stderr), syslogd (to syslog daemon), or <file name>",
			false, 
			option_t::set_value_charstr, ShoreVasLayer.opt_nfsd_log));

		W_DO(options->add_option("svas_mountd_log", "string",
			"-",
			"- (stderr), syslogd (to syslog daemon), or <file name>",
			false, 
			option_t::set_value_charstr, ShoreVasLayer.opt_mountd_log));

		W_DO(options->add_option("svas_client_log", "string",
			"-",
			"- (stderr), syslogd (to syslog daemon), or <file name>",
			false, 
			option_t::set_value_charstr, ShoreVasLayer.opt_client_log));

		W_DO(options->add_option("svas_log", "string",
			"-",
			"- (stderr), syslogd (to syslog daemon), or <file name>",
			false, 
			option_t::set_value_charstr, ShoreVasLayer.opt_log));

		W_DO(options->add_option("svas_shell_log", "string",
			"-",
			"- (stderr), syslogd (to syslog daemon), or <file name>",
			false, 
			option_t::set_value_charstr, ShoreVasLayer.opt_shell_log));
		//
		// logging levels
		//
		W_DO(options->add_option("svas_nfsd_log_level", "string",
			"error",
			"trace|info|error|internal|fatal",
			false, 
			option_t::set_value_charstr, ShoreVasLayer.opt_nfsd_log_level));

		W_DO(options->add_option("svas_mountd_log_level", "string",
			"error",
			"trace|info|error|internal|fatal",
			false, 
			option_t::set_value_charstr, ShoreVasLayer.opt_mountd_log_level));

		W_DO(options->add_option("svas_client_log_level", "string",
			"error",
			"trace|info|error|internal|fatal",
			false, 
			option_t::set_value_charstr, ShoreVasLayer.opt_client_log_level));

		W_DO(options->add_option("svas_log_level", "string",
			"error",
			"trace|info|error|internal|fatal",
			false, 
			option_t::set_value_charstr, ShoreVasLayer.opt_log_level));

		W_DO(options->add_option("svas_shell_log_level", "string",
			"error",
			"trace|info|error|internal|fatal",
			false, 
			option_t::set_value_charstr, ShoreVasLayer.opt_shell_log_level));

		//
		// use portmapper for service?
		//
		W_DO(options->add_option("svas_nfsd_pmap", "yes/no", "no",
		"yes means the server will register this service with the portmapper",
			false, 
			option_t::set_value_bool, ShoreVasLayer.opt_nfsd_pmap));

		W_DO(options->add_option("svas_mountd_pmap", "yes/no", "no",
		"yes means the server will register this service with the portmapper",
			false, 
			option_t::set_value_bool, ShoreVasLayer.opt_mountd_pmap));

		W_DO(options->add_option("svas_remote_pmap", "yes/no", "no",
		"yes means the server will register this service with the portmapper",
			false, 
			option_t::set_value_bool, ShoreVasLayer.opt_remote_pmap));

		W_DO(options->add_option("svas_client_pmap", "yes/no", "no",
		"yes means the server will register this service with the portmapper",
			false, 
			option_t::set_value_bool, ShoreVasLayer.opt_client_pmap));

		W_DO(options->add_option("svas_reuse_addrs", "yes/no", "no",
		"yes means the server uses socket option SO_REUSEADDR",
			false, 
			option_t::set_value_bool, ShoreVasLayer.opt_reuseaddr));

		//
		// 	END of options for RPC services
		//
		{ option_t *junk;
		W_DO(options->add_option("svas_serve", "string",
			0,
			"<Unix path of device to serve>",
			false, 
				svas_layer_init::set_value_device_path,
				junk));
		}

		W_DO(options->add_option("svas_shellrc", "string",
			"shore.rc",
			"<file name for vas startup file>(soon to be obsolete)",
			false, 
				option_t::set_value_charstr, ShoreVasLayer.opt_shellrc));

		W_DO(options->add_option("svas_tcllib", "string",
			"vas.tcllib",
			"<file name for tcl library file>(soon to be obsolete)",
			false, 
				option_t::set_value_charstr, ShoreVasLayer.opt_tcllib));

		W_DO(options->add_option("svas_noshm", "yes/no", "no",
			"yes prohibits shared memory communication with clients ",
			false, 
			option_t::set_value_bool, ShoreVasLayer.opt_no_shm));

		W_DO(options->add_option("svas_root", "string integer.integer", 
			0, "device name and  lvid of volume where / is to be found",
			false, 
			option_t::set_value_charstr, ShoreVasLayer.opt_root_volume));

		W_DO(options->add_option("svas_rpc_unregister", "yes/no", "no",
			"yes unregisters any other server with this program/version ",
			false, 
			option_t::set_value_bool, ShoreVasLayer.opt_rpc_unregister));

		W_DO(options->add_option("svas_print_user_errors", "yes/no", "no",
			"yes causes server to print error messages to stdout",
			false, 
			option_t::set_value_bool, ShoreVasLayer.opt_print_user_errors));

		W_DO(options->add_option("svas_print_user_stats", "yes/no", "no",
		"yes causes server to print statistics to stdout when tx state changes",
			false, 
			option_t::set_value_bool, ShoreVasLayer.opt_print_user_stats));

		W_DO(options->add_option("svas_clear_user_stats", "yes/no", "no",
		"yes causes server to clear statistics each time it prints them",
			false, 
			option_t::set_value_bool, ShoreVasLayer.opt_clear_user_stats));

		W_DO(options->add_option("svas_tclshell", "yes/no", "yes",
		"yes causes server to run a Tcl shell",
			false, 
			option_t::set_value_bool, ShoreVasLayer.opt_make_tcl_shell));

		W_DO(options->add_option("svas_sysp_cache_size", "small integer", 
			"1", "number of objects worth of metadata cached",
			false, 
			option_t::set_value_long, ShoreVasLayer.opt_sysp_cache_size));

		W_DO(options->add_option("svas_mount_table_size", "integer", 
			"20", "# entries in volume mount table",
			false, 
			option_t::set_value_long, ShoreVasLayer.opt_mounttab_size));
	}
	return RCOK;
}

svas_layer::svas_layer(ss_m *sm) 
{ 
	// here do things that require option values

	if (svas_layer_init::initialized++ >0) return; 

	ShoreVasLayer.initialize();


	ShoreVasLayer.cacheRootObj(); // relies on Serials being constructed
	ShoreVasLayer.Sm = sm;

	DBG(<<"ShoreVasLayer.SymlinksMax = " << ShoreVasLayer.SymlinksMax);

	if(sm->config_info(ShoreVasLayer.SmInfo)) {
		ShoreVasLayer.catastrophic("Cannot initialize the storage manager (2).");
	}

	{
		int x = atoi(ShoreVasLayer.opt_mounttab_size->value());
		if(x <= 0 || x > 65535) {
			catastrophic("Bad value for mount table size option.");
		} else  {
			init_mount_table(x);
		}
	}

	catchSignals();

	ShoreVasLayer.nfs_service = 0;
	ShoreVasLayer.mount_service = 0;
	ShoreVasLayer.client_service = 0;
	ShoreVasLayer.remote_service = 0;
	ShoreVasLayer.shell_service = 0;

	if(!CLTAB) {
		CLTAB = new cltab_t; // construct it
	}
	if(!CLTAB) {
		catastrophic( "Cannot init client table." );
	}

	DBG(<<"CLTAB constructed");

	// set all the booleans
	bool bad=0;

	INITBOOL(nfsd_pmap,opt_nfsd_pmap,true);
	INITBOOL(mountd_pmap,opt_mountd_pmap,true);
	INITBOOL(remote_pmap,opt_remote_pmap,true);
	INITBOOL(client_pmap,opt_client_pmap,true);

	INITBOOL(reuseaddr,opt_reuseaddr,false);

	INITBOOL(rpc_unregister,opt_rpc_unregister,false);
	INITBOOL(no_shm,opt_no_shm,false);
	INITBOOL(_pusererrs,opt_print_user_errors,false);
	INITBOOL(make_tcl_shell,opt_make_tcl_shell,true);

}

w_rc_t
svas_layer::start_services()
{
	FUNC(svas_layer::start_services);
	_service::func _func;

	// for error messages
	char b[200];
	ostrstream msg(b, sizeof(b));
	w_rc_t	rc;

	dassert(CLTAB != 0);

	// We have to set the root. 
	// We have to create a vas instance just for mounting
	// a volume for the root of the filesystem.
	// It's kind of grotty but... oh well.
	//
	if(ShoreVasLayer.opt_root_volume!=0 &&
		ShoreVasLayer.opt_root_volume->value() != 0) {
		bool		ok=true;
		lvid_t		volid;
		char 		devicename[
			strlen(ShoreVasLayer.opt_root_volume->value())+1];

		{
			DBG(<<"parsing " << ShoreVasLayer.opt_root_volume->value());
			(void) strcpy(devicename, ShoreVasLayer.opt_root_volume->value());
#define 	DEVSEP ' '
			char *p = strchr(devicename,DEVSEP);
			if(!p) {
				ShoreVasLayer.logmessage("Syntax error in option \"svas_root\"");
				return RC(SVAS_BadParam1);
			} 
			*p = '\0';
			p++;
			char *whole = p;
			p = strchr(whole,'.');
			if(p) {
				*p = '\0';
				p++;
				volid.high = strtol(whole,0,0);
				volid.low = strtol(p,0,0);
			} else {
				volid.high = 0;
				volid.low = strtol(whole,0,0);
			}
	   		DBG(<<"mounting root on " << volid );
		}

		svas_server *temp=0;

		temp = new svas_server((client_t *)0, 
			ShoreVasLayer.client_service->log );

		if(!temp) {
			return RC(SVAS_MallocFailure);
		}

		if(temp->status.vasresult != SVAS_OK) {
			rc =  RC(temp->status.vasreason);
		} else {
			DBG(<<"starting temp session user=" 
				<< ShoreVasLayer.ShoreUser
				<< " uid=" << ShoreVasLayer.ShoreUid
				<< " gid=" << ShoreVasLayer.ShoreGid
				);
			temp->startSession(
				ShoreVasLayer.ShoreUser,
				ShoreVasLayer.ShoreUid,
				ShoreVasLayer.ShoreGid,
				::same_process);
			// don't attach to anything

			DBG(<<"started temp session, temp =" << ::hex((unsigned int)temp)); 

			if(temp->status.vasresult != SVAS_OK) {
				rc =  RC(temp->status.vasreason);
			} else {

				(void) temp->suppress_p_user_errors();

				DBG(<<"about to serve " << devicename );
				if(temp->serve(devicename) != SVAS_OK) {
					rc = RC(SVAS_NotMounted);

					msg << endl << "**********" << endl;
					msg << "Cannot serve " << devicename << endl;
					msg << "You must format it and try again. "  << endl;
					msg << "**********" << endl << ends;
					ShoreVasLayer.logmessage(msg.str(), rc);
					(void) temp->un_suppress_p_user_errors();
				}
				if(!rc) {
					DBG(<<"about to mount " << volid );
					if(temp->mount(volid, "/", true) != SVAS_OK) {
						msg << "Cannot mount root on volume " 
							<< volid << "\n\t"
							<< " (from the following value of "
							<< "option svas_root : "
							<< ShoreVasLayer.opt_root_volume->value()
							<< ")" << endl << ends;
						rc = RC(SVAS_NotMounted);
						ShoreVasLayer.logmessage(msg.str(), rc);
					} else {
						// have to do anything with volume id?
						DBG(<<"Mounted " << volid << " on /" );
					}
				}

				//////////////////////////////////////////
				// serve the rest of the devices
				// given in svas_serve options
				//////////////////////////////////////////

				DBG(<<"about to serve devices");

				rc = svas_layer_init::serve_devices(temp);

				if(rc) {
					msg << "Please check your configuration file "
						<< ShoreVasLayer.configuration_file_name()
						<< ends;
					ShoreVasLayer.logmessage(msg.str());
				}

				(void) temp->un_suppress_p_user_errors();
			}
		}
		delete temp;
		DBG(<<"temp session gone.");
	} else {
		rc = RC(SVAS_NotMounted);
	}

	w_rc_t e;
#define START(x)\
	if(x!=NULL) { \
		DBG(<<"starting " << #x);\
		_func = x->__start;\
		DBG(<<"func is at " << (unsigned int)_func);\
		if(e = ((*_func)((void *)x))) return e; \
		DBG(<<"started " << #x);\
	}

	DBG(<<"svas_layer::start_service");
	START(ShoreVasLayer.mount_service)
	START(ShoreVasLayer.client_service)
	START(ShoreVasLayer.remote_service)
	START(ShoreVasLayer.nfs_service)
	START(ShoreVasLayer.shell_service)
#undef START
	DBG(<<"svas_layer::start_service: all services started");
	if((const void *)rc && (rc.err_num() == SVAS_NotMounted)) {
		msg << "Warning: Server is starting without a root directory." << endl << ends;
		ShoreVasLayer.logmessage(msg.str());
	}
	return RCOK;
}

svas_layer::~svas_layer()
{
	FUNC(svas_layer::~svas_layer);
	w_rc_t e;
	e = ShoreVasLayer.shutdown_filehandlers();
	if(e)  {
		ShoreVasLayer.catastrophic("Cannot shut down filehandlers", e);
	}

#define CLEANUP(x)\
	if(x!=NULL) { if(e = (*(x->__cleanup)) ((void *)x)) return; }

	CLEANUP(ShoreVasLayer.mount_service)
	CLEANUP(ShoreVasLayer.client_service)
	CLEANUP(ShoreVasLayer.remote_service)
	CLEANUP(ShoreVasLayer.nfs_service)
	CLEANUP(ShoreVasLayer.shell_service)
#undef CLEANUP

	ShoreVasLayer.uncacheRootObj();
	finish_mount_table();
}

w_rc_t
svas_layer_init::shutdown_filehandlers() 
{
	FUNC(svas_layer_init::shutdown_filehandlers);
	// dassert(me()->id == 0); // main thread
	// Not true anymore, since Bolo changed threads
	// start/end idiom
	// 
	w_rc_t e;

	e = shutdown_listeners();
	if(e) catastrophic("Cannot shut down listeners", e);

	/////////////////////////////////////////////////////
	// services have been shut down, meaning that no more
	// clients can connect; now we have to get rid
	// of the clients that are already connected and
	// not yet shut down...
	/////////////////////////////////////////////////////

	if(CLTAB) {
		CLTAB->shutdown_all();
	}

#	ifdef DEBUG
	if(CLTAB) {
		// tell us if any file handlers are active
		// ... shouldn't be any...
		int 		i;
		client_t	*c;

		for (i = OpenMax; i >= 0; i--) {
			if(c = CLTAB->find(i)) {
				dassert(c->is_down());
			}
			// is_active means the file handler exists,
			// but it could be shut down
			//
			if (sfile_read_hdl_t::is_active(i) ) {
				DBG( << "file handler is still exists (is_active()) for sock " << i);
			}
		}
	}
#	endif

	DBG( << "file handlers are destroyed ..." )
	// let all the threads to go away
	me()->yield();
	if(CLTAB) {
		int k;
		while((k = CLTAB->count()) > 0) {
			DBG(<< k << "clients still have to self-destruct; yielding");
			me()->yield();
		}
	}

	delete CLTAB;
	CLTAB = 0;
	DBG( << "CLTAB gone ...")

	/* every vas-related thread except main is now shut down */
	// dassert(me()->id == 0); // main thread
	return RCOK;
}

w_rc_t
svas_layer_init::shutdown_listeners() 
{
	FUNC(svas_layer_init::shutdown_listeners);
    /* do not accept any more connections */

	w_rc_t e;
#define SHUTDOWN(x)\
	if((x!=NULL) && (x->__shutdown != NULL))\
		{ if(e = (*(x->__shutdown))((void *)x)) return e; }

	SHUTDOWN(ShoreVasLayer.mount_service)
	SHUTDOWN(ShoreVasLayer.client_service)
	SHUTDOWN(ShoreVasLayer.remote_service)
	SHUTDOWN(ShoreVasLayer.nfs_service)
	SHUTDOWN(ShoreVasLayer.shell_service)
#undef SHUTDOWN

	return RCOK;
}

void
svas_layer::pconfig(ostream &out)
{
	ShoreVasLayer.pconfig(out);
}
void
svas_layer_init::pconfig(ostream &out)
{
	out << "["
		<< "page =" << ShoreVasLayer.SmInfo.page_size << "B"
		<< "; sm obj <= "  << ShoreVasLayer.SmInfo.max_small_rec << "B"
		<< "; lg obj " <<  ShoreVasLayer.SmInfo.lg_rec_page_space << "B/page" 
		<< "; buffer pool=" << ShoreVasLayer.SmInfo.buffer_pool_size << "KB"
		<< "]"
		<< endl;
}

option_t			*
svas_layer_init::svas_shellrc() const {	return ShoreVasLayer.opt_shellrc; }
option_t			*
svas_layer_init::svas_tcllib() const {	return ShoreVasLayer.opt_tcllib; }
int     			
svas_layer_init::sm_page_size() const 
{	return ShoreVasLayer.SmInfo.page_size; }
int     			
svas_layer_init::sm_lg_data_size() const
{	return ShoreVasLayer.SmInfo.lg_rec_page_space; }
int					
svas_layer_init::sm_sm_data_size() const
{	return ShoreVasLayer.SmInfo.max_small_rec; }
bool					
svas_layer_init::pusererrs() const
{	return ShoreVasLayer._pusererrs; }
option_group_t		 *
svas_layer_init::option_group()  { return options; }

void				
svas_layer_init::catastrophic(const char *msg, w_rc_t &x) {
	logmessage(msg,x);
	exit(2); // catastrophic()
}

void				
svas_layer_init::logmessage(const char *msg, w_rc_t &x) {
	if(log) {
		if(x) {
			log->clog << error_prio << x << ":" << ends;
		}
		log->clog << msg << flushl;
	} else {
		if(x) {
			cerr << x << ":" << ends;
		}
		cerr << msg << endl;
	}
}
