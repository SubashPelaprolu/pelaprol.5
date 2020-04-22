#include <stdlib.h>
#include "timer.h"

int timercmp(const struct timespec *a, const struct timespec *b){
  if(	(b->tv_sec  < a->tv_sec) ||
     ((b->tv_sec == a->tv_sec) && (b->tv_nsec <= a->tv_nsec)) ){
    return 1; //less
  }else{
    return 0; //equal
  }
  return -1;  //greater
}

void timeradd(struct timespec * res, const struct timespec * a){
  const unsigned int ns_in_sec = 1000000000;

  res->tv_sec  += a->tv_sec;
  res->tv_nsec += a->tv_nsec;
  if(res->tv_nsec > ns_in_sec){
    res->tv_sec++;
    res->tv_nsec %= ns_in_sec;
  }
}

void timersub(struct timespec *res, const struct timespec *a, const struct timespec *b){

  res->tv_sec  = a->tv_sec  - b->tv_sec;
  if ((a->tv_nsec - b->tv_nsec) < 0) {
    res->tv_sec -= 1;
    res->tv_nsec = b->tv_nsec - a->tv_nsec;
  } else {
    res->tv_nsec = a->tv_nsec - b->tv_nsec;
  }
}

void timerdiv(struct timespec *res, const unsigned int d){
  if(d > 0){
    res->tv_sec  = res->tv_sec / d;
    res->tv_nsec = res->tv_nsec / d;
  }
}

void timerzero(struct timespec *res){
  res->tv_sec = 0;
  res->tv_nsec= 0;
}
