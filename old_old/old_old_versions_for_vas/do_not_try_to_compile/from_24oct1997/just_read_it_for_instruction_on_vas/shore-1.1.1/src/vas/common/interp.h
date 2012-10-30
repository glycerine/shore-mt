/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef __INTERP_H__
#define __INTERP_H__
/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/interp.h,v 1.17 1995/10/12 19:58:41 nhall Exp $
 */
#include <copyright.h>
#include <externc.h>
#include <debug.h>
#include <stdio.h>
#include <tcl.h>

class Tcl_Interp;
class interpreter_t;

BEGIN_EXTERNCLIST
	void 	noprompt();
	void 	prompt();

	void 	init_shell(Tcl_Interp* ip, interpreter_t *interp);
	void 	quit_shell(Tcl_Interp *);
	void 	finish_shell(Tcl_Interp *);

END_EXTERNCLIST

extern int 			verbose; // in interp.C
extern int 			expecterrors; // in interp.C
extern int 			sm_page_size; // in interp.C
extern int 			sm_lg_data_size; // in interp.C
extern int 			sm_sm_data_size; // in interp.C

class interpreter_t {
public:
	typedef void  (*vfunc_t)(void*);
	void			*_vas; // svas_base *

private:
	Tcl_Interp 	*_interp;
	FILE 	*_file;
	w_rc_t	(*_file_callback)(void *);
	void	*_file_cookie;
	char	*_linebuf;
	int		_linebufsize;
	char	*_cur;
	int		_left;
	int		_self_destruct;
public:
	int		line_number;
#define LINELENGTH 84
	// start with enough for 32 lines

	interpreter_t(FILE *f, void *vas);
	~interpreter_t();
	w_rc_t	startup(); // to read startup file

	static  void	destroy(Tcl_Interp *ip);
	void	init() 				{ _cur = _linebuf; _left = _linebufsize;
		*_linebuf = 0; }
	void	expandbuf();
	void 	appendcmd(char *str);
	void 	makecmd(char *str) 	{ init(); appendcmd(str); }
	void	readline();
	int		eval();
	void 	print_result(ostream &o, int result);
	int 	consume(); // return 0 if done, 1 if not done
	void	set_callback(w_rc_t (*handler)(void *), void *cookie) {
				_file_callback = handler;
				_file_cookie = cookie;
	}
	Tcl_Interp 	*interp() { return _interp;}
};

#endif

