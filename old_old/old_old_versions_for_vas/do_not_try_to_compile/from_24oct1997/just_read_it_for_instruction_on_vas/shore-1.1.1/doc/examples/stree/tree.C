// htmlignore

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#if (__GNUC_MINOR__ < 7)
static char *rcsid="$Header: /p/shore/shore_cvs/src/examples/stree/tree.C,v 1.11 1996/07/19 22:57:20 nhall Exp $";
static void *const use_rcsid = (&use_rcsid, &rcsid, 0);
#endif

// htmlend
// Member functions of the SearchTree class
#include <iostream.h>
#include <string.h>
#include <ctype.h>
#include "stree.h"
#include <sys/types.h>
#include <sys/stat.h>

extern Ref<Pool> nodes;
static int getword(const char *&p, char *res, int size);

extern int verbose; // defined in main.C

void SearchTree::initialize() {
	root = NULL;
}

// htmllabel stree:searchtreeinsertFUNC
void SearchTree::insert(char *s, Ref<Cite> c) {
	Ref<Word> w;
	if (root) {
		w = root.update()->find_or_add(s);
	}
	else {
		root = new(nodes) Word;
		root.update()->initialize(s);
		w = root;
	}
	w.update()->occurs_on(c);
}

void SearchTree::insert_file(char *fname) {
	shrc rc;

	if (verbose)
		cout << "Indexing file " << fname << endl;

	// Open input file
	ifstream in(fname);
	if (!in) {
		perror(fname);
		SH_ABORT_TRANSACTION(rc);
	}
	// do a unix stat to get the total size of the file.
	struct stat in_stat;
	if (stat(fname,&in_stat)) {
		perror(fname);
		SH_ABORT_TRANSACTION(rc);
	}

	// Create target document

	// Strip leading path from file name;
	char *base_name = strrchr(fname, '/');
	if (base_name)
		base_name++;
	else
		base_name = fname;

	Ref<Document> doc;
	rc = doc.new_persistent(base_name, 0644, doc);
	if (rc) {
		perror(base_name);
		SH_ABORT_TRANSACTION(rc);
	}
	doc.update()->initialize(base_name,in_stat.st_size);

	// for each line of the document ...
	char linebuf[1024];
	while (in.getline(linebuf, sizeof linebuf -1)) {
		long off = doc->size();

		// copy the line to the body of the document
		doc.update()->append(linebuf);
		doc.update()->append("\n");

		// allocate a new Cite object for this line
		Ref<Cite> cite = new (nodes) Cite;
		cite.update()->initialize(doc, off);

		// for each word on the line ...
		char word[100];
		const char *p = linebuf;
		while (getword(p, word, sizeof word)) {
			// link the citation to the word
			insert(word, cite);
		}
	}
}

// htmllabel stree:searchtreefindFUNC
Ref<Word> SearchTree::find(char *str) const {
	if (root)
		return root->find(str);
	return NULL;
}

// Copy a word of at most SIZE characters (including terminating null)
// in to the buffer starting at RES.  Start searching at location P.
// Words are delimited by white space.  The result is translated to lower
// case, with all non-letters eliminated.
// P is updated to point to the first character not copied.
// The result is 1 if a word is found, 0 if '\0' is encountered first.
static int getword(const char *&p, char *res, int size) {
	for (;; ) {
		// skip leading white space
		while (isspace(*p))
			p++;

		// check for eoln
		if (*p == 0)
			return 0;

		// gather non-space characters, translating to lower case and
		// ignoring non-alpha characters
		int len;
		for (len = 0; len < size-1 && *p && !isspace(*p); p++) {
			if (isupper(*p))
				res[len++] = tolower(*p);
			else if (islower(*p))
				res[len++] = *p;
		}
		if (len > 0) {
			res[len] = 0;
			return 1;
		}
		// otherwise, word was all digits and punctuation, so try again.
	}
}
