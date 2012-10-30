// htmlignore

/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#if (__GNUC_MINOR__ < 7)
static char *rcsid="$Header: /p/shore/shore_cvs/src/examples/stree/docIndex.C,v 1.9 1996/07/19 22:57:01 nhall Exp $";
static void *const use_rcsid = (&use_rcsid, &rcsid, 0);
#endif

// htmlend
// Member functions of the DocIndex class
#include <iostream.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "doc_index.h"

extern Ref<Pool> nodes;
static int getword(const char *&p, char *res, int size);

extern int verbose; // defined in main.C

void DocIndex::initialize() {
	SH_DO(ind.init(UniqueBTree));
}

// htmllabel stree:docindexinsertFUNC
void DocIndex::insert(char *s, Ref<Cite> c) {
	Ref<Word> w;
	bool found;

	SH_DO(ind.find(s,w,found));
	if (!found) {
		w = new(nodes) Word;
		w.update()->initialize(s);
		SH_DO(ind.insert(s,w));
	}
	w.update()->occurs_on(c);
}

void DocIndex::insert_file(char *fname) {
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

// htmllabel stree:docindexfindFUNC
Ref<Word> DocIndex::find(char *str) const {
	Ref<Word> w;
	bool found;

	SH_DO(ind.find(str,w,found));
	if (found)
		return w;
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

void DocIndex::delete_word(sdl_string w) {
	int count;
	SH_DO(ind.remove(w,count));
	if (verbose)
		cout << "deleted " << count << (count==1 ? " copy" : " copies")
			<< " of word '" << (char *)w << "' from the index" << endl;
}

void DocIndex::print() const {
	// index_iter<typeof(ind)> iterator(ind);
	IndexScanIter<sdl_string,Ref<Word> > iterator(this->ind);
	SH_DO(iterator.next());
	while (!iterator.eof) {
		cout << "key: '" << iterator.cur_key << "' value: ";
		iterator.cur_val->print(1);
		SH_DO(iterator.next());
	}
}
