/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <stream.h>
#include <w.h>
#include <basics.h>
#include <serial_t.h>
#include <assert.h>
#include <vec_t.h>
#include <zvec_t.h>
#include <debug.h>

char *d = "dddddddddd";
char *djunk = "Djunk";
char *b = "bbbbbbbbbb";
char *bjunk = "Bjunk";
char *c = "cccccccccc";
char *cjunk = "Cjunk";
char *a = "aaaaaaaaaa";
char *ajunk = "Ajunk";

void V(const vec_t &a, int b, int c, vec_t &d)
{
	int	i;

	DBG(<<"*******BEGIN TEST("  << b << "," << c << ")");
	for(i=0; i<a.count(); i++) {
		DBG(<<"vec["<<i<<"]=("
			<<::dec((unsigned int)a.ptr(i)) <<"," <<a.len(i) <<")");
	}

	for(int l = 0; l<100; l++) {
		if(c > (int) a.size()) break;
		a.mkchunk(b, c, d);

#ifdef DEBUG
	cout <<"returning  :";
	for(i=0; i<d.count(); i++) {
		cout <<"result["<<i<<"]=("
			<<::dec((unsigned int)d.ptr(i)) <<"," <<d.len(i) <<")" << endl;
	}
	for(i=0; i<d.count(); i++) {
		for(int j=0; j< (int)d.len(i); j++) {
			cout << *(char *)(d.ptr(i)+j);
		}
	} cout << endl;
#endif
		c+=b;
	}
	DBG(<<"*******END TEST");
}

void
P(const char *s) 
{
	int len = strlen(s);
	istrstream anon(s,len);

	vec_t	t;
	cout << "P:" << s << endl;
	anon >> t;

	cout << "input operator:" << endl;
	for(int i=0; i<t.count(); i++) {
		cout <<"vec["<<i<<"]=("
			<<::dec((unsigned int)t.ptr(i)) <<"," <<t.len(i) <<")" << endl;
	}

	cout << "output operator:" << endl;
	cout << t;
	cout << endl;
}

main()
{
	vec_t test;
	vec_t tout;

#define TD(i,j) test.put(&d[i], j); cerr << "d is at " << ::dec((unsigned int)d) << endl;
#define TB(i,j) test.put(&b[i], j); cerr << "b is at " << ::dec((unsigned int)b) << endl;
#define TA(i,j) test.put(&a[i], j); cerr << "a is at " << ::dec((unsigned int)a) << endl;
#define TC(i,j) test.put(&c[i], j); cerr << "c is at " << ::dec((unsigned int)c) << endl;

	TA(0,10);
	TB(0,10);
	TC(0,10);
	TD(0,10);


	V(test, 5, 7, tout);
	V(test, 5, 10, tout);
	V(test, 5, 22, tout);

	V(test, 11, 0, tout);
	V(test, 11, 7, tout);
	V(test, 11, 9, tout);

	V(test, 30, 9, tout);
	V(test, 30, 29, tout);
	V(test, 30, 40, tout);
	V(test, 40, 30, tout);

	V(test, 100, 9, tout);

	P("{ {1   \"}\" }");
	/*{{{*/
	P("{ {3 \"}}}\"      }}");
	P("{ {30 \"xxxxxxxxxxyyyyyyyyyyzzzzzzzzzz\"} }");
	P("{ {30 \"xxxxxxxxxxyyyyyyyyyyzzzzzzzzzz\"}{10    \"abcdefghij\"} }");

	{
		vec_t t;
		t.reset();
		t.put("abc",3);
		for(int i=0; i<t.count(); i++) {
			cout <<"vec["<<i<<"]=("
				<<::dec((unsigned int)t.ptr(i)) <<"," <<t.len(i) <<")" << endl;
		}
		cout << "FINAL: "<< t << endl;
	}

	{
		cout << "ZVECS: " << endl;

		{
			zvec_t z(0);
			cout << "z(0).count() = " << z.count() << endl;
			cout << "z(0) is zero vec: " << z.is_zvec() << endl;
			vec_t  zv;
			zv.set(z);
			cout << "zv.set(z).count() = " << zv.count() << endl;
			cout << "zv is zero vec: " << zv.is_zvec() << endl;
		}
		{
			zvec_t z;
			cout << "z is zero vec: " << z.is_zvec() << endl;
			cout << "z.count() = " << z.count() << endl;
			vec_t  zv;
			zv.set(z);
			cout << "zv.set(z).count() = " << zv.count() << endl;
			cout << "zv(0) is zero vec: " << zv.is_zvec() << endl;
		}
	}

	{
		int n = 0x80000003;
		int m = 0xeffffffc;
		vec_t   num( (void*)&n, sizeof(n));
		vec_t   num2( (void*)&m, sizeof(m));

		cout << "vec containing 0x80000003 prints as: " << num  << endl;
		cout << "vec containing 0xeffffffc prints as: " << num2  << endl;
	}

	{
		char c = 'h';
		char last = (char)'\377';
		char last1 = '\377';
		char last2 = (char)0xff;

		vec_t   cv( (void*)&c, sizeof(c));
		vec_t   lastv( (void*)&last, sizeof(last));
		vec_t   last1v( (void*)&last1, sizeof(last1));
		vec_t   last2v( (void*)&last2, sizeof(last2));

		bool result = (bool)(cv < lastv);
		cout << "cv <= lastv: " << result << endl;
		cout << "cv prints as: " << cv <<endl;
		cout << "lastv prints as: " << lastv <<endl;
		cout << "last1 prints as: " << last1v <<endl;
		cout << "last2 prints as: " << last2v <<endl;
	}
}

