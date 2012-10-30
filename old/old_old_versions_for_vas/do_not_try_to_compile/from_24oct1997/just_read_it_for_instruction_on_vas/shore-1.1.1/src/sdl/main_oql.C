/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#include "metatypes.sdl.h"
#include "type_globals.h"
#include <std.h>
#include <iostream.h>
#include <GetOpt.h>
#include <stdio.h>

void usage(const char *prog) {
	cerr << "usage: " 
		<< prog 
		<< "[{-s input_file} | -S] "
		<< "[-f] "
		<< "[-c] "
		<< "[-v] "
		<< "{-d <directory>}*  "
		<< "[{-b <module_name>}* | -B]  "
		<< "[{-l <module_name>}* | -L]  "
		<< "[-o output_file] " 
		<< "[-L] "
		<< "[-B] "
		<< "[-r pool_name] "
		<< "[-a] "
		<< "[-p] "
		<< "[-P] "

		<< endl;
	exit(1);
}


int scheck_only = 0; // flag  calling for syntax check only.
int overwrite_module = 0; // flag if existing modules should be overriden.
int verbose_flag = 0;
int print_aqua = 0;
int print_oql = 0;
int print_oql2 = 0;
char *src_args[20];
char *link_args[20];
char *bind_args[20];
char *dir_args[20];
char *rm_args[20];
char *output_arg = 0; // only one allowed.
int src_count = 0;
int link_count = 0;
int bind_count = 0;
int dir_count = 0;
int rm_count = 0;
int bind_all = 0;
int link_all = 0;
extern int sdl_linking;
extern int sdl_errors;
char *argv0;
// test: -s filename = sdl source file to compile, 
//  -l linkname = sdl module name to link,
// 	-b bname	= sdl module name to print language binding for.

int Aexecute_query (const char *query_str, char *result_file);

char * cur_src = 0;
static int
process_src(char *fname)
// parse the named file as sdl input.
{
	W_COERCE(Shore::begin_transaction(3));
#ifdef 0
	if (fname[0]=='-' && fname[1]==0) // -s - : use original stdin.
		fname = "<stdin>";
	else if (!freopen(fname,"r",stdin))
	{
		char buf[200];
		sprintf(buf,"%s : couldn't open src file %s",argv0,fname);
		perror(buf);
		return 1;
	}
	cerr<< "processing source file " << fname << endl;
	cur_src = fname;
	// for oql, buffer the src into a char *.
	struct stat istat;
	fstat(0,&istat);
	char * ibuf;
	ibuf = new char [istat.st_size+1];
	read(0,ibuf,istat.st_size);
	ibuf[istat.st_size]=0;
	char result_f[200];
	Aexecute_query(ibuf,result_f);
#else 
	// use -s arg as a string to parse.
	char result_f[200];
	Aexecute_query(fname,result_f);
#endif
	SH_DO(SH_COMMIT_TRANSACTION);

	return 0;

}

extern
void add_dir(char *p);

static int
process_link(char *lname)
// process the string as the name of an sdl module object.
{
	Ref<sdlModule> lmod;
	int errcount;
	W_COERCE(Shore::begin_transaction(3));
	lmod = lookup_module(lname);
	sdl_linking = 1;
	if (lmod!= 0)
	{
		lmod.update()->resolve_types();
		if (sdl_errors)
		{
			cerr << sdl_errors << " found linking module " << lname << endl;
			Shore::abort_transaction();
			return sdl_errors;
		}
	}
	SH_DO(SH_COMMIT_TRANSACTION);
	return 0;
}

static int
rm_files(int rm_count, char **rm_names)
// process the string as the name of an sdl module object.
{
	int errcount;
	W_COERCE(Shore::begin_transaction(3));
	int i;
	for (i=0;i<rm_count; i++)
	{
		Ref<any> lpt;
		SH_DO ( OCRef::lookup(rm_names[i],lpt));
		Ref<Pool> pref = TYPE_OBJECT(Pool).isa(lpt); 
		if (pref != 0)
		{
			SH_DO(pref.destroy_contents());
		}
		SH_DO(Shore::unlink(rm_names[i]));


	}
	SH_DO(SH_COMMIT_TRANSACTION);
	return 0;
}

int
open_output_file(const char *oname)
{
	char buf[200];
	char * fn;
	if (output_arg) // use output arg as the file name
	{
		if (output_arg[0]=='-' && output_arg[1]==0) // -o - : use std_out stdin.
			fprintf(stderr,"language binding sent to standard output\n");
		else if(!freopen(output_arg,"w",stdout))
		{
			sprintf(buf,"%s : couldn't open binding output file %s",argv0,
				output_arg);	
			perror(buf);
			return -1;
		}
		fn = output_arg;
	}
	else
	{
		sprintf(buf,"%s.h",oname);
		if (!freopen(buf,"w",stdout))
		{
			sprintf(buf,"%s : couldn't open binding output file %s.h",argv0,
				oname);
			perror(buf);
			return -1;
		}
		fn = buf;
	}
	fprintf(stderr,"language binding output sent to %s\n",fn);
	return 0;
}

static int
process_bind(char *lname)
{
	Ref<sdlModule> bmod;
	Set<Ref<sdlModule> > omods;
	// first, [re]open stdout for appropriate name.
	// do this outside of a transaction..
	W_COERCE(Shore::begin_transaction(3));
	bmod = lookup_module(lname);
	if (bmod == 0) // null value
	{
		cerr << "couldn't find module " <<  lname << endl;
		return -1;
	}
	omods.add(bmod);
	if (open_output_file(lname)==0)
		;
	// this should be a readonly transaction, so never commit.
	SH_DO(SH_COMMIT_TRANSACTION);
	return 0;
}

// print out language binding header files for anything processed from 
// source.
extern Ref<sdlDeclaration>   g_module_list;
print_all_bindings()
{
	Ref<sdlModule> bmod;
	Set<Ref<sdlModule> > omods;
	W_COERCE(Shore::begin_transaction(3));
	Ref<sdlDeclaration> lpt;
	for (lpt = g_module_list; lpt != NULL; lpt = lpt->next)
	{
		bmod = ((Ref<sdlModDecl> &)lpt)->dmodule;
		omods.add(bmod);
	}
	// this should be a readonly transacton, so never commit.
	if (omods.get_size()>0)
	{
		if (open_output_file(omods.get_elt(0)->name.string()) == 0)
			;
	}
	SH_DO(SH_COMMIT_TRANSACTION);
	return 0;
}

link_all_modules()
// link all listed modules together.
{
	Ref<sdlModule> bmod;
	Set<Ref<sdlModule> > omods;
	W_COERCE(Shore::begin_transaction(3));
	Ref<sdlDeclaration> lpt;
	for (lpt = g_module_list; lpt != NULL; lpt = lpt->next)
	{
		bmod = ((Ref<sdlModDecl> &)lpt)->dmodule;
		omods.add(bmod);
	}
	sdl_linking = 1;

	for(int i=0; i<omods.get_size(); i++)
	{
		bmod = omods.get_elt(i);
		bmod.update()->resolve_types();
		if (sdl_errors)
		{
			cerr << sdl_errors << " found linking module " << bmod->name << endl;
			SH_DO(Shore::abort_transaction());
			return sdl_errors;
		}
	}
	sdl_linking = 0;
	// this should be a readonly transacton, so never commit.
	SH_DO(SH_COMMIT_TRANSACTION);
	return 0;
}


void metaobj_init(int argc, char *argv[]); // initialize named types
extern void OQL_Initialize(void);

void insert_rwords();
static int debug = 0;

	
int main(int argc, char **argv) {
	GetOpt opt(argc,argv,"8vfacpPSLBDs:l:b:d:o:r:");
	int c, rc;
	argv0 = argv[0];
	while ((c = opt()) != EOF) {
		switch (c) {
			case 'a':
				print_aqua = 1;
			break;
			case 'p':
				print_oql = 1;
			break;
			case 'P':
				print_oql2 = 1;
			break;
			case 'v': // verbose
				verbose_flag = 1;
			break;
			case 'f' : //  overwrite existing module if found.
				overwrite_module = 1;
			break;
			case 'c' : //  syntax check only
				scheck_only = 1;
			break;
			case 'S': // read from standard input
				src_args[src_count++] = "-";
			break;
			case 'B': // print out language binding for all modules processed.
				bind_all = 1;
			break;
			case 'L': // link all modules processed  (from source).
				link_all = 1;
			break;
			case 'D':
				debug++;
			break;
			case 's':
				src_args[src_count++] = opt.optarg;
			break;
			case 'r':
				rm_args[rm_count++] = opt.optarg;
			break;
			case 'l':
				link_args[link_count++] = opt.optarg;
			break;
			case 'b':
				bind_args[bind_count++] = opt.optarg;
			break;
			case 'd':
				dir_args[dir_count++] = opt.optarg;
			break;
			case 'o':
				if (output_arg)	
				{
					cerr << "only one output file name allowed" <<endl;
					usage(argv[0]);
				}
				output_arg = opt.optarg;
			break;
			default:
				usage(argv[0]);
				return 1;
		}
	}
	int i;
	if (debug || verbose_flag) {
		fprintf(stderr,"sdl: command line was\n\t");
		for (i = 0; i < argc; i++)
			fprintf(stderr,"%s ",argv[i]);
		fprintf(stderr,"\n");

		if (src_count>0)
		{
			fprintf(stderr,"compiling sdl source files: ");
			for (i = 0; i<src_count; i++)
				fprintf(stderr,"%s ",src_args[i]);
			fprintf(stderr,"\n");
		}
		if (link_count>0)
		{
			fprintf(stderr,"linking moudles: ");
			for (i = 0; i<link_count; i++)
				fprintf(stderr,"%s ",link_args[i]);
			fprintf(stderr,"\n");
		}
		if (bind_count>0)
		{
			fprintf(stderr,"creating language binding for modules: ");
			for (i = 0; i<bind_count; i++)
				fprintf(stderr,"%s ",bind_args[i]);
			fprintf(stderr,"\n");
		}
		if (dir_count>0)
		{
			fprintf(stderr,"shore directory search path: ");
			for (i = 0; i<dir_count; i++)
				fprintf(stderr,"%s ",dir_args[i]);
			fprintf(stderr,"\n");
		}
		if ( bind_all)
			fprintf(stderr,"-B: printing language binding for all source processed\n");
		if ( link_all)
			fprintf(stderr,"-L: linking modules for all source processed\n");
		if ( overwrite_module)
			fprintf(stderr,"-f: deleting existing modules\n");
		if (scheck_only)
			fprintf(stderr,"-c: syntax check only\n");


	}
	// initialization:
	if (!scheck_only)
		metaobj_init(argc,argv);
	OQL_Initialize();
	if (src_count)
		insert_rwords();
	if (dir_count)
	{
		w_rc_t crc;
		for (i=0; i<dir_count; i++)
			add_dir(dir_args[i]);
		// also, chdir to 1st dir arg, creating it if necessary.
		W_COERCE(Shore::begin_transaction(3));
		crc = Shore::chdir(dir_args[0]);
		if (crc)
		{
			if (crc.err_num() != SH_NotFound)
				crc.fatal(); //give up
			SH_DO(Shore::mkdir(dir_args[0],0755));
			SH_DO(Shore::chdir(dir_args[0]));
		}
		SH_DO(SH_COMMIT_TRANSACTION);
	}
	// first, remove anything specified by -r
	if (rm_count >0)
		rm_files(rm_count,rm_args);
		
	for (i = 0; i<src_count; i++) {
		if ((rc = process_src( src_args[i])))
			return rc;
		if (sdl_errors)
		{
			fprintf(stderr,"found %d errors processing sdl source file %s\n",
				sdl_errors,src_args[i]);
			return sdl_errors;
		}
	}
	if (src_count && (g_module_list!=   0)) // say what we created
	{
		char * m_dir = dir_count? dir_args[0]: "/types";
		W_COERCE(Shore::begin_transaction(3));
		Ref<sdlDeclaration> lpt;

		for (lpt = g_module_list; lpt != NULL; lpt = lpt->next)
			fprintf(stderr,"created module 	%s/%s\n",m_dir,(char *)lpt->name);
		SH_DO(SH_COMMIT_TRANSACTION);
	}
	if ( scheck_only && (link_count || bind_all || bind_count))
	{
		fprintf(stderr,"cannot link or bind with -c flag\n");
		return sdl_errors;
	}
	if (link_all)
		if (rc = link_all_modules())
			return rc;
	for (i = 0; i < link_count; i++) {
		if ((rc = process_link( link_args[i])))
			return rc;
	}
	if (bind_all) //orint all bindings from src module list
		if (rc = print_all_bindings())
			return rc;
	if (bind_count == 1) // old style binding printout
	{
		if ((rc = process_bind(bind_args[i])))
			return rc;
	}
	else if (bind_count > 0)
	{
		Set<Ref<sdlModule> > omods;
		W_COERCE(Shore::begin_transaction(3));
		
		for (i = 0; i < bind_count; i++) 
		{
			Ref<sdlModule> bmod;
			bmod = lookup_module(bind_args[i]);
			if (bmod == 0) // null value
			{
				cerr << "couldn't find module " <<  bind_args[i] << endl;
				break;
			}
			omods.add(bmod);
		}
		if (omods.get_size()>0 
			&& open_output_file(omods.get_elt(0)->name.string()) == 0)
				;
		W_COERCE(SH_COMMIT_TRANSACTION);
	}
	return sdl_errors;
}
