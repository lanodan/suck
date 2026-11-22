#include <config.h>


#include <stdio.h>
#include "suck_config.h"

#ifdef TIMEOUT
# if TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
# else
#  if HAVE_SYS_TIME_H
#    include <sys/time.h>
#  else
#    include <time.h>
#  endif
# endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include "timer.h"

/*internal functions */
double get_elapsed(struct timeval *);

/*-----------------------------------------------------------*/
const char *TimerFunc(int which_function, long nradd) { 

	static struct timeval start;
	static long nrbytes = 0L;	/* just in case */
	static char retdisp[256];
	static char none[] = { "" };	

	double elapsed, bps;
	long mins;		/* long so can get mins to exact decimal */
	char *retval = none;

	switch(which_function) {
	  case TIMER_START:
		nrbytes = 0L;
		gettimeofday(&start, NULL);
		break;
	  case TIMER_ADDBYTES:
		nrbytes += nradd;
		break;
	  case TIMER_DISPLAY:
		if(nrbytes > 0) {
			elapsed = get_elapsed(&start);
			bps = (elapsed > 0.0) ? bps = nrbytes / elapsed : 0.0;
			sprintf(retdisp,"%10.1f BPS", bps);
			retval = retdisp;
		}
		break;
	  case TIMER_TIMEONLY:
		elapsed = get_elapsed(&start);
		mins = ((long) elapsed) / 60 ;		/* get minutes */
		elapsed -= (mins * 60);		/* subtract to get remainder */
		sprintf(retdisp, "Elapsed Time = %ld mins %.2f seconds", mins, elapsed);
		retval = retdisp;
		break;
	  case TIMER_TOTALS:
		elapsed = get_elapsed(&start);
		bps = (elapsed > 0.0 && nrbytes > 0) ? bps = nrbytes / elapsed : 0.0;

		mins = ((long) elapsed) / 60 ;		/* get minutes */
		elapsed -= (mins * 60);		/* subtract to get remainder */
		sprintf(retdisp, "%ld Bytes Received in %ld mins %.2f seconds, BPS = %-10.1f", nrbytes, mins, elapsed, bps);
		retval = retdisp;
		break;	
	  default:
		/* ignore invalid commands */
		break;
	}
	return retval;
}
/*-----------------------------------------------------------------------------------*/
double get_elapsed(struct timeval *start) {
	struct timeval curr;
	double elapsed;

	/* compute elapsed time, in seconds */
	gettimeofday(&curr, NULL);
	elapsed = curr.tv_sec - start->tv_sec;
	elapsed += (((double) (curr.tv_usec - start->tv_usec)) / 1000000.0);

	return elapsed;
}
