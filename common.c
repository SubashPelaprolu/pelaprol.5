#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <unistd.h>
#include <fcntl.h>
#include "common.h"

#define MTEXT_SIZE sizeof(int)

static OSS * ossaddr = NULL;
static int msgid = -1;
static int shmid = -1;

OSS * oss_init(const int is_oss){

	 srand(getpid());

	const int shmflg = (is_oss) ? S_IRUSR | S_IWUSR | IPC_CREAT : 0;
 	const int key = ftok("makefile", 1234);

 	shmid = shmget(key, sizeof(OSS), shmflg);
 	if (shmid == -1) {
 		perror("shmget");
 		return NULL;
 	}

 	ossaddr = (OSS*) shmat(shmid, (void *)0, 0);
 	if (ossaddr == (void *)-1) {
 		perror("shmat");
 		shmctl(shmid, IPC_RMID, NULL);
 		return NULL;
 	}

	const int key2 = ftok("makefile", 1234);
	msgid = msgget(key2, shmflg);
	if(msgid == -1){
		perror("msgget");
		return NULL;
	}

	if(is_oss){ /* if we create the ossaddrory */
		memset(ossaddr, 0, sizeof(OSS));

		if(sem_init(&ossaddr->mutex, 1, 1) == -1){
			perror("sem_init");
			return NULL;
		}
  }

	return ossaddr;
}

int oss_deinit(const int clear){

  if(ossaddr == NULL){
    fprintf(stderr, "Error: shm is already cleared\n");
    return -1;
  }

  if(shmdt(ossaddr) == -1){
    perror("shmdt");
  }

  if(clear){
	  if(shmctl(shmid, IPC_RMID,NULL) == -1){
      perror("shmctl");
    }
		msgctl(msgid, IPC_RMID, NULL);
  }

  ossaddr = NULL;
  shmid = -1;

  return 0;
}

int msg_snd(const struct msgbuf *mb){

  if(msgsnd(msgid, (void*)mb, MTEXT_SIZE, 0) == -1){
    perror("msgsnd");
    return -1;
  }

  return 0;
}

int msg_rcv(struct msgbuf *mb){

  if(msgrcv(msgid, (void*)mb, MTEXT_SIZE, getpid(), 0) == -1){
    perror("msgrcv");
    return -1;
  }

  return 0;
}

int msg_rcv_nb(struct msgbuf *mb){

  if(msgrcv(msgid, (void*)mb, MTEXT_SIZE, getpid(), IPC_NOWAIT) == -1){
		if(errno != ENOMSG){
    	perror("msgrcv");
		}
    return -1;
  }

  return 0;
}
