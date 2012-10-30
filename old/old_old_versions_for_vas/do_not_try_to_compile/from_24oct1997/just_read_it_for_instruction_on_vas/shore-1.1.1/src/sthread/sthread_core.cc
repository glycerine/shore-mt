/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

/*
 *  $Id: sthread_core.cc,v 1.21 1997/06/15 02:26:22 solomon Exp $
 */

#define STHREAD_CORE_C

#include <w.h>
#include <w_fastnew.h>

#include "sthread.h"
#include "stcore.h"


#if defined(DEBUG)
static void sthread_core_stack_setup(sthread_core_t *core);
#endif

/*
   Bolo's note about "zone"s and "redzone"s on thread stacks.  I use the
   "zone" terminology for two things:
   1) A zone of unused space between thread stacks.  This keeps
   stacks far enough apart that purify doesn't think one thread
   is using another thread's stack.
   2) A redzone at the "far" end of the thread stack.  This is used
   to check for stack overflow at runtime.  An intrusion of the stack
   pointer into this region, or a corruption of the pattern stored
   in this region is noted as a stack overflow.
 */

#ifdef PURIFY

/* If stacks are too close together, purify thinks that any data is in
   a, beyond, or ahead of the thread stack.  Placing a buffer
   zone on each end of the stack prevents most of this problem.  This
   allows purifying the code without bogus purify events. */

#define	ZONE_STACK_SIZE		(16*1024)
#else
#define	ZONE_STACK_SIZE		0
#endif

#define	USER_STACK_SIZE		(sthread_t::default_stack)

#define	DEFAULT_STACK_SIZE	(2*ZONE_STACK_SIZE + USER_STACK_SIZE)


unsigned	sthread_t::stack_size() const
{
	/* XXX give the main thread a !=0 stack size for now */
	return _core.stack_size ? _core.stack_size : (int) default_stack;
}


struct thread_stack {
public:
    char	data[DEFAULT_STACK_SIZE];
    W_FASTNEW_CLASS_PTR_DECL;
};
W_FASTNEW_STATIC_PTR_DECL(thread_stack);
// force sthread_init_t to be constructed after thread_stack fastnew
static sthread_init_t sthread_init;


int sthread_core_init(sthread_core_t *core,
		      void (*proc)(void *), void *arg,
		      unsigned stack_size)
{
	if (thread_stack::_w_fastnew == 0) {
		thread_stack::_w_fastnew =  new w_fastnew_t(sizeof(thread_stack), 4, __LINE__, __FILE__);
		if (thread_stack::_w_fastnew == 0)
			W_FATAL(fcOUTOFMEMORY);
	}

	/* Get a life; XXX magic number */
	if (stack_size > 0 && stack_size < 1024)
		return -1;

	core->is_virgin = 1;

	core->start_proc = proc;
	core->start_arg = arg;

	core->use_float = 1;

	if (stack_size == USER_STACK_SIZE) {
		thread_stack *ts = new thread_stack;
		if (!ts)
			return -1;
		core->stack = ts->data + ZONE_STACK_SIZE;
	}
	else if (stack_size > 0) {
		char *ts = new char[stack_size + 2*ZONE_STACK_SIZE];
		if (!ts)
			return -1;
		core->stack = ts + ZONE_STACK_SIZE;
	}
	else
		core->stack = 0;
	core->stack_size = stack_size;

#if defined(DEBUG)  
#ifdef PURIFY
	if(!purify_is_running()) {
#endif
		sthread_core_stack_setup(core);
#ifdef PURIFY
	}
#endif
#endif

	if (stack_size > 0) {
		core->stack0 = (core->stack +
				((stack_grows_up ? 32 :
				  core->stack_size - 32)));
		core->save_sp = core->stack0;
	}
	else {
		/* A more elegant solution would be to make a
		   "fake" stack using the kernel stack origin
		   and stacksize limit.   This could also allow
		   the main() stack to have a thread-package size limit,
		   to catch memory hogs in the main thread. */
		core->stack0 = core->save_sp = core->stack;

		/* The system stack is never virgin */
		core->is_virgin = 0;
	}

	return 0;
}


extern "C" {
/* A callback to c++ land used by errors in the c-only core code */

void sthread_core_fatal()
{
	W_FATAL(fcINTERNAL);
}
}


void sthread_core_exit(sthread_core_t* core)
{
	if (core->stack_size == USER_STACK_SIZE) {
		thread_stack *ts = (thread_stack *) (core->stack - ZONE_STACK_SIZE);
		delete ts;
	}
	else if (core->stack_size > 0) {
		char	*ts = core->stack - ZONE_STACK_SIZE;
		delete ts;
	}
}


void sthread_core_set_use_float(sthread_core_t* p, int flag)
{
	p->use_float = flag ? 1 : 0;
}


/*
 * address_in_stack(core, addr)
 *
 * Indicate if the specified address is in the stack of the thread core
 */  

bool address_in_stack(sthread_core_t &core, void *address)
{
	if (core.stack_size == 0)
		return false;

	return ((char *)address >= core.stack &&
		(char *)address < core.stack + core.stack_size);
}



/* ZONE_USED / ZONE_DIV == portion of stack with redzone pattern */
#define	ZONE_DIV	8
#define	ZONE_USED	1

static inline void redzone(const sthread_core_t *core, char **start, int *size)
{
	int	zone_size = (ZONE_USED * core->stack_size) / ZONE_DIV;
	char	*zone;

	zone = core->stack;

	if (stack_grows_up)
		zone += (ZONE_DIV - ZONE_USED) * zone_size;

	*start = zone;
	*size = zone_size;
}


#if defined(DEBUG)  

#define PATTERN	0xdeaddead 

/*
   Install a pattern on the last 3/4 of the stack, so we can examine
   it to see if anything has been written into the area
 */  

static void sthread_core_stack_setup(sthread_core_t *core)
{
	char		*zone;
	int		zone_size;
	int		*izone;
	unsigned int	i;

	if (core->stack_size == 0)
		return;

	redzone(core, &zone, &zone_size);
	izone = (int *)zone;
	
	for (i = 0; i < zone_size/sizeof(int); i++)
		izone[i] = PATTERN;
}
#endif /* DEBUG */


/*
   Determine if the thread stack is ok:
   1) stack pointer within region
   2) redzone secure
 */  

int	sthread_core_stack_ok(const sthread_core_t *core, int onstack)
{
	char	*zone;
	int	zone_size;
	char	*sp;		/* bogus manner of retrieving current sp */
	int	overflow;

	if (core->stack_size == 0)
		return 1;

	redzone(core, &zone, &zone_size);

	if (onstack)
		sp = (char *) &sp;
	else
		sp = core->save_sp;

	if (stack_grows_up)
		overflow = sp > zone;
	else
		overflow = sp < (zone + zone_size);

	if (overflow) {
		cerr << "obvious overflow" << endl;
		return 0;
	}

#if defined(DEBUG)  
#ifdef PURIFY
	/* stack checks don't work if purify is running */
	if (purify_is_running())
		return 1;
#endif
	unsigned int	*izone = (unsigned int *)zone;
	unsigned int	i;
	
	for (i = 0; i < zone_size / sizeof(int); i++)
		if (izone[i] != PATTERN) {
#ifdef want_more_info			
			cerr.form("corrupt zone: core %#x, %#x at %d of %d\n",
				  (long)core,
				  (long)zone,
				  i, zone_size/sizeof(int));
#endif
			return 0;
		}
#endif

	return 1;
}

ostream &operator<<(ostream &o, const sthread_core_t &core)
{
	o << "core: ";
	if (core.stack_size == 0)
		o.form("[ system stack, sp=%#lx ]", (long) core.save_sp);
	else
		o.form("[ %#lx ... %#lx ... %#lx ] size=%d, used=%d",  
		       (long)core.stack,
		       (long)core.save_sp,
		       (long)(core.stack + core.stack_size -1),
		       core.stack_size,
		       stack_grows_up ?
		       (long)core.save_sp - (long)core.stack :
		       (long)core.stack + core.stack_size - (long)core.save_sp
		       );
	if (core.is_virgin)
		o << ", virgin-core";
	return o;
}
	
