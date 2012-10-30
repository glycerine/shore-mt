/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __SVAS_LAYER_H__
#define __SVAS_LAYER_H__

#include <copyright.h>
#include <vas_internal.h>
#include <option.h>
#include "hdrinfo.h"

#ifdef RPC_HDR
	typedef void * ss_m;
#endif

BEGIN_EXTERNCLIST
	void cleanup_pwstuff();
	void cleanup_netdb();
END_EXTERNCLIST

class rpc_service; //forward
class tclshell_service; // forward
class Directory;// forward
class svas_server;

class svas_layer_init {
public: 
	friend class svas_layer;

private:
	char  				*config_file_name;

protected:
	static int initialized;
	option_group_t		*options;
	void 				set_configuration_file_name();

public:
	option_group_t		*option_group(); 

	rpc_service 	*nfs_service;
	rpc_service 	*mount_service;
	rpc_service 	*client_service;
	rpc_service 	*remote_service;
	tclshell_service 	*shell_service;

	option_t			*opt_mounttab_size;
	option_t			*opt_shellrc;
	option_t			*opt_tcllib;

	/*	ports */
	option_t			*opt_nfsd_port;
	option_t			*opt_mountd_port;
	option_t			*opt_client_port;
	option_t			*opt_remote_port;

	option_t			*opt_reuseaddr;

	option_t			*opt_shell_log;

	/* log files */
	option_t			*opt_nfsd_log;
	option_t			*opt_mountd_log;
	option_t			*opt_client_log;
	option_t			*opt_log;

	/* log levels */
	option_t			*opt_shell_log_level;
	LogPriority			shell_loglevel;
	option_t			*opt_nfsd_log_level;
	LogPriority			nfsd_loglevel;
	option_t			*opt_mountd_log_level;
	LogPriority			mountd_loglevel;
	option_t			*opt_client_log_level;
	LogPriority			client_loglevel;
	option_t			*opt_log_level;
	LogPriority			_loglevel;

	option_t			*opt_print_user_stats;
	option_t			*opt_clear_user_stats;
	option_t			*opt_print_user_errors;
	option_t			*opt_make_tcl_shell;

	option_t			*opt_no_shm;
	option_t			*opt_root_volume;
	option_t			*opt_rpc_unregister;
	option_t			*opt_nfsd_pmap;
	option_t			*opt_mountd_pmap;
	option_t			*opt_remote_pmap;
	option_t			*opt_client_pmap;

	option_t			*opt_sysp_cache_size;

	char				**opt_serve_array;
	int					opt_serve_array_alloced;
	int					opt_serve_array_used;

	bool				rpc_unregister,
			no_shm, _pusererrs, make_tcl_shell,
			nfsd_pmap, mountd_pmap, remote_pmap, client_pmap,
			reuseaddr;

	ErrLog				*log; // the log for the whole layer

public:
	int OpenMax;
	int	FileSizeMax;
	int	LinksMax;
	int	SymlinksMax;
	int	NameMax;
	int	PathMax;

	char	*ShoreUser;	 
	uid_t	ShoreUid;	 
	gid_t	ShoreGid;
	static const gid_t	RootGid;
	static const uid_t	RootUid;
	w_ref_t<swapped_hdrinfo>	RootObj; 

#ifdef RPC_HDR
	// gak-- leave out the SmInfo-- that means 
	// the files that include msg.h cannot use SmInfo
	//
#else
	struct sm_config_info_t SmInfo;
#endif
	ss_m *Sm; 
	/* DO NOT PUT ANY DATA MEMBERS BEYOND HERE */

public:

	const char 			*configuration_file_name() const;

	svas_layer_init(); // for static initialization
	void initialize(); // for dynamic initialization by svas_layer()
	~svas_layer_init();

	// for use  by the shell - cannot inline these
	option_t			*svas_shellrc() const;
	option_t			*svas_tcllib() const;
	int     			sm_page_size() const;
	int     			sm_lg_data_size() const;
	int					sm_sm_data_size() const;
	bool				pusererrs() const;

	w_rc_t shutdown_filehandlers();
	w_rc_t shutdown_listeners();
	void pconfig(ostream &out);
private:
	void 				cacheRootObj(); // in Directory.C
	void 				uncacheRootObj(); // in Directory.C

public:

	static void
	init_Boolean(bool &var, option_t *& opt, bool defaultval);
	w_rc_t set_value_log_level(option_t *opt, const char *value,
									ostream *err_stream);
	static w_rc_t set_value_device_path(option_t *opt, const char *value,
								ostream *err_stream);
	static w_rc_t serve_devices(svas_server *temp);
private:
	w_rc_t _set_value_device_path(option_t *opt, const char *value,
								ostream *err_stream);
	w_rc_t _serve_devices(svas_server *temp);

public:

#define INITBOOL(varname,optname,defaultval)\
	svas_layer_init::init_Boolean(ShoreVasLayer.varname, ShoreVasLayer.optname, defaultval)

	// Note that for catastrophic and logmessage, rather than
	// providing a default value of w_rc_t the methods are overloaded.
	// This is necessary due to a bug in gcc-2.6.2.  It has been
	// fixed in later versions of the compiler.
	void				catastrophic(const char *, w_rc_t & );
	void				catastrophic(const char * c) {
						w_rc_t rcok = RCOK;
						catastrophic(c, rcok);
					}
	void				logmessage(const char *, w_rc_t &);
	void				logmessage(const char * c) {w_rc_t rcok = RCOK; logmessage(c, rcok);}
};

extern svas_layer_init ShoreVasLayer;

class svas_layer {
private:
	/* NO OTHER DATA MEMBERS -- KEEP IT THAT WAY */

protected:
	friend class svas_server;
	friend class Directory;

private:
	void catchSignals();
public:
	static w_rc_t setup_options(option_group_t *);
	static w_rc_t option_value(const char *name, const char**val);

	svas_layer(ss_m *);
	~svas_layer();

	void pconfig(ostream &out);

	w_rc_t start_services();
	w_rc_t configure_client_service();
	w_rc_t configure_remote_service();
	w_rc_t configure_nfs_service();
	w_rc_t configure_mount_service();
	w_rc_t configure_tclshell_service();
};

#endif /*__SVAS_LAYER_H__*/
