/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
/////////////
// PR 233
/////////////

#include <iostream.h>
#include <fstream.h>
#include <std.h>
#define MODULE_CODE
#include <ShoreApp.h>
#include "strings.h"
#include <assert.h>

#define NELTS 10
sdl_string save[NELTS];
char * save_str[NELTS];

// TODO : test strcat,  bcopy
static  size_t 	previous_size = 1;
void
checkheapsize(
	Ref<stg> 	o,
	int			line,
	bool		expect_chg = false
) 
{
	static 	OStat	statbuf;
	shrc			rc;
	rc = o.ostat(&statbuf);
	if(rc) {
		cerr << "Error during ostat: " << rc << endl;
		exit(1);
	}
	if( (previous_size != statbuf.hsize) && !expect_chg) {
		cerr << line << ": Expected no size change; heap size was " 
				<< previous_size
				<< " now is " << statbuf.hsize << endl;
		exit(1);
	}
	if((previous_size == statbuf.hsize) && expect_chg) {
		cerr << line << " : Expected size change; heap size is " 
				<< statbuf.hsize 
				<< " was " << previous_size
				<< endl;
		exit(1);
	}
	previous_size = statbuf.hsize;
}
	

void 
dump(int line, 
	const char *msg,
	const sdl_string &s
) 
{
	cerr << endl << line << ": " << msg  << endl;
	cerr <<"\tstrlen()=" << s.strlen() << endl;
	cerr <<"\tblen()=" << 	s.blen() << endl;
	cerr << "\tvalue=" << (char *)s<<"|" <<endl;

	// look for l.c. vowels only
	char vowels[] = "aeiouy";
	for(size_t i=0; i<sizeof(vowels); i++) {
		if(s.countc(vowels[i])>0) {
			cerr <<"\tcountc(" << vowels[i] << ")=" 
			<< 	s.countc(vowels[i]) << endl;
		}
	}
}

int
main(int argc, char *argv[])
{
    int 	ncats = 3;
    shrc 	rc;

    if(argc >2 || argc < 1){
		cerr << " bad arguments " << endl;
		exit(1);
    }
    if(argc>=2) {
	ncats = atoi(argv[1]);
	if(ncats<=0) {
		cerr << " bad arguments " << endl;
		exit(1);
	}
    }
    pid_t p = getpid();
    char big[8192];
    for(size_t k=0; k<sizeof(big); k++) {
	big[k]='b';
    }
    big[sizeof(big)]='\0';

    char buf[30];
    ostrstream obuf(buf,sizeof(buf));

    obuf << "tmp" << p << "tmp" << p << ends;
    char *fname = buf;
    //
    // Establish a connection with the vas and initialize 
	// the object  cache.
	//
    SH_DO(Shore::init(argc, argv));

    SH_BEGIN_TRANSACTION(rc);

    // If that failed, or if a transaction aborted...
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
    } else {
	// The main body of the transaction goes here.

	SH_DO(Shore::chdir("/"));

	//////////////////////////////////
	// create a Unix file object
	//////////////////////////////////
	{
	    REF(stg) o;
	    SH_DO( REF(stg)::new_persistent (fname, 0644, o) ) ;
	    if(!o) {
		cerr << "Cannot create new stg." << endl;
	    } else {
		for(int j=0; j < NELTS; j++) {

		    ////////////////////////////////
		    // strlen does NOT compute ::strlen semantics
		    ////////////////////////////////
		    char *initial = "initial value=";
		    char *Value = "Value";

#define P(z) {\
	z;\
	dump(__LINE__, #z, o->stringarray[j]);\
}

			// empty
		    P(;);

			{ 	// try some flaky strcat stuff
				// try to strcat an empty string
				// PR 264 -- strcat doesn't work with null ptr
				save[j].strcat((char *)o->stringarray[j]);

			}

			// "initial value="
		    P(o.update()->stringarray[j].set(initial))

		    /* should extend the string but not change the
			 * nature of the null-terminated value
			 */
		    P(o.update()->stringarray[j].set(17, '\0'));
			checkheapsize(o, __LINE__, true);

			// "initial value=Z"
		    P(o.update()->stringarray[j].set(14, 'Z'));
			checkheapsize(o, __LINE__, false);

			// "initial value=0/1/..."
		    P(o.update()->stringarray[j].set(14, (char)('0'+j)));

		    /* should extend the string but not change its
			 * null-terminated string value
			 */
			// "initial value=0/1/..."
		    P(o.update()->stringarray[j].set(20, 'Z'));

#ifdef PURIFY
		    if (purify_is_running()) {
			// Clean up what's between the end of
			// the string and the end of the object
			size_t z;
			for(z=o->stringarray[j].strlen()+1;
				z < o->stringarray[j].blen();
				z++) {
			    o.update()->stringarray[j].set(z, '\0');
			}
		    }
#endif

			// "Initial value=0/1/..."
		    P(o.update()->stringarray[j].set(0, 'I'));

			// "Initial Value=0/1/..."
		    P(o.update()->stringarray[j].set(Value, 8, 5));

		    // save a copy of the sdl_string with memcpy() 
		    // and also a copy of the char * value with ::strcpy

		    // copy stringarray[j] into save[j]
			cout << "o->stringarray[" << j << "]:"
				<< " .blen()=" << o->stringarray[j].blen() 
				<< " .strlen()=" << o->stringarray[j].strlen() 
				<< endl;

		    save[j].memcpy(o->stringarray[j], o->stringarray[j].blen());

	        save_str[j] = new char[save[j].strlen() + 1];
		    if(save_str[j]) {
				::strcpy(save_str[j], save[j]);
			} else {
				cerr << " couldn't allocate space! " << endl;
				exit(1);
			}
		}
		o.update()->astring = "Now is the time.";

		{ 	// try some flaky strcat stuff
			dump(__LINE__, "astring", o->astring);
			o->update()->astring.strcat("");
			checkheapsize(o, __LINE__, true);
			o->update()->astring.strcat("");
			o->update()->astring.strcat("");
			dump(__LINE__, "astring", o->astring);
		}
	    }
	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }

	cerr << "Doing checks..." << endl;

    SH_BEGIN_TRANSACTION(rc);
    if(rc){
	cerr << rc << endl;
	return 1;
    } else {
	// The main body of the transaction goes here.

	{
#define SSIZ 1000
	    char 	s[SSIZ]; // plenty
	    char 	t[SSIZ]; // plenty
	    char	c;
	    REF(stg) o;
	    SH_DO( REF(stg)::lookup (fname,  o) ) ;
	    if(!o) {
			cerr << "Cannot locate " << fname << endl;
		} else {
			for(int j=0; j < NELTS; j++) {
				c = o->stringarray[j].get((size_t)0);
				int xx = o->stringarray[j].strlen();
				cout << " o->stringarray[j].strlen() = "
					<< xx << endl;

				o->stringarray[j].get(s, 0,xx+1);

				o->stringarray[j].get(t);
				if(::strcmp(t,s)) {
					cerr << 
					"mismatched strings: " << s << "|" << t << endl;
				}
				if(o->stringarray[j].strcmp(s)) {
					cerr << __LINE__ << " assert failure "<< endl;
					cerr
						<< (char *)o->stringarray[j] <<"|"
						<< " should match "
						<< s <<"|"
						<< endl;
					assert(1);
				}
				if(o->stringarray[j].strcmp(o->stringarray[j])) {
					cerr << __LINE__ << " assert failure "<< endl;
					cerr
						<< (char *)o->stringarray[j] <<"|"
						<< " should match itself "
						<< endl;
					assert(1);
				}
				cout << "stringarray[" << j << "]=" << s << endl;

				// compare with the saved sdl_string
				if(save[j].strcmp(o->stringarray[j]) ) {
					cerr << __LINE__ << " assert failure "<< endl;
					cerr
						<< (char *)o->stringarray[j] <<"|"
						<< " should match saved sdlstring "
						<< (char *)save[j] <<"|"
						<< endl;
					assert(1);
				}

				// compare with the saved value
				if(save_str[j]) {
					if(o->stringarray[j].strcmp(save_str[j]) ) {
						cerr << __LINE__ << " assert failure "<< endl;
					cerr
						<< (char *)o->stringarray[j] <<"|"
						<< " should match saved char *"
						<< save_str[j] <<"|"
						<< endl;
					assert(1);
					}
				}
				// contrast with astring
				if(save_str[j]) {
					if(o->stringarray[j].strncmp(o->astring, 5)==0 ) {
						cerr << __LINE__ << " assert failure "<< endl;
					cerr
						<< (char *)o->stringarray[j] <<"|"
						<< " should not match astring "
						<< (char *)o->astring <<"|"
						<< endl;
					assert(1);
					}
				}

			}
			o->astring.get(s);
			cout << "astring=" << s << endl;

			{ 		
				int k,l; 
				char *z[3] = { 
					"A stitch in time.", 
					"Now is the time.",
					"Zebra stripes." };

				sdl_string tmp = o->astring;

				for(int q = 0; q < 3; q++) {
					k = o->astring.strcmp(z[q]);
					l = tmp.strcmp(z[q]);
					assert(k==l);

					cerr << (const char *)o->astring;
					 if(k<0) {
						c = '<';
					 } else if (k==0) {
						c = '=';
					 } else if (k>0) {
						c = '>';
					} else {
						assert(0);
					}
					cerr << c << z[q] << endl;
				}
			}
		}
	}
	SH_DO(SH_COMMIT_TRANSACTION);
    }

	cerr << "done with checks..." << endl;

    // 
    // Another transaction to destroy the file...
    //
    SH_BEGIN_TRANSACTION(rc);
    if(rc){
	// after longjmp
	cerr << rc << endl;
	return 1;
    } else {
	SH_DO(Shore::unlink(fname));
	SH_DO(SH_COMMIT_TRANSACTION);
    }

#ifdef SIMPLE
	// Another set of simple checks 
	{
		sdl_string a, b;

		a = "";

		cerr 
			<< "a=" << a 
			<< "b=" << b 
			<< endl;

		a = "a";
		a.set(0,'\0');
		cerr 
			<< "a=" << a 
			<< "b=" << b 
			<< endl;
	}
#endif

    return 0;
}
