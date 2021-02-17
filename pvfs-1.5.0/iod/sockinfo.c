#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <dfd_set.h>

/* GLOBALS */
static dyn_fdset act_set;
static int act_set_max;

int is_active(int s)
{
	return(dfd_isset(s, &act_set));
}

int set_active(int s)
{
	static int init = 1;

	/* make sure to zero everything the first time we're called */
	/* IT WOULD BE BETTER IF THERE WERE AN INIT FN FOR THIS CODE */
	if (init) {	
		act_set_max = 0;
		dfd_init(&act_set, 32);
		init = 0;
	}

	dfd_set(s, &act_set);
	if (s >= act_set_max) act_set_max = s+1;
	return(1);
}

int clr_active(int s)
{
	dfd_clr(s, &act_set);
	if (s+1 >= act_set_max) /* need to reduce act_set_max */ {
		int i;
		for (i=s; i >= 0; i--) if (dfd_isset(i, &act_set)) break;
		act_set_max = i+1;
	}
	return(1);
}

/* THERE REALLY OUGHT TO BE SOME CLEANUP FN FOR THIS CODE */

