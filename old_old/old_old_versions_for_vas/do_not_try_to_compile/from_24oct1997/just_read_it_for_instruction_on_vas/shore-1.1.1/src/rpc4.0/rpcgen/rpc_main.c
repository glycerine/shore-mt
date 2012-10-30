/* @(#)rpc_main.c	2.2 88/08/01 4.0 RPCSRC */
/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
#ifndef lint
static char sccsid[] = "@(#)rpc_main.c 1.7 87/06/24 (C) 1987 SMI";
#endif

#define PWHAT \
	if(fout) f_print(fout, "/*???%d %s???*/\n",__LINE__,__FILE__);

/*
 * rpc_main.c, Top level of the RPC protocol compiler. 
 * Copyright (C) 1987, Sun Microsystems, Inc. 
 */

#include <stdio.h>
#ifdef SUNOS41
#include <strings.h>  
#else
#include <string.h>  
#ifndef Linux
char *rindex(char *, char);	/* provide prototype since string.h doesn't */
#endif /* Linux */
#endif /* !SUNOS41 */
#include <sys/file.h>
#include "rpc_util.h"
#include "rpc_parse.h"
#include "rpc_scan.h"
#ifdef notdef
#include <debug.h>
#endif

#define EXTEND	1		/* alias for TRUE */

struct commandline {
	int cplusflag;
	int ansiflag;
	int cflag;
	int hflag;
	int lflag;
	int sflag;
	int mflag;
	char *infile;
	char *fileprefix;
	char *outfile;
};

int cplusplus=0;
int ansi
# 	ifdef Sparc
	= 0; /* sun delivers ancient non-ansi cc -- bleah */
#	else
	= 1;
#	endif
static char *cmdname;

#ifdef SOLARIS2
static char CPP[] = "/usr/ccs/lib/cpp";
#else
static char CPP[] = "/lib/cpp";
#endif
static char CPPFLAGS[] = "-C";
static char *allv[] = {
	"rpcgen", "-s", "udp", "-s", "tcp",
};
static int allc = sizeof(allv)/sizeof(allv[0]);
static c_output(), h_output(), s_output(), l_output(),
	do_registers(), parseargs();

main(argc, argv)
	int argc;
	char *argv[];

{
	struct commandline cmd;

	if (!parseargs(argc, argv, &cmd)) {
		f_print(stderr,
			"usage: %s infile\n", cmdname);
		f_print(stderr,
			"       %s [-c | -C | -h | -l | -m] [-o outfile] [infile]\n",
			cmdname);
		f_print(stderr,
			"       %s [-s udp|tcp]* [-o outfile] [infile]\n",
			cmdname);
		exit(1);
	}
	cplusplus = cmd.cplusflag;
	ansi = cmd.ansiflag;

#ifdef notdef
fprintf(stderr, "cmd.infile=%s\n", cmd.infile);
fprintf(stderr, "cmd.outfile=%s\n", cmd.outfile);
fprintf(stderr, "cmd.fileprefix=%s\n", cmd.fileprefix);
#endif

	if (cmd.cflag) {
		c_output(cmd.infile, "-DRPC_XDR", cmd.fileprefix, 0, cmd.outfile);
	} else if (cmd.hflag) {
		h_output(cmd.infile, "-DRPC_HDR", cmd.fileprefix, 0, cmd.outfile);
	} else if (cmd.lflag) {
		l_output(cmd.infile, "-DRPC_CLNT", cmd.fileprefix, 0, cmd.outfile);
	} else if (cmd.sflag || cmd.mflag) {
		s_output(argc, argv, cmd.infile, "-DRPC_SVC", cmd.fileprefix, 0,
			 cmd.outfile, cmd.mflag);
	} else {
		c_output(cmd.infile, "-DRPC_XDR", cmd.fileprefix, 1, "_xdr.c");
		reinitialize();
		h_output(cmd.infile, "-DRPC_HDR", cmd.fileprefix, 1, ".h");
		reinitialize();
		l_output(cmd.infile, "-DRPC_CLNT", cmd.fileprefix, 1, "_clnt.c");
		reinitialize();
		s_output(allc, allv, cmd.infile, "-DRPC_SVC", cmd.fileprefix, 1,
			 "_svc.c", cmd.mflag);
	}
	exit(0);
}

/*
 * add extension to filename 
 */
static char *
extendfile(file, ext)
	char *file;
	char *ext;
{
	char *res;
	char *p;

	res = alloc(strlen(file) + strlen(ext) + 1);
	if (res == NULL) {
		abort();
	}
	p = rindex(file, '.');
	if (p == NULL) {
		p = file + strlen(file);
	}
	(void) strcpy(res, file);
	(void) strcpy(res + (p - file), ext);
	return (res);
}

static char *
file_prefix(file)
	char *file;
{
	char *res;
	char *dot;
	char *slash;

	res = alloc(strlen(file)+1);
	if (res == NULL) {
		abort();
	}
	slash = rindex(file, '/');
	if (slash == NULL) {
		slash = file;
	} else {
		slash++;
	}
	(void) strcpy(res, slash);
	dot = rindex(res, '.');
	if (dot != NULL) {
		*dot = '\0';
	}
	return (res);
}

/*
 * Open output file with given extension 
 */
static
open_output(infile, outfile)
	char *infile;
	char *outfile;
{
	if (outfile == NULL) {
		fout = stdout;
	} else {
		if (infile != NULL && streq(outfile, infile)) {
			f_print(stderr, "%s: output would overwrite %s\n", cmdname,
				infile);
			crash();
		}
		fout = fopen(outfile, "w");
		if (fout == NULL) {
			f_print(stderr, "%s: unable to open ", cmdname);
			perror(outfile);
			crash();
		}
		record_open(outfile);
	}
	f_print(fout, "/*\n * Please do not edit this file.\n");
	f_print(fout, " * It was generated using rpcgen (modified.)\n");
	f_print(fout, " * infile= %s.\n",infile);
	f_print(fout, " * outfile= %s.\n",outfile);
	f_print(fout, " */\n");
	if(outfile!=NULL) {
		char *dot = rindex(outfile,'.');
		*dot = '_';
		f_print(fout, "#ifndef __%s__\n", outfile);
		f_print(fout, "#define __%s__\n", outfile);
		*dot = '.';
	} else {
		f_print(fout, "#ifndef __%s__\n", "stdout");
		f_print(fout, "#define __%s__\n", "stdout");
	}
}

/*
 * Open input file with given define for C-preprocessor 
 */
static
open_input(infile, define)
	char *infile;
	char *define;
{
	int pd[2];

	infilename = (infile == NULL) ? "<stdin>" : infile;
	(void) pipe(pd);
	switch (fork()) {
	case 0:
		(void) close(1);
		(void) dup2(pd[1], 1);
		(void) close(pd[0]);
		execl(CPP, CPP, CPPFLAGS, define, infile, NULL);
		perror("execl");
		exit(1);
	case -1:
		perror("fork");
		exit(1);
	}
	(void) close(pd[1]);
	fin = fdopen(pd[0], "r");
	if (fin == NULL) {
		f_print(stderr, "%s: ", cmdname);
		perror(infilename);
		crash();
	}
}

/*
 * Compile into an XDR routine output file
 */
static
c_output(infile, define, prefix, extend, outfile)
	char *infile;
	char *define;
	char *prefix;
	int extend;
	char *outfile;
{
	char *out_define = define + 2;
	definition *def;
	char *include;
	char *outfilename;
	long tell;

	open_input(infile, define);	
	outfilename = extend ? extendfile(prefix, outfile) : outfile;
	open_output(infile, outfilename);
	f_print(fout, "#ifndef %s\n#define %s\n#endif\n", out_define,out_define);

	f_print(fout, "#include <rpc/rpc.h>\n");
	if (infile && (include = extendfile(prefix, ".h"))) {
		f_print(fout, "#include \"%s\"\n", include);
		free(include);
	}
	tell = ftell(fout);
	while (def = get_definition()) {
		emit(def);
	}
	PWHAT f_print(fout, "#endif\n");

	if (extend && tell == ftell(fout)) {
		(void) unlink(outfilename);
	}
}

/*
 * Compile into an XDR header file
 */
static
h_output(infile, define, prefix, extend, outfile)
	char *infile;
	char *define;
	char *prefix;
	int extend;
	char *outfile;
{
	char *out_define = define + 2;
	definition *def;
	char *outfilename;
	long tell;

	open_input(infile, define);
	outfilename =  extend ? extendfile(prefix, outfile) : outfile;
	open_output(infile, outfilename);
	f_print(fout, "#ifndef %s\n#define %s\n#endif\n", out_define,out_define);
	tell = ftell(fout);
	while (def = get_definition()) {
		print_datadef(def);
	}
	if (extend && tell == ftell(fout)) {
		PWHAT f_print(fout, "#endif\n");
		(void) unlink(outfilename);
	}
	PWHAT f_print(fout, "#endif\n");
}

/*
 * Compile into an RPC service
 */
static
s_output(argc, argv, infile, define, prefix, extend, outfile, nomain)
	int argc;
	char *argv[];
	char *infile;
	char *define;
	char *prefix;
	int extend;
	char *outfile;
	int nomain;
{
	char *out_define = define + 2;
	char *include;
	definition *def;
	int foundprogram;
	char *outfilename;

	open_input(infile, define);
	outfilename = extend ? extendfile(prefix, outfile) : outfile;
	open_output(infile, outfilename);
	f_print(fout, "#ifndef %s\n#define %s\n#endif\n", out_define,out_define);
	f_print(fout, "#include <stdio.h>\n");
	f_print(fout, "#include <rpc/rpc.h>\n");
	if (infile && (include = extendfile(prefix, ".h"))) {
		f_print(fout, "#include \"%s\"\n", include);
		free(include);
	}
	foundprogram = 0;
	while (def = get_definition()) {
		foundprogram |= (def->def_kind == DEF_PROGRAM);
	}
	if (extend && !foundprogram) {
	PWHAT
		f_print(fout, "#endif\n");
		(void) unlink(outfilename);
		return;
	}
	PWHAT
	if (nomain) {
		write_programs((char *)NULL);
	} else {
		write_most();
		do_registers(argc, argv);
		write_rest();
		write_programs("static");
	}
	f_print(fout, "#endif\n");
}

static
l_output(infile, define, prefix, extend, outfile)
	char *infile;
	char *define;
	char *prefix;
	int extend;
	char *outfile;
{
	char *out_define = define + 2;
	char *include;
	definition *def;
	int foundprogram;
	char *outfilename;

	open_input(infile, define);
	outfilename = extend ? extendfile(prefix, outfile) : outfile;
	open_output(infile, outfilename);
	f_print(fout, "#ifndef %s\n#define %s\n#endif\n", out_define,out_define);
	f_print(fout, "#include <rpc/rpc.h>\n");
	if (infile && (include = extendfile(prefix, ".h"))) {
		f_print(fout, "#include \"%s\"\n", include);
		free(include);
	}
	foundprogram = 0;
	while (def = get_definition()) {
		foundprogram |= (def->def_kind == DEF_PROGRAM);
	}
	if (extend && !foundprogram) {
		PWHAT
		f_print(fout, "#endif\n");
		(void) unlink(outfilename);
		return;
	}
	write_stubs();
	PWHAT f_print(fout, "#endif\n");
}

/*
 * Perform registrations for service output 
 */
static
do_registers(argc, argv)
	int argc;
	char *argv[];

{
	int i;

	for (i = 1; i < argc; i++) {
		if (streq(argv[i], "-s")) {
			write_register(argv[i + 1]);
			i++;
		}
	}
}

/*
 * Parse command line arguments 
 */
static
parseargs(argc, argv, cmd)
	int argc;
	char *argv[];
	struct commandline *cmd;

{
	int i;
	int j;
	char c;
	char flag[(1 << 8 * sizeof(char))];
	int nflags;

	cmdname = argv[0];
	cmd->infile = cmd->outfile = NULL;
	if (argc < 2) {
		return (0);
	}
	flag['C'] = 0; /* c++ */
	flag['c'] = 0;
	flag['h'] = 0;
	flag['s'] = 0;
	flag['o'] = 0;
	flag['l'] = 0;
	flag['m'] = 0;
	flag['A'] = 0;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] != '-') {
			if (cmd->infile) {
				return (0);
			}
			cmd->infile = argv[i];
		} else {
			for (j = 1; argv[i][j] != 0; j++) {
				c = argv[i][j];
				switch (c) {
				case 'C':
				case 'c':
				case 'h':
				case 'l':
				case 'm':
					if (flag[c]) {
						return (0);
					}
					flag[c] = 1;
					break;
				case 'o':
				case 's':
					if (argv[i][j - 1] != '-' || 
					    argv[i][j + 1] != 0) {
						return (0);
					}
					flag[c] = 1;
					if (++i == argc) {
						return (0);
					}
					if (c == 's') {
						if (!streq(argv[i], "udp") &&
						    !streq(argv[i], "tcp")) {
							return (0);
						}
					} else if (c == 'o') {
						if (cmd->outfile) {
							return (0);
						}
						cmd->outfile = argv[i];
					}
					goto nextarg;

				default:
					return (0);
				}
			}
	nextarg:
			;
		}
	}
	cmd->cplusflag = flag['C'];
	cmd->ansiflag = flag['A'];
	cmd->cflag = flag['c'];
	cmd->hflag = flag['h'];
	cmd->sflag = flag['s'];
	cmd->lflag = flag['l'];
	cmd->mflag = flag['m'];
	nflags = cmd->cflag + cmd->hflag + cmd->sflag + cmd->lflag + cmd->mflag;
	cmd->fileprefix = file_prefix(cmd->infile);
	if (nflags == 0) {
		if (cmd->outfile != NULL || cmd->infile == NULL) {
			return (0);
		}
	} else if (nflags > 1) {
		return (0);
	}
	return (1);
}
