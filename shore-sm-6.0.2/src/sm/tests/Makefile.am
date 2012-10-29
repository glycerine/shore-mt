## Process this file with automake to produce Makefile.in
include $(top_srcdir)/Makefile.generic
RESULT_COMPARISON_FILES = \
						  lockid_test-out.x86_64 \
						  lockid_test-out.sun4v \
						  lock_cache_test-out \
						  htab-n500-out.1024 \
						  htab-n500-out.2048 \
						  htab-n500-out.4096 \
						  htab-n500-out.8192 \
						  htab-n500-out.16384 \
						  htab-n500-out.32768 \
						  htab-n10000-R-out \
						  htab-n100000-R-out 
						  
EXTRA_DIST = testall EXAMPLE_SHORECONFIG $(RESULT_COMPARISON_FILES)

## no-op rule for making the -out files
## this is to avoid automagic makefiles thinking that the -out
## files are executable files to be made from the cpp files of the same
## prefix
$(RESULT_COMPARISON_FILES) : noop

noop:

CLEANFILES = ./log/* ./volumes/*

AM_CXXFLAGS          +=  \
		 -I$(top_srcdir)/src/fc \
		 -I$(top_srcdir)/src/sthread \
		 -I$(top_srcdir)/src/common  \
		 -I$(top_srcdir)/src/sm  \
		 -I/s/gtest/gtest-1.6.0/include 

# For maintainer only:
# LIBS +=  /s/gtest/gtest-1.6.0/lib/.libs/libgtest.a

check_PROGRAMS     = \
		    startstop$(EXEEXT) \
		    create_rec$(EXEEXT) \
		    sort_stream$(EXEEXT) \
		    file_scan_many$(EXEEXT) \
		    log_exceed$(EXEEXT) \
		    lockid_test$(EXEEXT) \
		    lock_cache_test$(EXEEXT) \
		    vtable_example$(EXEEXT) \
		    rtree_example$(EXEEXT) \
		    sm_pagesize$(EXEEXT) \
		    htab$(EXEEXT) 

check_PROGRAMS += hashes$(EXEEXT) 

TESTS = testall

hashes_SOURCES      = hashes.cpp 
sm_pagesize_SOURCES      = sm_pagesize.cpp 
lockid_test_SOURCES      = lockid_test.cpp 
lock_cache_test_SOURCES      = lock_cache_test.cpp 
startstop_SOURCES      = startstop.cpp 
file_scan_many_SOURCES      = file_scan_many.cpp init_config_options.cpp 
create_rec_SOURCES      = create_rec.cpp init_config_options.cpp 
sort_stream_SOURCES      = sort_stream.cpp init_config_options.cpp 
log_exceed_SOURCES      = log_exceed.cpp init_config_options.cpp 
vtable_example_SOURCES      = vtable_example.cpp init_config_options.cpp 
rtree_example_SOURCES      = rtree_example.cpp init_config_options.cpp 

htab_SOURCES      = htab.cpp

LDADD      = \
	$(top_builddir)/src/sm/libsm.a  \
	$(top_builddir)/src/common/libcommon.a  \
	$(top_builddir)/src/sthread/libsthread.a  \
	$(top_builddir)/src/fc/libfc.a  \
	$(LOCALLDADD)

file_scan_many.pure:  
	$(PURIFY_BIN)/purify $(PURIFY_FLAGS)  -collector=$(PURIFY_COLLECTOR)  $(CXX) -o $@ $(file_scan_many_SOURCES:.cpp=.o) $(LDADD) $(LIBS)

create_rec.pure:  
	$(PURIFY_BIN)/purify $(PURIFY_FLAGS)  -collector=$(PURIFY_COLLECTOR)  $(CXX) -o $@ $(create_rec_SOURCES:.cpp=.o) $(LDADD) $(LIBS)

lockid_test.pure:  
	$(PURIFY_BIN)/purify $(PURIFY_FLAGS)  -collector=$(PURIFY_COLLECTOR)  $(CXX) -o $@ $(lockid_test_SOURCES:.cpp=.o) $(LDADD) $(LIBS)

lock_cache_test.pure:  
	$(PURIFY_BIN)/purify $(PURIFY_FLAGS)  -collector=$(PURIFY_COLLECTOR)  $(CXX) -o $@ $(lock_cache_test_SOURCES:.cpp=.o) $(LDADD) $(LIBS)

sort_stream.pure:  
	$(PURIFY_BIN)/purify $(PURIFY_FLAGS)  -collector=$(PURIFY_COLLECTOR)  $(CXX) -o $@ $(sort_stream_SOURCES:.cpp=.o) $(LDADD) $(LIBS)

log_exceed.pure:  
	$(PURIFY_BIN)/purify $(PURIFY_FLAGS)  -collector=$(PURIFY_COLLECTOR)  $(CXX) -o $@ $(log_exceed_SOURCES:.cpp=.o) $(LDADD) $(LIBS)

vtable_example.pure:  
	$(PURIFY_BIN)/purify $(PURIFY_FLAGS)  -collector=$(PURIFY_COLLECTOR)  $(CXX) -o $@ $(vtable_example_SOURCES:.cpp=.o) $(LDADD) $(LIBS)

rtree_example.pure:  
	$(PURIFY_BIN)/purify $(PURIFY_FLAGS)  -collector=$(PURIFY_COLLECTOR)  $(CXX) -o $@ $(rtree_example_SOURCES:.cpp=.o) $(LDADD) $(LIBS)

startstop.pure:  
	$(PURIFY_BIN)/purify $(PURIFY_FLAGS)  -collector=$(PURIFY_COLLECTOR)  $(CXX) -o $@ $(startstop_SOURCES:.cpp=.o) $(LDADD) $(LIBS)
