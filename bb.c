#include <string.h>
#include "bb.h"

void bb_init(struct bounded_buffer * bb){

  memset(bb->queue, -1, sizeof(int)*USER_LIMIT);
  bb->head = bb->tail = 0;
  bb->count = 0;
}

int bb_push(struct bounded_buffer * bb, const int p){
  if(bb->count >= USER_LIMIT){
    return -1;
  }

  bb->queue[bb->tail++] = p;
  bb->tail %= USER_LIMIT;
  bb->count++;
  return 0;
}

int bb_pop(struct bounded_buffer * bb){
  const unsigned int pi = bb->queue[bb->head++];
  bb->count--;
  bb->head %= USER_LIMIT;

  return pi;
}

int bb_top(struct bounded_buffer * bb){
  return bb->queue[bb->head];
}

int bb_size(struct bounded_buffer * bb){
  return bb->count;
}

int bb_remove(struct bounded_buffer * bb, const int p){
  int i=0, n = bb_size(bb);
  for(i=0; i < n; i++){
    const int x = bb_pop(bb);
    if(x != p){
      bb_push(bb, x);
    }
  }
  return 0;
}

int bb_data(struct bounded_buffer * bb, int data[USER_LIMIT]){
  int i=0;
  const int n = bb_size(bb);

  for(i=0; i < n; i++){
    data[i] = bb_pop(bb);
    bb_push(bb, data[i]);
  }
  return n;
}
