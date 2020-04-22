#include <time.h>

enum { TIMER_CPU=0, TIMER_SYSTEM, TIMER_BURST, TIMER_STARTED, TIMER_IOBLOCK, TIMER_READY, TIMER_COUNT };

//Like timeval operations, but for timespec ( man timeradd )
int  timercmp(const struct timespec *a, const struct timespec *b);
void timeradd(struct timespec * res, const struct timespec * b);
void timersub(struct timespec *res, const struct timespec *a, const struct timespec *b);
void timerdiv(struct timespec *res, const unsigned int d);
void timerzero(struct timespec *res);
