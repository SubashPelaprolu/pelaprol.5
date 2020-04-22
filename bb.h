#ifndef BB_H
#define BB_H

#include "user.h"

struct bounded_buffer {
	int queue[USER_LIMIT];
	int head, tail;
	int count;
};

void bb_init(struct bounded_buffer * bb);

int bb_push(struct bounded_buffer * bb, const int p);

int bb_pop(struct bounded_buffer * bb);
int bb_top(struct bounded_buffer * bb);

int bb_size(struct bounded_buffer * bb);
int bb_remove(struct bounded_buffer * bb, const int p);
int bb_data(struct bounded_buffer * bb, int data[USER_LIMIT]);

#endif
