#ifndef COMMON_H
#define COMMON_H

#include "user.h"

typedef struct {
	pid_t oss_pid;									/* PID of master process */
	struct timespec clock;
	struct user users[USER_LIMIT];

	sem_t mutex;

	struct resource R[R_SIZE];	/* resource descriptors */
} OSS;

struct msgbuf {
	long mtype;
	int val;
};

OSS * oss_init(const int flags);
int oss_deinit(const int clear);

int msg_snd(const struct msgbuf *mb);
int msg_rcv(struct msgbuf *mb);
int msg_rcv_nb(struct msgbuf *mb);	/* non-blocking */

#endif
