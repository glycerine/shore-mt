/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/interp.C,v 1.29 1995/10/12 16:53:02 nhall Exp $
 */
#include <copyright.h>

#include <errno.h>
#include <stdio.h>
#include <stream.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
//#include <bzero.h>
#include <unistd.h>
#include <assert.h>
#include <server_stubs.h>
#include <interp.h>

/* GAK */
#include "../client/svas_layer.h" 

#ifndef TRUE
#define TRUE 		1
#endif
#ifndef FALSE
#define FALSE 		0
#endif

int					LineNumber = 0;
int					verbose = 0;
int					expecterrors = 0;
unsigned int 		my_uid = 0;
unsigned int 		my_gid = 0;
int 				sm_page_size; 
int 				sm_lg_data_size;
int 				sm_sm_data_size;

// defined in {client,server}/vas.C
#include <vas_types.h>
int					dirent_overhead = sizeof(struct _entry);

static				interpreter_t *_the_only_=0;

// These are the variables that can be imported & exported
static struct varinfo {
	char 		*vname;
	int			dt;
	void 		*addr;
} choices[] = {
	{	"linenumber", TCL_LINK_INT | TCL_LINK_READ_ONLY, &LineNumber },
	{	"verbose", TCL_LINK_BOOLEAN, &verbose },
	{	"expecterrors", TCL_LINK_BOOLEAN , &expecterrors},
	{	"pagesize", TCL_LINK_INT , &sm_page_size},
	{	"lgdatasize", TCL_LINK_INT , &sm_lg_data_size},
	{	"smdatasize", TCL_LINK_INT , &sm_sm_data_size},
	{	"direntoverhead", TCL_LINK_INT , &dirent_overhead},
	{	"uid", TCL_LINK_INT | TCL_LINK_READ_ONLY, &my_uid},
	{	"gid", TCL_LINK_INT | TCL_LINK_READ_ONLY, &my_gid},
	{	NULL,  0 }
};

//
// Tcl's result is stashed in the Tcl_Interp; this is
// the function that prints the result on the screen
void
interpreter_t::print_result(
	ostream &o,
	int result
)
{
	DBG( << "about to print result");
	bool errorInfoWorked = FALSE;

	if (result == TCL_ERROR) {
		o << "TCL ERROR :  ";
	} 
	if(_interp==0)  {
		return;
	}

	// print the result, whether in error or not
	if(_interp->result && (strlen(_interp->result) > 0) ) {
		 o << "==>" << _interp->result << endl;
	}

	// if error, do additional stuff
	if (result == TCL_ERROR) {
		char *msg;

		Tcl_AddErrorInfo(_interp, ""); // just to be sure we don't
							// get an error in the following:

		result = Tcl_Eval(_interp, "set errorInfo");
		if(result == TCL_OK) {
			if(strlen(_interp->result)>0) {
				msg = strstr(_interp->result, "while executing");
				if(msg)  {
					errorInfoWorked = TRUE;
					o << "\nError occurred " << msg << " ";
				}
			}
		} else {
			o 	<< "<could not get errorInfo: " 
					<< _interp->result << ">";
		}
		o << endl;
		Tcl_ResetResult(_interp);

		if( errorInfoWorked == FALSE) {
			o << "\nError occurred in command " << _linebuf << endl;

			result = Tcl_Eval(_interp, "info level");
			if(result == TCL_OK) {
				if(atoi(_interp->result)>0) {
					o << "\tat nested command level " << _interp->result << ", ";
				}
			} else {
				o 	<< "\t<could not get command nesting level: " 
						<< _interp->result << ">";
			}
			Tcl_ResetResult(_interp);
		}

		o << "\tat lineno " << LineNumber  << ", ";

		result = Tcl_Eval(_interp, "_info script");
		if(result == TCL_OK) {
			o << "file " 
				<< (strlen(_interp->result)==0? "stdin":_interp->result);
		} else {
			o << "<could not get name of script file: " 
				<< _interp->result << ">" ;
		}
		o << endl;
	}
	Tcl_ResetResult(_interp);
	DBG( << "done printing result");
}

#include <unix_stats.h>
unix_stats _shell_rusage;
unix_stats &shell_rusage = _shell_rusage;

interpreter_t::interpreter_t(FILE *f,  void *vas)
	 : _vas(vas), 
	   _file(f), _file_callback(0), _file_cookie(0),
	   _linebufsize(LINELENGTH<<5), line_number(0),
		_self_destruct(0)
{
	FUNC(interpreter_t::interpreter_t);

	shell_rusage.start();
	_linebuf = new char[_linebufsize];

	my_uid = (unsigned int) geteuid();
	my_gid = (unsigned int) getegid();

	_interp = Tcl_CreateInterp();

	/* TODO: REPLACE WITH LIST */ _the_only_ =  this;

	makecmd(
		"if [file exists [info library]/init.tcl] {source [info library]/init.tcl}"
	);
	if(eval()!=TCL_OK) goto bad;

	// NB: the "file" command gets moved to _file in init_shell()
	init_shell(_interp, this);

	// these never change, fortunately
	sm_page_size = ShoreVasLayer.sm_page_size();
	sm_sm_data_size = ShoreVasLayer.sm_sm_data_size();
	sm_lg_data_size = ShoreVasLayer.sm_lg_data_size();

	// Must be done after shell init:
	if(ShoreVasLayer.svas_tcllib() !=0) {
		char *tcllib = (char *)ShoreVasLayer.svas_tcllib()->value();

		makecmd("if [_file exists ");
		appendcmd(tcllib);
		appendcmd("] {source ");
		appendcmd(tcllib);
		appendcmd("}\n");

		if(eval()!=TCL_OK) goto bad;
	}

	{
		struct varinfo 		*c, *cp = NULL;

		Tcl_ResetResult(_interp);

		for(c = choices; c->dt != 0; c++) {

			DBG( << "calling Tcl_LinkVar(" 
				<< c->vname
				<< " " 
				<< ::hex((unsigned int)c->addr) << ")"
			);

			if(Tcl_LinkVar(_interp, 
				c->vname, (char *)c->addr, c->dt) != TCL_OK) {
				Tcl_AppendResult(_interp,
					"Error in Tcl_LinkVar(" ,
					c->vname, " ",  ::hex((unsigned int)c->addr),
					" ", c->dt, 0);
				goto bad;
			}
		}
	}

	(void) Tcl_SetVar(_interp, "tcl_interactive", (isatty(0)?"1":"0"),
		TCL_GLOBAL_ONLY);

	// call the startup function -- client side leaves
	// it up to the vas.rc or client to run the command "vas ..."
	// whereas the server side does it automatically

	Tcl_ResetResult(_interp);

	return;

bad:
	DBG(<< "");
	print_result(cerr, TCL_ERROR);
	DBG(<< "");
}

interpreter_t::~interpreter_t()
{
	FUNC(~interpreter_t);

	cout << "Bye (Tcl shell going away...)" << endl;
	{
		struct varinfo 		*c, *cp = NULL;

		Tcl_ResetResult(_interp);

		for(c = choices; c->dt != 0; c++) {
			DBG( << "calling Tcl_UnlinkVar(" << c->vname << ")");
			(void) Tcl_UnlinkVar(_interp, c->vname);
		}
	}
	if(_interp) {
		Tcl_ResetResult(_interp);
		finish_shell(_interp);
		Tcl_DeleteInterp(_interp);
		_interp = 0;
	}
	delete[] _linebuf;

}

#include <svas_error_def.h>
#include <os_error_def.h>

w_rc_t
interpreter_t::startup()
{
	int res;
	Tcl_ResetResult(_interp);
	if(ShoreVasLayer.svas_shellrc() !=0) {
		char *vasrc = (char *)ShoreVasLayer.svas_shellrc()->value();

		if(vasrc) {
			if(strlen(vasrc)<1) {
				Tcl_AppendResult(_interp,
				"Bad (null) file name given for run-commands file.",
				0);
				res = SVAS_BadFileNameSyntax;
				goto bad;
			}
			makecmd( "if [_file exists " );
			appendcmd(vasrc);
			appendcmd( "] { source " );
			appendcmd(vasrc);
			appendcmd("}\n");

			cout << "Looking for run command file \"" << 
					vasrc << "\"..." << endl;

			if(eval() != TCL_OK) {
				DBG(<< "");
				Tcl_AppendResult(_interp, 
					"\nError occurred reading ", vasrc, 0); 

				// TODO: figure out how to propagate the
				// correct error code up
				res = SVAS_FAILURE;
				goto bad; // after all, it's fatal.
			}
			cout << "Done reading " << vasrc << "." << endl;
		} 
		// could have run "quit" from the script!
		if(_self_destruct) {
			return RC(SVAS_ABORTED); // overloaded!
		}
	}
	return RCOK;
bad:
	DBG(<< "");
	print_result(cerr, TCL_ERROR);
	DBG(<< "");
	return RC(res);
}

/* command to print linked variables */
extern "C" int cmd_variables(ClientData, Tcl_Interp* ip, int ac, char* av[]);

int
cmd_variables(ClientData, Tcl_Interp* ip, int ac, char* av[])
{
	struct varinfo 		*c, *cp = NULL;

	for(c = choices; c->dt != 0; c++) {
		Tcl_AppendResult(ip, c->vname, " ", 0);
	}
	return TCL_OK;
}

static bool prompt_off = FALSE;
void
noprompt()
{
	DBG(<<"noprompt");
	prompt_off = TRUE;
}

void
prompt()
{
	DBG(<<"prompt");
	if(!prompt_off) {
		cout << "(" << LineNumber << ")% " << flush;
	}
}

void	
interpreter_t::expandbuf() 
{ // realloc the buf
	// get two more lines' worth of space

	char	*savebuf = _linebuf;
	_linebuf = new char[_linebufsize + (LINELENGTH<<1)];
	memcpy(_linebuf, savebuf, _linebufsize);
	_linebufsize += (LINELENGTH<<1);
	_left += (LINELENGTH<<1);
	delete[] savebuf;

}
void 	
interpreter_t::appendcmd(char *str) 
{ 
	int l = strlen(str);
	strncpy(_cur, str, l);
	_left -= l;
	_cur += l;
	*_cur = 0;
}

int		
interpreter_t::eval() 
{
	int res;
	DBG(<<"eval:" << _linebuf);

	// don't wipe out prev results! // Tcl_ResetResult(_interp);

	if(!Tcl_CommandComplete(_linebuf)) {
		DBG(<<"incomplete command:" << _linebuf);
		Tcl_AppendResult(_interp, "Incomplete command :", 
			_linebuf, 0);
		return TCL_CONTINUE;
	} else {
		DBG(<<"evaluating " << _linebuf);
#ifndef notdef
		/* use history TODO: option */
		res = Tcl_RecordAndEval(_interp, _linebuf, 0);
#else
		/* do not use history */
		res = Tcl_Eval(_interp, _linebuf);
#endif 
	}
	init(); // avoid duplicate evals
	DBG(<<"eval:" << _linebuf);
	return res;
}

void	
interpreter_t::readline() 
{
	if (_file_callback && _file_cookie) {
		w_rc_t e = (*_file_callback)(_file_cookie);
		if (e) {
			cerr << "GACK file callback wait in readline:" << endl << e << endl;
			return;
		}
	}

	clearerr(_file);
	(void) fgets(_cur, _left, _file);

	line_number++;

	int len = strlen(_cur);
	_cur += len;
	dassert(*_cur == 0);
	_left -= len;

	// will there be enough room to read more-or-less,
	// a standard whole screen line?

	if(_left < 80) {
		expandbuf();
	}
}

int
interpreter_t::consume()
{
	int 		result;
	init();
	while (1) {
		// we get here only because the file handler unblocked
		// us, hence we already have keyboard activity
		LineNumber++;
		readline();

		if(feof(_file)) {
			cout << "^D -- Bye\n" << endl;
			appendcmd(" bye");
			// quit_shell(_interp);
			// assert(0); // should not return here
			// break;
		}

		result = eval();
		if(result != TCL_CONTINUE) {
			/* evaluating the command could have destroyed the interp */
			break; // out and return
		}
	}
	DBG( << "interpreter_t::consume returning " << !_self_destruct);
	if(_self_destruct) {
		// return 0 if done (self_destruct)
		return 0;
	} else {
		DBG(<< "");
		print_result(cout, result );
		prompt();
		// return 1 if caller should continue (!self_destruct)
		return 1;
	} 
}

void	
interpreter_t::destroy(Tcl_Interp *ip)
{
	/* HACK TODO: FIX */
	assert(_the_only_-> _interp == ip);
	_the_only_->_self_destruct = 1 ;
}

