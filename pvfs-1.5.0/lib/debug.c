/*
 * debug.c copyright (c) 1996 Rob Ross and Matt Cettei, all rights reserved.
 */

static char debug_c[] = "$Header: /projects/cvsroot/pvfs/lib/debug.c,v 1.2 1999/08/13 23:26:20 rbross Exp $";


#ifdef DEBUG
int debug_on = 1; /* debugging output turned on by default */
#else
int debug_on = 0; /* debugging output turned off by default */
#endif
