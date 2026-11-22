#ifndef _SUCK_TIMER_H
#define _SUCK_TIMER_H 1

const char *TimerFunc(int, long);

enum  timer_funcs { TIMER_START, TIMER_ADDBYTES, TIMER_DISPLAY, TIMER_TOTALS, TIMER_TIMEONLY };

#endif /* _SUCK_TIMER_H */
