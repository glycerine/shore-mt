/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Header: /p/shore/shore_cvs/src/vas/common/shell.C,v 1.35 1996/03/04 16:17:45 nhall Exp $
 */
#include <copyright.h>

#include "shell.misc.h"
#include "interp.h"
#include "server_stubs.h"

#define VASPTR svas_base *

#ifdef USE_VERIFY
verify_t *v;
#endif

class dummy_tclout _dummy;

int 
cmdreply(ClientData clientdata, Tcl_Interp *ip, CMDResult q, const char *c) 
{
	int res = (q==tcl_ok)?TCL_OK : TCL_ERROR;

	DBG(<<c << " replying with " << res);
	if(statistics_mode & (s_autoprint | s_autoclear)) {
		(void) print_statistics(clientdata, ip, statistics_mode);
	}
	yield();
	dassert(!tclout.bad());
	return res;
}

CMDResult
SyntaxError(Tcl_Interp 	*ip, 
	char		*msg, 
	char		*info
)
{
	FUNC(SyntaxError);
	Tcl_AppendResult(ip, "Syntax error:", msg, info, 0);
	dassert(!tclout.bad());
	yield();
	return tcl_error;
}

int 
check(ClientData clientdata,
	Tcl_Interp* ip, int ac, int n1, int n2, char* s)
{
	FUNC(check);
	dassert(!tclout.bad());

	yield();
	if(n2 < n1) n2=n1;

	if ((ac < n1) || (ac > n2)) {
		Tcl_AppendResult(ip, ::form("Wrong # arguments: need %d ",n1-1), 0);
		if(n2>n1) {
			Tcl_AppendResult(ip, ::form("to %d",n2-1),0);
		}
		Tcl_AppendResult(ip,  " arg(s); got ", ::form("%d",ac-1), ".", 0);

		if (s)  {
			Tcl_AppendResult(ip,  "\nShould be \"", s, "\"", 0);
		} 
		return TCL_ERROR;
	}
    CMDREPLY(tcl_ok);
}

void Tcl_AppendSerial( Tcl_Interp *ip, int res, const serial_t &ser)
{
	FUNC(TclAppendSerial);
	if(res == SVAS_OK) {
		tclout.seekp(ios::beg);
		dassert(!tclout.bad());
		tclout << ser << " " << ends;
		dassert(!tclout.bad());
		Tcl_AppendResult(ip, tclout.str(), 0);
	}
}
void Tcl_AppendLVid( Tcl_Interp *ip, int res, const lvid_t &vid)
{
	FUNC(TclAppendLVid);
	if(res == SVAS_OK) {
		tclout.seekp(ios::beg);
		dassert(!tclout.bad());
		tclout << vid << " " << ends;
		dassert(!tclout.bad());
		Tcl_AppendResult(ip, tclout.str(), 0);
	}
}
void Tcl_AppendLoid( Tcl_Interp *ip, int res, const lrid_t &loid)
{
	FUNC(TclAppendLoid);
	if(res == SVAS_OK) {
		tclout.seekp(ios::beg);
		dassert(!tclout.bad());
		tclout << loid << " " << ends;
		dassert(!tclout.bad());
		Tcl_AppendResult(ip, tclout.str(), 0);
	}
}

int _atoi(const char *c)
{
	dassert(!tclout.bad());
	yield();
	return strtol(c, (char **)NULL, 0);
}

RequestMode	getRequestMode(Tcl_Interp* ip, char *p) 
{
	FUNC(getRequestMode);
	int len = strlen(p);
	if(strncasecmp(p, "Blocking", len) == 0) {
		return Blocking;
	} else if(strncasecmp(p, "NonBlocking",len) == 0) {
		return NonBlocking;
	}
	Tcl_AppendResult(ip, "Unknown RequestMode:", p, " (using Blocking)", 0);
	return Blocking;
}

LockMode	getLockMode(Tcl_Interp* ip, char *p) 
{
	FUNC(getLockMode);
	if(strcasecmp(p, "NL") == 0) {
		return NL;
	} else if(strcasecmp(p, "IS") == 0) {
		return IS;
	} else if(strcasecmp(p, "IX") == 0) {
		return IX;
	} else if(strcasecmp(p, "SH") == 0) {
		return SH;
	} else if(strcasecmp(p, "SIX") == 0) {
		return SIX;
	} else if(strcasecmp(p, "UD") == 0) {
		return UD;
	} else if(strcasecmp(p, "EX") == 0) {
		return EX;
	}
	Tcl_AppendResult(ip, "Unknown LockMode:", p, " (using SH)", 0);
	return SH;
}

int get_vid(
	// Tcl_Interp* ip, 
	void *v, const char *av, lvid_t &target
)
{
	// handle all these cases:
	// a.b.c.d:e (that's the lvid_t's << and >> way
	// high.low 
	// low (implies high == 0)

	FUNC(get_vid);

	const char *colon =  strchr(av,':');

	if(colon) {
		istrstream(av, strlen(av)) >> target;
		DBG(<<"vid parsed by lvid_t class");
	} else {
		// how many dots?
		const char *dot = strchr(av,'.');
		if(dot) {
			if(strchr(dot, '.')) {
				DBG(<<"too many dots");
				return TCL_ERROR; // syntax error
			}
			target.high = _atoi(av);
			target.low = _atoi(dot);
		} else {
			target.high = 0;
			target.low  = _atoi(av);
		}
	}
	DBG(<<av << " maps to " << target);
	return TCL_OK;
}

int getCompareOp(ClientData clientdata,
	Tcl_Interp* ip, char *av, CompareOp &target)
{
	FUNC(getCompareOp);
	if(strcmp(av, "<=")==0) {
		target = leOp;
	} else if(strcmp(av, ">=")==0) {
		target = geOp;
	} else if(strcmp(av, "==")==0) {
		target = eqOp;
	} else if(strcmp(av, "=")==0) {
		target = eqOp;
	} else if(strcmp(av, ">")==0) {
		target = gtOp;
	} else if(strcmp(av, "<")==0) {
		target = ltOp;
	} else{
		SYNTAXERROR(ip, "Unknown compare op", av);
	}
	DBG(<<"compare op is " << target);
	yield();
	return SVAS_OK;
}

/**************************************************************************/

static int 
find_command(const char *str)
{
	FUNC(find_command);
	register int i;
	for(i=0; i<__num_commands__; i++) {
		if(strncmp(str,_commands[i].name, strlen(_commands[i].name))==0) {
			return i;
		}
	}
	for(i=0; _aliases[i].name ; i++) {
		if(strncmp(str,_aliases[i].name, strlen(_aliases[i].name))==0) {
			return _aliases[i].cmdnum;
		}
	}
	return -1;
}

static int 
next_keyword(int cmd, int lastkwdindex= -1)
{
	FUNC(next_keyword)
	register int i = lastkwdindex;
	char	*x = ::form(";%d",cmd);
	char 	*y;

	DBG("cmd= " << cmd << " i=" << i << " looking for " << x);

	for(++i; _keywords[i].word!=0 ; i++) {
		if( (y=strstr(_keywords[i].helpindex, x)) &&
			(atoi(y+1)==cmd) ) {
				DBG("returns i " << i << _keywords[i].word
				<< " " << _keywords[i].helpindex
				<< " y=" << y
				);
			return i;
		}
	}
	return -1;
}

static int 
find_keyword(const char *str)
{
	FUNC(find_keyword);
	register int i;
	for(i=0; _keywords[i].word!=0 ; i++) {
		if(strncmp(str,_keywords[i].word, strlen(_keywords[i].word))==0) {
			return i;
		}
	}
	return -1;
}

int 
append_usage_cmdi(Tcl_Interp *ip, int i)
{
	FUNC(append_usage_cmdi);
	DBG(<<"i= " << i);

	if(i < 0 || i > __num_commands__) {
		return TCL_ERROR;
	}
	Tcl_AppendResult(ip, _commands[i].name, " ", _usage[i], "\n", 0);
	return TCL_OK;
}

int append_usage(Tcl_Interp *ip, const char *str)
{
	FUNC(append_usage);
	int i;

	i = find_command(str);
	if(i>=0) {
		return append_usage_cmdi(ip, i);
	} else {
		return TCL_ERROR;
	}
}

static void
append_help_cmdi(Tcl_Interp *ip, int i)
{
	FUNC(append_help_cmdi);
	int from, to;

	(void) append_usage_cmdi(ip, i);

	from = _help[i].from; to = _help[i].to;
	DBG(<<"from= " << from << " to=" << to);
	for(i=from; i <= to; i++) {
		Tcl_AppendResult(ip, _help_txt[i], "\n", 0);
	}
}

static int
append_help_keyi(Tcl_Interp *ip, int i)
{
	FUNC(append_help_keyi);
	int from, to;
	char *list;

	DBG(<<"keyword is " << _keywords[i].word);

	Tcl_AppendResult(ip, "Keyword ", _keywords[i].word, 0);
	list = _keywords[i].helpindex;

	while(list && (*list == ';')) {
		list++;
		if(*list == 0) break;

		i = atoi(list);
		DBG(<<"list= " << list << " i=" << i);

		if(_commands[i].cmdnum != i) {
			dassert(0);
			continue;
		}
		Tcl_AppendResult(ip, "\nCommand ", _commands[i].name," : \n", 0);

		from = _help[i].from; to = _help[i].to;
		DBG(<<"from= " << from << " to=" << to);
		if(from <= to) {
			for(i=from; i <= to; i++) {
				Tcl_AppendResult(ip, _help_txt[i], "\n", 0);
			}
		}
		list = strchr(list,';');
	}
	return TCL_OK;
}

static  int
append_help(Tcl_Interp *ip, const char *str)
{
	FUNC(append_help);
	int i;

	i = find_command(str);
	if(i>=0) {
		append_help_cmdi(ip,i);

		int j = -1;

		/* append keywords */
		Tcl_AppendResult(ip, "Keywords: ", 0);
		while((j = next_keyword(i, j)) >= 0) {
			Tcl_AppendResult(ip, _keywords[j].word, " ", 0);
		}
	} else {
		DBG(<<"not a command or alias");

		/* try keywords */
		i = find_keyword(str);
		if(i>=0) {
			append_help_keyi(ip,i);
		} else {
			return TCL_ERROR;
		}
	}
	return TCL_OK;
}

int
cmd_commands(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
    FUNC(cmd_commands);
    CHECK(1, 1, NULL);

	int i;
	for(i=0; i<__num_commands__; i++) {
		Tcl_AppendResult(ip, _commands[i].name," ", 0);
	}
	return TCL_OK;
}

void 
init_shell(Tcl_Interp* ip, interpreter_t *interp)
{
	FUNC(init_shell);
	struct command *c;

	if(Tcl_Eval(ip, "rename format _format")!=TCL_OK) {
		catastrophic(::form("Fatal error initializing shell: _format."));
	}
	if(Tcl_Eval(ip, "rename info _info")!=TCL_OK) {
		catastrophic(::form("Fatal error initializing shell: _info."));
	}
	if(Tcl_Eval(ip, "rename time _time")!=TCL_OK) {
		catastrophic(::form("Fatal error initializing shell: _time."));
	}
	if(Tcl_Eval(ip, "rename pwd _pwd")!=TCL_OK) {
		catastrophic(::form("Fatal error initializing shell: _pwd."));
	}
	if(Tcl_Eval(ip, "rename cd _cd")!=TCL_OK) {
		catastrophic(::form("Fatal error initializing shell: _cd."));
	}
	if(Tcl_Eval(ip, "rename file _file")!=TCL_OK) {
		catastrophic(::form("Fatal error initializing shell: _file."));
	}
	
	// commands
	for(c = &_commands[0]; c->name != 0;  c++) {
		Tcl_CreateCommand(ip, c->name, c->func, (ClientData)interp, 0);
	}
	// aliases
	for(c = &_aliases[0]; c->name != 0;  c++) {
		Tcl_CreateCommand(ip, c->name, c->func, (ClientData)interp, 0);
	}

	// if this doesn't work, something's really wrong
	if(Tcl_Eval(ip, "set expecterrors 0")!=TCL_OK){
		catastrophic(
		::form("Fatal error initializing shell: set expecterrors 0"));
	}
	if(Tcl_Eval(ip, "set verbose 0")!=TCL_OK){
		catastrophic(::form("Fatal error initializing shell: set verbose 0"));
	}
#ifdef USE_VERIFY
	v = new verify_t();
#endif
}

void quit_shell(Tcl_Interp* ip)
{
	FUNC(quit_shell);

	if(Tcl_Eval(ip, "info args xerrs")==TCL_OK) {
		// the procedure exists!
		 DBG(<<"invoking xerrs");
		 assert(Tcl_Eval(ip, "xerrs")==TCL_OK);
	}

	// locate me among all the interpreters 
	interpreter_t::destroy(ip);
}

// called by interpreter
void finish_shell(Tcl_Interp* ip)
{
	FUNC(finish_shell);
	struct command *c;

#ifdef USE_VERIFY
	delete v;	
#endif

	for(c = &_commands[0]; c->name;  c++) {
		Tcl_DeleteCommand(ip, c->name);
	}
	DBG(<< "Shell commands gone");
}

int
_get_mode(
	ClientData clientdata,
	Tcl_Interp* ip, int ac, char* av[],
	int imode, 
	mode_t *mode
)
{
	FUNC(_get_mode);
	DBG(<<"ac == " << ac
		<< "imode == " << imode
	);
	if((ac==(imode+1)) && (av[imode]!=0) ) {
		*mode = _atoi(av[imode]);
	}
	return TCL_OK;
}


int
cmd_crash(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	cerr << "Simulated crash." << endl;

	crash(ip);
}

int
cmd_keywords(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
    FUNC(cmd_keywords);
    CHECK(1, 1, NULL);

	int i;
	for(i=0; _keywords[i].word != 0; i++) {
		Tcl_AppendResult(ip, _keywords[i].word, " ", 0);
	}
	return TCL_OK;
}
int
cmd_help(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
    FUNC(cmd_help);
    CHECK(1, 2, NULL);

	if(ac==1) {
		append_help_cmdi(ip, __cmd_help__);
	} else {
		for(int i =1; i<ac; i++) {
			Tcl_AppendResult(ip, av[i], ":\n", 0);
			append_help(ip, av[i]);
		}
	}
	return TCL_OK;
}

#include "sdl_fct.h"
int
cmd_sdltest(ClientData clientdata, Tcl_Interp* ip, int ac, char* av[])
{
	cerr << "sdl app test." << endl;
	sdl_main_fctpt sdl_app= 0;
	if (ac <=1)
	{
		cerr << "no args: nothing to call" << endl;
	}
	else 
	{
		sdl_app = sdl_fct::lookup(av[1]);
		if (sdl_app==0)
			cerr << " no sdl function " << av[1] << " found " <<endl;
		else
		{
			int sdl_rc;
			sdl_rc = (sdl_app)(ac-1,av +1);
			cerr << av[1] << " return value " << sdl_rc <<endl;
		}
	}
	return TCL_OK;
}

