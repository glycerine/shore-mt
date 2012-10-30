/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __SVAS_LAYER_H__
#define __SVAS_LAYER_H__
/*
 * $Header: /p/shore/shore_cvs/src/vas/client/svas_layer.h,v 1.13 1995/09/01 21:15:04 nhall Exp $
 */

#include <copyright.h>
#include <w.h>
#include <option.h>
#include <errlog.h>
#include <externc.h>

BEGIN_EXTERNCLIST
	void cleaup_pwstuff();
	void cleaup_netdb();
END_EXTERNCLIST

class svas_layer {
public:
	static w_rc_t setup_options(option_group_t*);
	static w_rc_t option_value(const char *name, const char**val);
};

#ifndef __PROCESS_OPTIONS_C__
// just to be sure that process_options.C doesn't
// depend on any of this internal junk

extern "C" void	svas_set_shellrc(option_t *);
class svas_layer_init {
	friend				void svas_set_shellrc(option_t *);
public:	
	option_t			*opt_shellrc; 
	option_t			*opt_tcllib; 

	friend class svas_layer;
	friend class svas_client;

private:
	static int initialized;
	int     _sm_page_size, _sm_lg_data_size, _sm_sm_data_size;

protected:
	option_group_t		*options;

public:
	option_group_t		*option_group(); 
	ErrLog				*log;
	option_t			*opt_host; 
	option_t			*opt_port;
	option_t			*opt_log;
	option_t			*opt_shm_small_obj; 
	option_t			*opt_shm_large_obj; 
	option_t			*opt_audit; 
	option_t			*opt_print_user_errors; 
	option_t			*opt_use_pmap; 
	bool				_pusererrs;
	bool				do_audit;
	bool				use_pmap;

public:
	svas_layer_init();
	~svas_layer_init();

	// not inlined because server and client have different
	// implementations of this object and the interpreter
	// must generate code to get this info:
	//
	int     sm_page_size()const ;
	int     sm_lg_data_size()const ;
	int		sm_sm_data_size()const ;
	option_t			*svas_shellrc()const ;
	option_t			*svas_tcllib()const ;
	bool	pusererrs() const;
	static void init_Boolean(bool &, option_t *&, bool);
};

#define INITBOOL(varname,optname,defaultval)\
	svas_layer_init::init_Boolean(ShoreVasLayer.varname, ShoreVasLayer.optname, defaultval)

extern svas_layer_init ShoreVasLayer;
#endif /*__PROCESS_OPTIONS_C */
#endif /*__SVAS_LAYER_H__*/
