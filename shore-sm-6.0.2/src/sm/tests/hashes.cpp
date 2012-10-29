#define SM_SOURCE
#include "sm_int_4.h"
#include "btree.h"
#include "btcursor.h"
#include "page_s.h"
#include "bf.h"
#include "btree_p.h"
#include "xct.h"
#include "w_rusage.h"

// #include "gtest/gtest.h"
#include <set>
#include <map>

#undef EXPECT_LT
#define EXPECT_LT(a,b) { if(false==((a)<(b))) { cout << "EXPECT-FAILURE "<<  #a <<  "<" << #b << endl; } }
#undef EXPECT_EQ
#define EXPECT_EQ(a,b) { if(false==((a)==(b))) { cout << "EXPECT-FAILURE " << #a << "==" << #b << endl; } }
#undef EXPECT_NE
#define EXPECT_NE(a,b) { if(true==((a)==(b))) { cout << "EXPECT-FAILURE " << #a << "!=" << #b << endl; } }


#define MAXN 1000
static char buf[MAXN];
static int COUNT;
int MAXCOUNTKVL = 1000000;

long int max_vol = 100;
long int max_store = 10000;
long int max_page = 400;
long int max_rec = 32768;

vid_t    vol(1000);
stid_t   stor(vol,900);

void dump(lockid_t &l)
{
    cout << "Lock: " << endl;
    cout << "\t vid()=" << l.vid() << endl;

    stid_t stor;
    l.extract_stid(stor);
    cout << "\t store()=" << l.store() << " stid_t=(" << stor << ")" << endl;
}


void SimpleKey()
{
    const char     *keybuf = "Admiral Richard E. Byrd";
    const char     *keybuf2= "Most of the confusion in the world comes from not knowing how little we need.";
	vec_t    key(keybuf, strlen(keybuf));
	vec_t    key2(keybuf2, strlen(keybuf2));
	kvl_t kvl;

    cout << "Sources: " << endl
        <<  "\t vol " << vol << endl
        <<  "\t store " << stor << endl
        ;
    {
		kvl.set(stor, key);
        lockid_t l(kvl);
		if(0) {
        cout << "Key lock1 " << l << endl;
		}
    }
    {
		kvl.set(stor, key2);
        lockid_t l(kvl);
		if(0) {
        cout << "Key lock2 " << l << endl;
		}
    }
}

void SameKey() 
{
    const char *buf = "Don't think, Feel";
	vec_t    key(buf, strlen(buf));
    {
		kvl_t kvl;
		kvl.set(stor, key);
        lockid_t l(kvl);
        lockid_t l2(kvl);
        EXPECT_EQ(l.hash(), l2.hash());
    }
    {
        stid_t   stor2(vol,10);
		kvl_t kvl, kvl2;
		kvl.set(stor, key);
        lockid_t l(kvl);
		kvl2.set(stor2, key);
        lockid_t l2(kvl2);
        EXPECT_NE(l.hash(), l2.hash());
    }
}

void CollisionQualityPrefix(int n)
{
    std::set<uint32_t> observed;
	w_assert0(n + 7 < MAXN);
	w_assert0(COUNT <= 1000000);
// Populate the key prefix with something of length n
// We want to see how well it does with large prefixes in common
	for(int i=0; i < n+7; i++) {
		buf[n] = char('a'+ (n % 25));
	}
    // buf [0] = 'k';
    // buf [1] = 'e';
    // buf [2] = 'y';
	vec_t    key(buf, n+7);
	kvl_t    kvl;

	// The last 7 bytes define up to a million unique key values
    for (int i = 0; i < COUNT; ++i) {
        buf[0+n] = '0' + ((i / 1000000) % 10);
        buf[1+n] = '0' + ((i / 100000) % 10);
        buf[2+n] = '0' + ((i / 10000) % 10);
        buf[3+n] = '0' + ((i / 1000) % 10);
        buf[4+n] = '0' + ((i / 100) % 10);
        buf[5+n] = '0' + ((i / 10) % 10);
        buf[6+n] = '0' + ((i / 1) % 10);
		kvl.set(stor, key);
        lockid_t l(kvl);
		uint32_t h = l.hash();
		if(0) {
				cout 
					<< h 
					<< " i=" << i 
					<< " buf=" << buf
					<< " kvl=" << kvl 
					<< endl;
		}
        observed.insert(h);

    }
    double collision_rate = ((double)COUNT - observed.size()) / (double) COUNT;
    std::cout << "fixed-prefix Seq(" << n << "): distinct hashes " << observed.size()
        << " out of " << COUNT
        << "(collision rate=" << collision_rate * 100.0f << "%)" << endl;
    EXPECT_LT (collision_rate, 0.00005f);
}

void CollisionQualitySuffix(int n)
{
    std::set<uint32_t> observed;
	w_assert0(n + 7 < MAXN);
	w_assert0(COUNT <= 1000000);
// Populate the key suffix with something of length n
// We want to see how well it does with large prefixes in common
	for(int i=0; i < n+7; i++) {
		buf[n] = char('a'+ (n % 25));
	}
    // buf [0] = 'k';
    // buf [1] = 'e';
    // buf [2] = 'y';
	vec_t    key(buf, n+7);
	kvl_t    kvl;

	// The first 7 bytes define up to a million unique key values
    for (int i = 0; i < COUNT; ++i) {
        buf[0] = '0' + ((i / 1000000) % 10);
        buf[1] = '0' + ((i / 100000) % 10);
        buf[2] = '0' + ((i / 10000) % 10);
        buf[3] = '0' + ((i / 1000) % 10);
        buf[4] = '0' + ((i / 100) % 10);
        buf[5] = '0' + ((i / 10) % 10);
        buf[6] = '0' + ((i / 1) % 10);
		kvl.set(stor, key);
        lockid_t l(kvl);
		uint32_t h = l.hash();
		if(0) {
				cout 
					<< h 
					<< " i=" << i 
					<< " buf=" << buf
					<< " kvl=" << kvl 
					<< endl;
		}
        observed.insert(h);
    }
    double collision_rate = (COUNT - observed.size()) / (double) COUNT;
    std::cout << "fixed-suffix Seq(" << n << "): distinct hashes " << observed.size()
        << " out of " << COUNT
        << "(collision rate=" << collision_rate * 100.0f << "%)" << endl;
    EXPECT_LT (collision_rate, 0.00005f);
}

void CollisionQualityRandomKey()
{
	// typedef std::set<uint32_t> observed_t;
	typedef std::map<uint32_t, uint32_t> observed_t;
    observed_t observed;
    std::set<uint32_t> used_randoms;
	vec_t    key(buf, 10);
	kvl_t    kvl;

    buf [0] = 'k';
    buf [1] = 'e';
    buf [2] = 'y';
    buf [3] = 'z';
    buf [8] = 'b';
    buf [9] = 'c';
    
    ::srand(1233); // use fixed seed for repeatability
    for (int i = 0; i < COUNT; ++i) {
		// find an unused value:
        uint32_t val;
        while (true) {
            val = ((uint32_t) ::rand() << 16) + ::rand();
            if (used_randoms.find(val) == used_randoms.end()) {
				used_randoms.insert(val);
                break;
            }
        }
        *reinterpret_cast<uint32_t*>(buf + 4) = val;
		kvl.set(stor, key);
        lockid_t l(kvl);
		uint32_t h = l.hash();
		observed_t::iterator it;
		it=observed.find(h);
        if (it != observed.end()) {
			if(0) {
                std::cout << "collide! " << val <<  " with " <<
				it->second << endl;
			}
        }
		if(0) {
				cout 
					<< h 
					<< " i=" << i 
					<< " val=" << val
					<< " kvl=" << kvl 
					<< endl;
		}
        observed.insert(std::pair<uint32_t,uint32_t>(h, val));
    }
    double collision_rate = (COUNT - observed.size()) / (double) COUNT;
    std::cout << "Rnd: distinct hashes " << observed.size()
        << " out of " << COUNT
        << "(collision rate=" << collision_rate * 100.0f << "%)" << endl;
    EXPECT_LT (collision_rate, 0.00005f);
}

void SpeedRandomKey() 
{
	vec_t    key(buf, 10);
	kvl_t    kvl;

    buf [0] = 'k';
    buf [1] = 'e';
    buf [2] = 'y';
    buf [3] = 'z';
    buf [8] = 'b';
    buf [9] = 'c';
    
    ::srand(1233); // use fixed seed for repeatability

	unix_stats U; U.start();
    for (int i = 0; i < COUNT*1000; ++i) {
		// find an unused value:
        uint32_t val;
		val = ((uint32_t) ::rand() << 16) + ::rand();
        *reinterpret_cast<uint32_t*>(buf + 4) = val;
		kvl.set(stor, key);
        lockid_t l(kvl);
		(void) l.hash();
    }
	U.stop(1/*iteration*/);
	cout << "Unix stats " << U << endl;
}

void CollisionQualityID()
{
	const int DUMP=0;
	typedef std::map<uint32_t, lockid_t> observed_t;
    // typedef std::set<uint32_t> observed_t;
    observed_t observed;
	observed_t::iterator it;

	int count=0;
	lockid_t l;
	uint32_t h;

	// volumes
    for (int v = 1; v < max_vol; ++v) {
		if(count >= COUNT) break;

		vid_t   vol( v % 32768 );

		l.zero();
		l = lockid_t(vol);
		h = l.hash();

		it=observed.find(h);
        if (it != observed.end()) {
			std::cout << "collide! " << l 
				<< " with " << it->second 
				<< " hash = " << h
				<< endl;
        }
		if(DUMP) { cout << h << " vol " << vol << endl; }
		// observed.insert(h);
        observed.insert(std::pair<uint32_t,lockid_t>(h, l));
		count++;

		// stores 
		for (int s = 1; s < max_store; ++s) {
			if(count >= COUNT) break;
			snum_t  snum(s);

			stid_t stid(l.vid(), snum);
			l = lockid_t(stid);
			h = l.hash();
			it=observed.find(h);
			if (it != observed.end()) {
				std::cout << "collide! " << l 
				<< " with " << it->second 
				<< " hash = " << h
				<< endl;
			}
			if(DUMP) { cout << h << " stid " << stid << endl; }
			// observed.insert(h);
			observed.insert(std::pair<uint32_t,lockid_t>(h, l));
			count++;

			// pages & extents
			for (int p = 1; p < max_page; ++p) {
				if(count >= COUNT) break;
				shpid_t pnum(p%32768 );

				lpid_t pid(stid, pnum);
				l = lockid_t(pid);
				h = l.hash();
				it=observed.find(h);
				if (it != observed.end()) {
					std::cout << "collide! " << l 
					<< " with " << it->second 
					<< " hash = " << h
					<< endl;
				}
				if(DUMP) { cout << h << " pid " << pid << endl; }
				// observed.insert(h);
				observed.insert(std::pair<uint32_t,lockid_t>(h, l));
				count++;

				extid_t ext;
				ext.vol= vol;
				ext.ext = p%32768;

				l = lockid_t(ext);
				h = l.hash();
				it=observed.find(h);
				if (it != observed.end()) {
					std::cout << "collide! " << l 
					<< " with " << it->second 
					<< " hash = " << h
					<< endl;
				}
				if(DUMP) { cout << h << " ext " << ext << endl; }

				// observed.insert(h);
				observed.insert(std::pair<uint32_t,lockid_t>(h, l));
				count++;

				for (int t = 1; t < max_rec; ++t) {
					if(count >= COUNT) break;
					slotid_t slot(t%32768 );
					rid_t rid(pid, slot);
					l = lockid_t(rid);
					h = l.hash();
					it=observed.find(h);
					if (it != observed.end()) {
						std::cout << "collide! " << l 
						<< " with " << it->second 
						<< " hash = " << h
						<< endl;
					}
					if(0 && DUMP) { cout << h << " rid " << rid << endl; }
					// observed.insert(h);
					observed.insert(std::pair<uint32_t,lockid_t>(h, l));
					count++;
				}
			} // pages
		} // slots
	} // volumes

    double collision_rate = ((double)count - observed.size()) / (double) count;
    std::cout << "RID: distinct hashes " << observed.size()
        << " out of " << count
        << "(collision rate=" << collision_rate * 100.0f << "%)" << endl;
    EXPECT_LT (collision_rate, 0.00005f);
}

void CollisionQualitySTID()
{
	const int DUMP=0;
	typedef std::map<uint32_t, stid_t> observed_t;
    observed_t observed;
	observed_t::iterator it;

	int count=0;
	lockid_t l;

	// volumes
    for (int v = 1; v < max_vol; ++v) {
		if(count >= COUNT) break;

		vid_t   vol( v % 32768 );

		// only one store for bfpid since store is ignored.
		for (int s = 1; s < max_store; ++s) {
			stid_t stid(vol, s);
			if(count >= COUNT) break;
			uint32_t h = stid.hash();
			it=observed.find(h);
			if (it != observed.end()) {

				std::cout 
				<< "collide! " << stid 
				<< " with " << it->second 
				<< " h=" << h
				<< endl;
			}
			if(DUMP) { cout << h << " stid " << stid << endl; }
			observed.insert(std::pair<uint32_t,stid_t>(h, stid));
			count++;
		} //stores
	} // volumes

    double collision_rate = ((double)count - observed.size()) / (double) count;
    std::cout << "STID: distinct hashes " << observed.size()
        << " out of " << count
        << "(collision rate=" << collision_rate * 100.0f << "%)" << endl;
    EXPECT_LT (collision_rate, 0.00005f);
}

void CollisionQualityBFPID()
{
	const int DUMP=0;
	typedef std::map<uint32_t, bfpid_t> observed_t;
    observed_t observed;
	observed_t::iterator it;

	int count=0;
	lockid_t l;

	// volumes
    for (int v = 1; v < max_vol; ++v) {
		if(count >= COUNT) break;

		vid_t   vol( v % 32768 );

		// only one store for bfpid since store is ignored.
		for (int s = 1; s < 2; ++s) {
			stid_t stid(vol, s);
			// pages 
			for (int p = 1; p < 32768; ++p) {
				if(count >= COUNT) break;
				shpid_t pnum(p%32768 );

				lpid_t pid(stid, pnum);

				bfpid_t bfpid (pid);
				uint32_t h = bfpid.hash();
				it=observed.find(h);
				if (it != observed.end()) {
					std::cout 
					<< "collide! " << bfpid 
					<< " with " << it->second 
					<< " h=" << h
					<< endl;
				}
				if(DUMP) { cout << h << " bfpid " << pid << endl; }
				observed.insert(std::pair<uint32_t,bfpid_t>(h, bfpid));
				count++;

			} // pages
		} // slots
	} // volumes

    double collision_rate = ((double)count - observed.size()) / (double) count;
    std::cout << "BFPID: distinct hashes " << observed.size()
        << " out of " << count
        << "(collision rate=" << collision_rate * 100.0f << "%)" << endl;
    EXPECT_LT (collision_rate, 0.00005f);
}

int main(int argc, char ** argv) {
	if(argc < 2) {
		cout << "Usage: " << argv[0] << " <COUNT>" << endl;
		return 1;
	}
	long int count = strtol(argv[1], NULL, 0);

	cout 
		// HIDEAKI_HASHES defined in lock_s_inline.h
		<< " lockid_t hash VERSION  "  << 
#if HIDEAKI_HASH==1
		1
#else
		0
#endif 
		<< "; count=" << count << endl;
		;
#define ID_LOCK_HASH 1
	if(ID_LOCK_HASH) {
		// tests for volume, store, page and rid locks.
		COUNT = count;
		if(count > max_vol * max_store * max_page * max_rec)
			COUNT = max_vol * max_store * max_page * max_rec;

		cout <<  "Using ID COUNT=" << COUNT << endl;
		CollisionQualityID();
	}
#define STID_HISTOID_HASH 1
	if(STID_HISTOID_HASH) {
		// tests for stid_t (hashed for histoid)
		COUNT = count;
		if(count > max_vol * max_store)
			COUNT = max_vol * max_store;

		cout <<  "Using STID COUNT=" << COUNT << endl;
		CollisionQualitySTID();
	}
#define BFPID_HASH 1
	if(BFPID_HASH) {
		// tests for bfpid_t
		COUNT = count;
		if(count > max_vol * 32768)
			COUNT = max_vol * 32768;

		cout <<  "Using BFPID COUNT=" << COUNT << endl;
		CollisionQualityBFPID();
	}

#define KVL_HASH 1
	if(KVL_HASH) {
		if(count >= 0 && count <= MAXCOUNTKVL) 
		{
			COUNT = count;
		} else if(count > MAXCOUNTKVL) {
			COUNT = MAXCOUNTKVL;
		} else {
			cerr << count 
				<< ": COUNT out of range [0-" << MAXCOUNTKVL << ") " << endl;
			return 1;
		}
		cout <<  "KVL COUNT=" << COUNT << endl;

		// tests for kvl locks.
		SimpleKey();
		SameKey();
		cout << endl;
		// argument is the #bytes of prefix/suffix that's fixed.
		// CollisionQualityPrefix(0); must give at least enough
		// space for unique keys
		CollisionQualityPrefix(0);
		CollisionQualityPrefix(10);
		CollisionQualityPrefix(100);
		CollisionQualityPrefix(500);
		cout << endl;
		CollisionQualitySuffix(0);
		CollisionQualitySuffix(10);
		CollisionQualitySuffix(100);
		CollisionQualitySuffix(500);
		cout << endl;
		CollisionQualityRandomKey();
		cout << endl;
		SpeedRandomKey();
	}


	return 0;
}

