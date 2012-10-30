// htmlignore

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#if (__GNUC_MINOR__ < 7)
static char *rcsid="$Header: /p/shore/shore_cvs/src/examples/stree/main.C,v 1.20 1996/07/19 22:57:17 nhall Exp $";
static void *const use_rcsid = (&use_rcsid, &rcsid, 0);
#endif

// htmlend
/*
 * ShoreConfig.h is needed only by applications
 * that distinguish platforms.  (Stree does not,
 * but we include this for documentation purposes.)
 */
#include <ShoreConfig.h>

#include <iostream.h>
#include <fstream.h>
#include <std.h>
#include "stree.h"

Ref<SearchTree> repository;
Ref<Pool> nodes;		// Place to create new anonymous objects

const char *DEMO_DIR = "stree";

char *argv0;
int verbose;
extern "C" int optind;

enum OPERATION {
	OP_NONE, OP_ADD, OP_LIST, OP_DEL, OP_POOL_LIST, OP_CLEAR
} operation = OP_NONE;

static void add_files(int argc, char *const*argc);
static void list_files(char *str);
static void delete_files(int argc, char *const*argc);
static void pool_list();
static void clear_all();

void usage() {
	cerr << "usage:" << endl;
	cerr << "\t" << argv0 << " -a[V] fname [fname ...]" << endl;
	cerr << "\t" << argv0 << " -l[V] word" << endl;
	cerr << "\t" << argv0 << " -d fname [fname ...]" << endl;
	cerr << "\t" << argv0 << " -p" << endl;
	cerr << "\t" << argv0 << " -c" << endl;
	cerr << "\t" << "the -V option turns on verbose mode" << endl;
	exit(1);
}

int main(int argc, char *argv[]) 
{
	argv0 = argv[0];
	shrc rc;
			
	// htmllabel stree:initcall
	// initialize connection to server
	SH_DO(Shore::init(argc, argv, 0, getenv("STREE_RC")));

	// get command-line options
	int c;
	while ((c = getopt(argc,argv,"aldpcV")) != EOF) switch(c) {
		case 'a': operation = OP_ADD; break;
		case 'l': operation = OP_LIST; break;
		case 'd': operation = OP_DEL; break;
		case 'p': operation = OP_POOL_LIST; break;
		case 'c': operation = OP_CLEAR; break;
		case 'V': verbose++; break;
		default: usage();
	}

	if (operation == OP_NONE)
		usage();

	// htmllabel stree:begintrans
	// Start a transaction for initialization
	SH_BEGIN_TRANSACTION(rc);
	if (rc)
		rc.fatal(); // this terminates the program with extreme prejudice

	// Check that our demo directory exists
	rc = Shore::chdir(DEMO_DIR);
	if (rc != RCOK) {
		if (rc != RC(SH_NotFound))
			SH_ABORT_TRANSACTION(rc);

		// Not found.  Must be the first time through.
		// Create the directory
		SH_DO(Shore::mkdir(DEMO_DIR, 0755));
		SH_DO(Shore::chdir(DEMO_DIR));

		// htmllabel stree:createrepository
		// Make a new SearchTree object ...
		repository = new("repository", 0644) SearchTree;
		repository.update()->initialize();

		// ... and a pool for allocating Nodes.
		SH_DO(nodes.create_pool("pool", 0644, nodes));
	} else { // not first time

		// Get the repository root from the database ...
		SH_DO(Ref<SearchTree>::lookup("repository",repository));

		// ... and the pool for creating nodes
		SH_DO(nodes.lookup("pool", nodes));
	}

	SH_DO(SH_COMMIT_TRANSACTION);

	switch (operation) {
		case OP_ADD:
			add_files(argc-optind, argv+optind);
			break;
		case OP_LIST:
			if (optind != argc-1)
				usage();
			list_files(argv[optind]);
			break;
		case OP_DEL:
			delete_files(argc-optind, argv+optind);
			break;
		case OP_POOL_LIST:
			pool_list();
			break;
		case OP_CLEAR:
			clear_all();
			break;
		default: break;
	}

	return 0;
} // main

// Add all the named files to the repository
static void add_files(int argc, char *const*argv) {
	shrc rc;

	SH_BEGIN_TRANSACTION(rc);
	if (rc)
		rc.fatal();
	for (int i=0; i<argc; i++)
		repository.update()->insert_file(argv[i]);
	if (verbose)
		cout << "about to commit" << endl;
	SH_DO(SH_COMMIT_TRANSACTION);
	if (verbose)
		cout << "committed" << endl;
} // add_files

// List all uses of a word
static void list_files(char *str) {
	shrc rc;
	int occurrences=0;

	SH_BEGIN_TRANSACTION(rc);
	if (rc)
		rc.fatal();
	Ref<Word> w = repository->find(str);
	if (verbose)
		cout << "========== " << str << endl;
	if (w && w->count() > 0) {
		Ref<Cite> c;
		for (int i=0; c = w->occurrence(i); i++) {
			occurrences++;
			c->print(verbose);
		}
	} else if (verbose) {
		cout << "**** Not found" << endl;
		occurrences = -1;
	}
	if(occurrences >= 0 && verbose) {
		cout << "**** " << occurrences << " citation"
			<< (char *)(occurrences==1?"":"s") << endl;
	}
	SH_DO(SH_COMMIT_TRANSACTION);
} // list_files

// htmllabel stree:deletefilesFUNC
// Removed the named files from the repository
static void delete_files(int argc, char *const*argv) {
	shrc rc;

	SH_BEGIN_TRANSACTION(rc);
	if (rc)
		rc.fatal();

	for (int i=0; i<argc; i++) {
		Ref<Document> d;
		SH_DO(d.lookup(argv[i],d));
		d.update()->finalize();
		SH_DO(Shore::unlink(argv[i]));
	}
	if (verbose)
		cout << "about to commit" << endl;
	SH_DO(SH_COMMIT_TRANSACTION);
	if (verbose)
		cout << "committed" << endl;
} // delete_files

// htmllabel stree:poollistFUNC
static void pool_list() {
	shrc rc;

	SH_BEGIN_TRANSACTION(rc);
	if (rc)
		rc.fatal();

	Ref<any> ref;
	Ref<Word> w;
	Ref<Cite> c;
	{
		PoolScan scan("pool");
		if (scan != RCOK)
			SH_ABORT_TRANSACTION(scan.rc());

		while (scan.next(ref, true) == RCOK) {
			if (w = TYPE_OBJECT(Word).isa(ref)) {
				w->print(1);
			}
			else if (c = TYPE_OBJECT(Cite).isa(ref)) {
				c->print(2);
			}
			else cout << " Unknown type of object" << endl;
		}
	}
	SH_DO(SH_COMMIT_TRANSACTION);
} // pool_list

// htmllabel stree:clearallFUNC
static void clear_all() {
	shrc rc;

	SH_BEGIN_TRANSACTION(rc);
	if (rc)
		rc.fatal();

	rc = Shore::unlink("repository");
	if (rc)
		cout << rc << endl;

	SH_DO(nodes.lookup("pool", nodes));
	SH_DO(nodes.destroy_contents());
	rc = Shore::unlink("pool");
	if (rc)
		cout << rc << endl;

	rc = Shore::chdir("..");
	if (rc)
		cout << rc << endl;

	SH_DO(Shore::rmdir(DEMO_DIR));

	SH_DO(SH_COMMIT_TRANSACTION);
} // clear_all
