#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include "common.h"

static int random_total(const struct resource R[R_SIZE]){
	int i, l=0, request[R_SIZE];
	for(i=0; i < R_SIZE; i++){
		if(R[i].total > 0){
			request[l++] = i;
		}
	}

	if(l == 0){
		return R_SIZE;
	}
	int r = rand() % l;

	return request[r];
}

static int random_available(const struct resource R[R_SIZE]){
	int i, l=0, request[R_SIZE];
	for(i=0; i < R_SIZE; i++){
		if(R[i].available > 0){
			request[l++] = i;
		}
	}

	if(l == 0){
		return R_SIZE;
	}
	int r = rand() % l;

	return request[r];
}

int main(const int argc, char * argv[]){

	const int ui = atoi(argv[1]);

	OSS *ossaddr = oss_init(0);
	if(ossaddr == NULL){
		return -1;
	}
	struct user * usr = &ossaddr->users[ui];

	srand(getpid());
	bzero(usr->R, sizeof(struct resource)*R_SIZE);

	//generate our total request of resources
	int i;
	for(i=0; i < R_SIZE; i++){
		usr->R[i].total = 1 + (rand() % ossaddr->R[i].total);
	}

	sem_wait(&ossaddr->mutex);
	const unsigned int life = ossaddr->clock.tv_sec + USER_LIFE;
	sem_post(&ossaddr->mutex);

	int term = 0;
	int nreq = 0;

	struct msgbuf msg;

	while(term == 0){

		//check if we should terminated
		sem_wait(&ossaddr->mutex);
		const int state = (life > ossaddr->clock.tv_sec) ? usr->state : USER_TERMINATE;
		sem_post(&ossaddr->mutex);

		if(state == USER_TERMINATE)
			break;

		const int prob = (nreq > 0) ? 100 : B_MAX;

		if(	(rand() % prob) < B_MAX){	// in range [0;B] process is requesting resource
			usr->request.id = random_total(usr->R);
			if(usr->request.id == R_SIZE){
				break;
			}

			usr->request.val = 1 + (rand() % usr->R[usr->request.id].total);
			nreq++;

		}else{
			usr->request.id = random_available(usr->R);
			if(usr->request.id == R_SIZE){
				continue;
			}


			usr->request.val = -1* (1 + (rand() % usr->R[usr->request.id].available));
			nreq--;
		}
		usr->request.state	= WAITING;


		msg.mtype = getppid();
		msg.val = ui;
		if(	(msg_snd(&msg) == -1) ||	/* tell OSS we have a request */
				(msg_rcv(&msg) == -1)){		/* wait for reply */
			/* in case or error */
			break;	/* stop */
		}

		/* check what happened with our request */
		switch(usr->request.state){
			case ACCEPTED:
				if(usr->request.val > 0){	/*if it was a request */
					usr->R[usr->request.id].total -= usr->request.val;
				}
				break;
			case DENIED:	//we get here only when a deadlock has occured
			case BLOCKED:
				term = -1;
				break;

			default:
				fprintf(stderr, "CHILD: Invalid request state %d\n", usr->request.state);
				term = -1;
				break;
		}
		bzero(&usr->request, sizeof(struct request));

  }

	sem_wait(&ossaddr->mutex);
	usr->state = USER_EXITED;
	sem_post(&ossaddr->mutex);

	oss_deinit(0);
  return 0;
}
