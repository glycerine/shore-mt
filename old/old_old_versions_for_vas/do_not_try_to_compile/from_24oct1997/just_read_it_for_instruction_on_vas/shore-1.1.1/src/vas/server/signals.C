/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *   $Header: /p/shore/shore_cvs/src/vas/server/signals.C,v 1.22 1997/01/24 16:48:14 nhall Exp $
 */ 
#include <copyright.h>
#include <w_workaround.h>
#include <w_signal.h>
#include <vas_internal.h>
#include <fcntl.h>
#ifdef SOLARIS2
#include "/usr/ucbinclude/sys/ioctl.h"
#else
#include <sys/ioctl.h>
#endif

BEGIN_EXTERNCLIST
#if defined(DEBUG) && defined(DEBUGTHREADS)
	void dumpthreads();
#endif

	void 	sigquit(int); 
	void 	sigcont(int);
	void 	sighup(int);
#ifdef SUNOS41
	int		ioctl(int, ...);
#endif
	void 	dobg(bool);
END_EXTERNCLIST

static bool background=0; // TODO: fix

static pid_t mypg = -1;
static pid_t ppid = -1;
static int devttyfd= -1;

/* whatever is not included here doesn't get touched. */
/* whatever here has SIG_DFL doesn't get touch either */
struct shore_siginfo {
	int	 sig;
	char *string;
	int	 error_type;
	_W_ANSI_C_HANDLER  handler;
} siginfo[] = {								/* termio(4) stty(1) conventional*/
	{ 0,		"<none>", 	0,	 W_ANSI_C_HANDLER SIG_DFL },
	{ SIGHUP,	"SIGHUP", 	0,	 W_ANSI_C_HANDLER sighup },
	{ SIGINT,	"SIGINT", 	0, 	 W_ANSI_C_HANDLER SIG_DFL }, /* INTR, intr,  ^C */
	{ SIGQUIT,	"SIGQUIT",	0,  W_ANSI_C_HANDLER sigquit }, /* QUIT, quit, ^\ */
	{ SIGILL,	"SIGILL",	0,  W_ANSI_C_HANDLER SIG_DFL },
	{ SIGTRAP,	"SIGTRAP",	0,  W_ANSI_C_HANDLER SIG_DFL },
	{ SIGIOT,	"SIGIOT",	0,  W_ANSI_C_HANDLER SIG_DFL },
#ifndef Linux
    	/* linux does not define this */
	{ SIGEMT,	"SIGEMT",	0,  W_ANSI_C_HANDLER SIG_DFL },
#endif
	{ SIGFPE,	"SIGFPE",	0,  W_ANSI_C_HANDLER SIG_DFL },
	{ SIGKILL,	"SIGKILL",	0,  W_ANSI_C_HANDLER SIG_DFL },
	{ SIGBUS,	"SIGBUS",	0,  W_ANSI_C_HANDLER SIG_DFL },
	{ SIGSEGV,	"SIGSEGV",	0,  W_ANSI_C_HANDLER SIG_DFL },
#ifndef Linux
    	/* linux does not define this */
	{ SIGSYS,	"SIGSYS",	0,  W_ANSI_C_HANDLER SIG_DFL } ,
#endif
	{ SIGPIPE,	"SIGPIPE",	0, 	W_ANSI_C_HANDLER SIG_DFL }, /* diskproc exited? */
	{ SIGALRM,	"SIGALRM",	0, 	W_ANSI_C_HANDLER SIG_DFL },
	{ SIGTERM,	"SIGTERM",	0,  W_ANSI_C_HANDLER SIG_DFL },
	{ SIGURG,	"SIGURG",	0,  W_ANSI_C_HANDLER SIG_DFL },
	{ SIGSTOP,	"SIGSTOP",	0,  W_ANSI_C_HANDLER SIG_DFL },
	{ SIGTSTP,	"SIGTSTP",	0,  W_ANSI_C_HANDLER SIG_DFL }, /* SUSP, susp, ^Z */
	{ SIGCONT,	"SIGCONT",	0,  W_ANSI_C_HANDLER sigcont },	/* foreground/background */
	{ SIGCHLD,	"SIGCHLD",	0,  W_ANSI_C_HANDLER SIG_DFL },
	{ SIGTTIN,	"SIGTTIN",	0, 	W_ANSI_C_HANDLER sigcont 			},
	{ SIGTTOU,	"SIGTTOU",	0,  W_ANSI_C_HANDLER SIG_DFL },
	{ SIGIO,	"SIGIO",	0,  W_ANSI_C_HANDLER SIG_DFL },
#if defined(HPUX8) || defined(Linux) || defined (SOLARIS2)
    { SIGPWR,   "SIGPWR",   0,  W_ANSI_C_HANDLER SIG_DFL },
    /* this signal cannot be caught in HP-UX */
    /*{ _SIGRESERVE,    "SIGRESERVE",   0,  W_ANSI_C_HANDLER SIG_DFL },*/
#else
    { SIGXCPU,  "SIGXCPU",  0,  W_ANSI_C_HANDLER SIG_DFL },
    { SIGXFSZ,  "SIGXFSZ",  0,  W_ANSI_C_HANDLER SIG_DFL },
#endif /* HPUX8*/
	{ SIGVTALRM,"SIGVTALRM",0,  W_ANSI_C_HANDLER SIG_DFL },
	{ SIGPROF,	"SIGPROF",	0,  W_ANSI_C_HANDLER SIG_DFL },
	{ SIGWINCH,	"SIGWINCH",	0,  W_ANSI_C_HANDLER SIG_DFL },
#if (!defined(Linux)) && (!defined(SOLARIS2))
    	/* linux does not define this */
	{ SIGLOST,	"SIGLOST",	0,  W_ANSI_C_HANDLER SIG_DFL },
#endif
	{ SIGUSR1,	"SIGUSR1",	0,  W_ANSI_C_HANDLER SIG_DFL },
	{ SIGUSR2,	"SIGUSR2",	0,  W_ANSI_C_HANDLER SIG_DFL }, /* SM uses this */
#ifdef SOLARIS2
	{ SIGPOLL,     "SIGPOLL",     0, W_ANSI_C_HANDLER SIG_DFL },
	{ SIGWAITING,  "SIGWAITING",   0, W_ANSI_C_HANDLER SIG_DFL },
	{ SIGLWP,      "SIGLWP",      0, W_ANSI_C_HANDLER SIG_DFL },
#endif /* SOLARIS2 */
	{ 0, 0, 0, 0}
};

#ifdef notdef
// TODO: remove this ifdef notdef and try bg/fg on the HPUX8, SUNOS41 */
static bool
isbg()
{
	int ctermpg = 0;
	bool result;

	if(background)  /* pretend we are */
		return true;

	dassert(devttyfd != -1);
#ifdef HPUX8
	/* posix termio  */
	if((ctermpg = tcgetpgrp(devttyfd)) == -1) {
		if((errno == ENOTTY) || (errno == EACCESS)) {
			/* not the controlling terminal - oh well */
			/* since we have no controlling terminal, I guess
			 * we had better say that we're in the background!
			 */
			return true;
		} else { 
			perror("tcgetpgrp");
			exit(2);
		}
	}
#else
	if(ioctl(devttyfd, TIOCGPGRP, &ctermpg) < 0) {
		if((errno == ENOTTY) || (errno == EOPNOTSUPP)) {
			/* not the controlling terminal - oh well */
			/* since we have no controlling terminal, I guess
			 * we had better say that we're in the background!
			 */
			return true;
		} else { 
			perror("TIOCGPGRP");
			exit(2);
		}
	} 
#endif
	dassert(ppid != -1);
	dassert(mypg != -1);
	result = (ctermpg  != mypg);

	DBG( << (char *)(result?"back":"fore") << "ground");
	return result;
}

static void
dofg(bool reopenstdin, bool reattach)
{
	/* background is set by -b or the terminal "background"
	 * command and, if set, perpetually overrides all other factors
	 */
	if(background) return; /* do nothing */

	if(reopenstdin) {
		/* reopen stdin - closes whatever it was... */
		DBG(<<
		"reopening, old fd " <<  fileno(stdin));
		freopen("/dev/tty", "r", stdin);
		DBG(<< "reopened, new fd " << fileno(stdin));
		/* set close-on-exec */
		if (fcntl(fileno(stdin), F_SETFD, NULL) < 0)    {
			exit(1);
		}
		if(reattach) {
			dassert(devttyfd != -1);
#ifdef HPUX8
			if(tcsetpgrp(devttyfd, mypg) < 0) {
				perror("tcsetpgrp");
			}
#else
			if(ioctl(devttyfd, TIOCSPGRP, &mypg) < 0) {
				perror("TIOCSPGRP");
			}
#endif
		}
	}
	/* restore mask after stdin has a fileno */
	// TODO: fix mask for select
	// setStdinLink();
}
#endif

//
// WARNING: This code should not be used.  See below.
//
//void
//dobg(bool dodetach)
//{
//	if(dodetach) {
//		DBG( << " detaching, ttyfd" << devttyfd);
//
//		dassert(devttyfd != -1);
//		dassert(ppid != -1);
//		dassert(mypg != -1);
//#ifdef HPUX8
//		if(tcsetpgrp(devttyfd, ppid) < 0) {
//			perror("tcsetpgrp");
//		}
//#else
//		if(ioctl(devttyfd, TIOCSPGRP, &ppid) < 0) {
//			perror("TIOCSPGRP");
//		}
//#endif
//	}
//	DBG( << "stdin fd " <<  fileno(stdin));
//
//	/* WARNING: fileno(stdin) is an unsigned char.  So without the cast the
//	 * test below would always be true.  The cast was added to make this
//	 * compile without any warnings.  This function is not being used and
//	 * if it does get used then it should be changed to use c++ I/O.
//	if ((signed char)fileno(stdin) != -1) {
//		/* it's open */
//		// clearStdinLink(); TODO: fix select mask
//		fclose(stdin);
//	}
//	/* now it's closed */
//}

void
sigcont(int sig)
{
	FUNC(sigcont);
	static int countTTIN = 0;

	dassert(sig == SIGCONT || sig==SIGTTIN);

	if(sig==SIGTTIN) {
		catastrophic(
	"You cannot run a server with a command shell while in the background.");
	}

#ifdef notdef
	if( isbg() ) {
		dobg(false); /* already detached */
	} else {
		countTTIN = 0;
		dofg(true, false);
	}
	if(++countTTIN > 4) {
		exit(4);
	}
#endif
}

#ifdef notdef
void
startuptty()
{
	if( isbg() ) {
		// clearStdinLink(); TODO: fix select mask
		/* its fd is already closed, but we want to 
		 * clean up the FILE too, so fclose it:
		 */
		fclose(stdin); 
	} else {
		/* already open, attached */
		// TODO: fix select mask setStdinLink();
	}
	DBG(<<"stdin fd " << fileno(stdin));
}
#endif

void sigquit(int sig) {
	DBG( << "Got signal " << sig );

#if defined(DEBUG) && defined(DEBUGTHREADS)
	cerr << "\nLOCKS:"<<endl; ss_m::dump_locks();
	cerr << "\nTHREADS:"<<endl; dumpthreads();
#endif
}

void sighup(int sig) {
	DBG( << "Got signal " << sig );
	// it's not safe to do anything from a signal
	// unless/until the threads package is extended
	// to deal with signals
}

void
svas_layer::catchSignals()
{
	extern int errno;
	register int i;
	FUNC(catchSignals);

	if( (ppid = getppid()) < 0 ) {
		perror("getppid(0)");
	}
	if( (mypg = 
#ifdef Ultrix42
		getpgrp(0)
#else
		getpgrp()
#endif
	) < 0 ) {
		perror("getpgrp()");
	}
	dassert(ppid != -1);
	dassert(mypg != -1);
	if((devttyfd = open("/dev/tty", O_RDONLY, 0))<0) {
		if(errno == ENXIO) {
			/* no such device  - that means whoever
			 * forked us did a TIOCNOTTY first - gak
			 */
			devttyfd = 0;
			/* that's our best guess. and if it's closed,
			 * well, c'est la vie. we won't be able to 
			 * do much.
			 */
		} else {
			perror("open dev/tty");
			// admin_ShutServer(0);
			exit(0);
		}
	}

	for(i=1; i<NSIG; i++) {

        if (siginfo[i].sig == 0) {
            break;  /* end of list of signals */
        }

		if(siginfo[i].handler == W_ANSI_C_HANDLER SIG_DFL){
			continue; /* don't bother changing it */
		}
	
#if defined(POSIX_SIGNALS)
		struct sigaction sa;

		sa.sa_handler = W_POSIX_HANDLER siginfo[i].handler;
		sigemptyset(&sa.sa_mask);
		sa.sa_flags = 0;

		if (sigaction(siginfo[i].sig, &sa, 0) == -1)
			exit(5);
#else
        if(signal (siginfo[i].sig, siginfo[i].handler) 
			== W_ANSI_C_HANDLER SIG_ERR) {
			exit(5);
		} 
#endif
	}
}

#ifdef notdef
void
uncatchSignal(
	int sig
)
{
	FUNC(uncatchSignal);

	handler = W_ANSI_C_HANDLER SIG_DFL;

    if(signal (sig, SIG_DFL) < 0) {
		catastrophic("signal-- reset to default"); 
		exit(1);
	}
}
#endif

#ifdef notdef

// for shell.C
void
redirect(int whichfile, char *path)
{
	static char *ctermpath = "/dev/tty";
	FILE *newstream;
	FILE *oldstream;
	// LINK *link;
	char *flags;
	// int	linkClass;

	DBG(<<"redirect " << whichfile << " to " << path);

	path = next_word();
	if( path == NULL) {
		path = ctermpath;
	} 
	if(RunInBackground && 
		((path == ctermpath) || !strcmp(path,ctermpath) ) ) {
nobg:
		fprintf(stdout, "\"-background\" option overrides.\n");
		fprintf(stdout, "Cannot redirect to terminal.\n");
		/* stdin is closed now, should never get selected */
		return;
	}
	switch(whichfile) {
		case 0:
			/* flags = O_RDONLY; */
			flags = "r";
			oldstream = stdin;
			// linkClass = CL_STDIN;
			break;

		case 1:
			flags = "a";
			oldstream = stdout;
			// linkClass = CL_STDOUT;
			break;

		case 2:
			/* flags = O_WRONLY; */
			flags = "a";
			oldstream = stderr;
			// linkClass = CL_STDERR;
			break;

		default:
			SM_ERROR(TYPE_FATAL, esmINTERNAL);
			return;
	}
	clearLink( &Links[fileno(oldstream)] );
	newstream = freopen(path, flags,  oldstream);
	if(newstream == NULL) {
		SM_ERROR(TYPE_SYS, errno);
		if(!RunInBackground) {
			newstream = freopen(ctermpath, flags,  oldstream);
			if(newstream == NULL) {
				SM_ERROR(TYPE_FATAL, errno);
			}
		} else {
			goto nobg;
		}
	}

	/* TODO: */
	// link = &(Links[fileno(newstream)]);
	// link->linkClass = linkClass; 
	// link->flags = 0;
	// setSelectBits(fileno(newstream));
	// setLink(link);
}
#endif

