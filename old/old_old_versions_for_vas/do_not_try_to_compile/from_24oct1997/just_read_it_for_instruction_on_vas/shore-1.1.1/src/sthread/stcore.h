/* --------------------------------------------------------------- */
/* -- Copyright (c) 1994, 1995 Computer Sciences Department,    -- */
/* -- University of Wisconsin-Madison, subject to the terms     -- */
/* -- and conditions given in the file COPYRIGHT.  All Rights   -- */
/* -- Reserved.                                                 -- */
/* --------------------------------------------------------------- */

#ifndef STCORE_H
#define STCORE_H

typedef struct sthread_core_t {
	char	*save_sp;	/* saved stack-pointer */
	int	is_virgin;	/* TRUE if just started */
	void	(*start_proc)(void *);		/* thread start function */
	void	*start_arg;	/* argument for start_proc */
	int	use_float;	/* use floating-point? */
	int	stack_size;	/* stack size */
	char	*stack;		/* stack */
	char	*stack0;	/* initial stack pointer */
	void	*thread;	/* thread which uses this core */
} sthread_core_t;


extern int  sthread_core_init(sthread_core_t* t,
			      void (*proc)(void *), void *arg,
			      unsigned stack_size);

extern void sthread_core_exit(sthread_core_t *t);

extern void sthread_core_fatal();

extern void sthread_core_set_use_float(sthread_core_t *core, int flag);

extern void sthread_core_switch(sthread_core_t *from, sthread_core_t *to);

extern int sthread_core_stack_ok(const sthread_core_t *core, int onstack);

extern int stack_grows_up;
extern int minframe;

#endif /*STCORE_H*/
