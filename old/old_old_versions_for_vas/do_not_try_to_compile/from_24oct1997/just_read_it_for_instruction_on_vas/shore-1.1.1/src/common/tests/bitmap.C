/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */
#include <w.h>
#include <w_base.h>
#include <getopt.h>
#include <stream.h>
#include <basics.h>
#include <assert.h>
#include <bitmap.h>

static void bm_print(ostream &o, u_char *map, const int bits)
{
	static	u_char	mask[] = {
		0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff
		};
	/* XXX perhaps printing in reverse order makes more sense? */

	/* print all but the last byte, which may be partially used */
	int	i;
	for (i = 0; i < bits/8; i++)
		W_FORM(o)("%02x", map[i]);

	/* print only the used portion of the last byte */
	W_FORM(o)("%02x", map[i] & mask[bits % 8]);
}


main(int argc, char **argv)
{
	int	numBits = 71;
	int	numBytes;
	int	i;
	int	errors = 0;
	int	iterations = 0;
	bool	random_tests = false;
	bool	stateless_tests = false;
	int	c;

	while ((c = getopt(argc, argv, "n:ri:")) != EOF) {
		switch (c) {
		case 'n':
			/* number of bits */
			numBits = atoi(optarg);
			break;
		case 'r':
			/* tests using random bit numbers;
			   otherwise tests test all valid
			   bit numbers in order */
			random_tests = true;
			break;
		case 'i':
			/* iterations for each test */
			iterations = atoi(optarg);
			break;
		default:
			errors++;
			break;
		}
	}
	if (errors) {
		W_FORM(cerr)("usage: %s [-n bits] [-r] [-i iterations]\n",
			     argv[0]);
		return 1;
	}
	if (!iterations)
		iterations = random_tests ? 10000 : numBits;

	// tests must leave no state if probes are random, or will
	// repeat through the sample space
	stateless_tests = (random_tests || iterations > numBits);

	numBytes = (numBits - 1) / 8 + 1;

	u_char	*map = new u_char[numBytes];
	if (!map)
		W_FATAL(fcOUTOFMEMORY);

	W_FORM(cout)("test bitmap of %d bits, stored in %d bytes (%d bits).\n",
		     numBits, numBytes, numBytes*8);
	
	for (i = 0; i < numBytes; i++)
		map[i] = rand();

	cout << "Clear Map:    ";
	errors = 0;
	bm_zero(map, numBits);
	for (i = 0; i < numBits; i++)  {
		if (bm_is_set(map, i))  {
			cout << i << " ";
			errors++;
			break;
		}
	}
	cout << (errors ? "failed" : "passed") << endl << flush;
	
	cout << "Set Map:      ";
	errors = 0;
	bm_fill(map, numBits);
	for (i = 0; i < numBits; i++)  {
		if ( ! bm_is_set(map, i))  {
			cout << i << " ";
			errors++;
			break;
		}
	}
	cout << (errors ? "failed" : "passed") << endl << flush;
	
	cout << "Set:          ";
	errors = 0;
	bm_zero(map, numBits);
	for (i = 0; i < iterations; i++)  {
		int	bit = (random_tests ? rand() : i) % numBits;
		bm_set(map, bit);
		if ( ! bm_is_set(map, bit) )  {
			cout << bit << " ";
			errors++;
			break;
		}
	}
	cout << (errors ? "failed" : "passed") << endl << flush;
	
	cout << "Clear:        ";
	errors = 0;
	bm_fill(map, numBits);
	for (i = 0; i < iterations; i++)  {
		int	bit = (random_tests ? rand() : i) % numBits;
		bm_clr(map, bit);
		if (bm_is_set(map, bit))  {
			cout << bit << " ";
			errors++;
			break;
		}
	}
	cout << (errors ? "failed" : "passed") << endl << flush;
	
	cout << "First Set:    ";
	errors = 0;
	bm_zero(map, numBits);
	for (i = 0; i < iterations; i++)  {
		int	bit = (random_tests ? rand() : i) % numBits;
		bm_set(map, bit);
		
		if (bm_first_set(map, numBits, 0) != bit)  {
			cout << bit << " ";
			errors++;
			break;
		}
		
		if (bm_first_set(map, numBits, bit) != bit)  {
			cout << bit << " ";
			errors++;
			break;
		}

		bm_clr(map, bit);
	}
	cout << (errors ? "failed" : "passed") << endl << flush;
	
	cout << "First Clear:  ";
	errors = 0;
	bm_fill(map, numBits);
	for (i = 0; i < iterations; i++)  {
		int	bit = (random_tests ? rand() : i) % numBits;
		bm_clr(map, bit);
		if (bm_first_clr(map, numBits, 0) != bit)  {
			cout << bit << " ";
			errors++;
			break;
		}
		
		if (bm_first_clr(map, numBits, bit) != bit)  {
			cout << bit << " ";
			errors++;
			break;
		}
		bm_set(map, bit);
	}
	cout << (errors ? "failed" : "passed") << endl << flush;

	cout << "Last Set:     ";
	errors = 0;
	bm_zero(map, numBits);
	for (i = 0; i < iterations; i++)  {
		int	bit = (random_tests ? rand() : i) % numBits;
		bm_set(map, bit);
		
		if (bm_last_set(map, numBits, numBits-1) != bit)  {
			cout << bit << " ";
			errors++;
			break;
		}
		
		if (bm_last_set(map, numBits, bit) != bit)  {
			cout << bit << " ";
			errors++;
			break;
		}
		if (stateless_tests)
			bm_clr(map, bit);
	}
	cout << (errors ? "failed" : "passed") << endl << flush;
	
	cout << "Last Clear:   ";
	errors = 0;
	bm_fill(map, numBits);
	for (i = 0; i < iterations; i++)  {
		int	bit = (random_tests ? rand() : i) % numBits;
		bm_clr(map, bit);
		if (bm_last_clr(map, numBits, numBits-1) != bit)  {
			cout << bit << " ";
			errors++;
			break;
		}
		
		if (bm_last_clr(map, numBits, bit) != bit)  {
			cout << bit << " ";
			errors++;
			break;
		}
		if (stateless_tests)
			bm_set(map, bit);
	}
	cout << (errors ? "failed" : "passed") << endl << flush;

	cout << "Num Set:      ";
	errors = 0;
	bm_zero(map, numBits);
	if (bm_num_set(map, numBits) != 0) {
		cout << "all ";
		errors++;
	}
	for (i = 0; i < numBits; i++)  {
		bm_set(map, i);
		
		if (bm_num_set(map, numBits) != i+1)  {
			cout << i << " ";
			errors++;
			break;
		}
	}
	cout << (errors ? "failed" : "passed") << endl << flush;

	cout << "Num Clear:    ";
	errors = 0;
	bm_fill(map, numBits);
	if (bm_num_clr(map, numBits) != 0) {
		cout << "all ";
		errors++;
	}
	for (i = 0; i < numBits; i++)  {
		bm_clr(map, i);
		
		if (bm_num_clr(map, numBits) != i+1)  {
			cout << i << " ";
			errors++;
			break;
		}
	}
	cout << (errors ? "failed" : "passed") << endl << flush;
	
	delete [] map;
	
	cout << flush;
}


