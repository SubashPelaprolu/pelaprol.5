#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
//#include <errno.h>
#include <sys/ipc.h>
#include <sys/wait.h>

#include "common.h"
#include "user.h"
#include "bb.h"

static 			 unsigned int num_users = 0;
static const unsigned int max_users = 100;
static 			 unsigned int num_exited = 0;

static unsigned int num_rel = 0, num_req = 0;
static unsigned int num_accept=0, num_block = 0, num_deny = 0;
static unsigned int num_deadlock = 0, num_kills = 0;
static unsigned int num_lines = 0;

static int arg_verbose = 0;

static OSS *ossaddr = NULL;
static const char * logfile = "output.txt";

static int sig_flag = 0;	//raise, if we were signalled
static unsigned int user_bitmap = 0;

//Queues for processes with requests
static struct bounded_buffer req_bb; // request queue

//check which user entry is free, using bitmap
static int get_free_ui(){
	int i;
  for(i=0; i < USER_LIMIT; i++){
  	if(((user_bitmap & (1 << i)) >> i) == 0){	//if bit is 0, than means entry is free
			user_bitmap ^= (1 << i);	//set bit to 1, to mark it used
      return i;
    }
  }
  return -1;
}

//Clear a user entry
void ui_clear(struct user * users, const unsigned int i){
  user_bitmap ^= (1 << i); //set bit to 0, to mark it free
  memset(&users[i], 0, sizeof(struct user));
}

struct user * ui_new(struct user * users, const int ID){
	const int i = get_free_ui();
	if(i == -1){
		return NULL;
	}

  users[i].ID	= ID;
  users[i].state = USER_READY;
	return &users[i];
}

static int user_fork(void){

  struct user *usr;
	char arg[100];


  if((usr = ui_new(ossaddr->users, num_users)) == NULL)
    return 0;

	const int ui = usr - ossaddr->users; //process index
	snprintf(arg, sizeof(arg), "%u", ui);

	const pid_t pid = fork();
	switch(pid){
		case -1:
			ui_clear(ossaddr->users, ui);
			perror("fork");
			break;

		case 0:

			execl("./user", "./user", arg, NULL);
			perror("execl");
			exit(EXIT_FAILURE);

		default:
			num_users++;
			usr->pid = pid;
			break;
  }

  printf("[%li.%li] OSS: Generating process with PID %u\n", ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec, usr->ID);
	return 0;
}

static int fork_ready(struct timespec *forkat){

  if(num_users < max_users){  //if we can fork more

    // if its time to fork
    if(timercmp(&ossaddr->clock, forkat)){

      //next fork time
      forkat->tv_sec = ossaddr->clock.tv_sec + 1;
      forkat->tv_nsec = 0;

      return 1;
    }
  }
  return 0; //not time to fokk
}

//Stop users. Used only in case of an alarm.
static void stop_users(){
  int i;
  struct msgbuf mb;
	memset(&mb, 0, sizeof(mb));

	mb.val = DENIED;

  for(i=0; i < USER_LIMIT; i++){
    if(ossaddr->users[i].pid > 0){
      mb.mtype = ossaddr->users[i].pid;
      msg_snd(&mb);
    }
  }
}

static void signal_handler(int sig){
  printf("[%li.%li] OSS: Signal %d\n", ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec, sig);
  sig_flag = sig;
}

static int current_requests(){

  printf("[%li.%li] Current resource requests\n", ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec);

  int i, count=0;
  for(i=0; i < USER_LIMIT; i++){
    struct user * usr = &ossaddr->users[i];
    if((usr->pid > 0) && (usr->request.val!= 0)){
      printf("P%d: R%d=%d\n", usr->ID, usr->request.id, usr->request.val);
			count++;
    }
  }

	num_lines += count;
	return count;
}

static void show_available(){

  printf("[%li.%li] Currently available resources\n", ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec);
	num_lines++;

	printf("    ");
	int i;
	for(i=0; i < R_SIZE; i++){
		printf("R%02d ", i);
  }
	printf("\n");
	num_lines++;


	//OSS total resources
	printf("TOT ");
  for(i=0; i < R_SIZE; i++){
    printf("%*d ", 3, ossaddr->R[i].total);
  }
	printf("\n");
	num_lines++;

	//OSS avaialble
  printf("OSS ");
  for(i=0; i < R_SIZE; i++){
    printf("%3d ", ossaddr->R[i].available);
  }
  printf("\n");
	num_lines++;


	//show what the users have
	for(i=0; i < USER_LIMIT; i++){
    struct user * usr = &ossaddr->users[i];
		if(usr->pid > 0){
  		printf("P%02d ", usr->ID);

			int j;
  		for(j=0; j < R_SIZE; j++){
  			printf("%3d ", usr->R[j].available);
			}

  		printf("\n");
			num_lines++;
    }
	}
}

static int all_finished(const int uis[USER_LIMIT], const int finished[USER_LIMIT], const int n){

  int i, dead_one = -1;
	for(i=0; i < n; i++){
		if(!finished[i]){
      if(dead_one == -1){
        dead_one = uis[i];     //index of first dead process
				break;
      }
    }
  }

  if(arg_verbose && (dead_one > 0)){
    printf("Users ");
    for(i=0; i < n; i++){
  		if(!finished[i]){	//if not finished
        printf("P%d ", ossaddr->users[i].ID);
      }
    }
    printf("in deadlock.\n");
  }

	return dead_one;
}

static int deadlock_check(void){

	int i,j, avail[R_SIZE], finished[USER_LIMIT], uis[USER_LIMIT];

	/* nobody finished */
	for(i=0; i < USER_LIMIT; i++)
		finished[i] = 0;

	/* available resources */
	for(j=0; j < R_SIZE; j++)
		avail[j] = ossaddr->R[j].available;

	/* make a list of users with requests */
	const int nreq = bb_data(&req_bb, uis);

  i=0;
	while(i != nreq){

		for(i=0; i < nreq; i++){

			if(finished[i] == 1)
				continue;

			const int ui = uis[i];
			struct user * usr = &ossaddr->users[ui];

      if(	(usr->request.val < 0 ) ||
					(usr->request.val <= avail[usr->request.id])){

				finished[i] = 1;

				for(j=0; j < R_SIZE; j++){
					avail[j] += usr->R[j].available;
				}

				break;
			}
		}
	}

	/* return index of first user in deadlock, if any */
	return all_finished(uis, finished, nreq);
}

static void deny_deadlocked_user(const int ui){
	int i;
	struct msgbuf msg;
	struct user * usr = &ossaddr->users[ui];

	if(arg_verbose){
    printf("Killing P%i deadlocked for R%d:%d\n", usr->ID, usr->request.id, usr->request.val);
		num_lines++;

    printf("Resources released are as follows:");

    for(i=0; i < R_SIZE; i++){
      if(usr->R[i].available > 0){
  		    printf("R%i:%d ", i, usr->R[i].available);
      }
    }
  	printf("\n");
		num_lines++;

    current_requests();
    show_available();
  }

	num_kills++;
  usr->state = USER_TERMINATE;

	usr->request.state = DENIED;
	num_deny++;

	/* return its resources to OSS */
	for(i=0; i < R_SIZE; i++){
		ossaddr->R[i].available += usr->R[i].available;
		usr->R[i].available = 0;
	}

	/* remove user from queue */
	bb_remove(&req_bb, ui);

	/* tell him request is denied */
	msg.mtype = usr->pid;
	msg.val = DENIED;
	msg_snd(&msg);
}

static void resolve_deadlock(){
  if(arg_verbose){
    num_lines++;
    printf("[%li.%li] Master running deadlock detection\n", ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec);
  }

  int dead_one = -1, old_kills=num_kills;
  while((dead_one = deadlock_check()) >= 0){

		if(arg_verbose){
			printf("Attempting to resolve deadlock...\n");
			num_lines++;
		}

		deny_deadlocked_user(dead_one);
  }

  if(old_kills != num_kills){	/* if we had a deadlock */
		num_deadlock++;
		if(arg_verbose){
    	printf("System is no longer in deadlock\n");
			num_lines++;
		}
	}
}

static int req_process(struct user * usr){

	enum request_state old_state = usr->request.state;

	struct request * request = &usr->request;
	if(request->val < 0){	//release
			num_rel++;
			    usr->R[request->id].available  -= -1*request->val;
			ossaddr->R[request->id].available  += -1*request->val;
			usr->request.state = ACCEPTED;

			printf("[%li.%li] Master has acknowledged Process P%d releasing R%d=%d\n",
        ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec, usr->ID, usr->request.id, usr->request.val);

	}else if(request->val > 0){	//request


		if(ossaddr->R[request->id].available >= request->val) {
			usr->R[request->id].available 		+= request->val;
			ossaddr->R[request->id].available -= request->val;

			usr->request.state = ACCEPTED;
			num_accept++;

			if(old_state == BLOCKED){
		    printf("[%li.%li] Master unblocking P%d and granting it R%d=%d\n",
		      ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec, usr->ID, usr->request.id, usr->request.val);

		  }else if(old_state == WAITING){
				num_req++;
		    if(usr->request.val> 0){
		      printf("[%li.%li] Master granting P%d request R%d=%d\n",
		        ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec, usr->ID, usr->request.id, usr->request.val);

		    }
		  }

		}else if(usr->request.state != BLOCKED){
			usr->request.state = BLOCKED;
			num_block++;
		  printf("[%li.%li] Master blocking P%d for requesting R%d=%d\n",
		  	ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec, usr->ID, usr->request.id, usr->request.val);
		}

	}else{
		usr->request.state = DENIED;
		num_deny++;
		printf("[%li.%li] Master denied P%d invalid request R%d=%d\n",
	        ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec, usr->ID, usr->request.id, usr->request.val);
	}
	return usr->request.state;
}

static int req_dispatch(){
	int i, count=0;
  struct timespec tdisp;
	struct msgbuf msg;

  tdisp.tv_sec = 0;

	const int nreq = bb_size(&req_bb);

	for(i=0; i < nreq; i++){

		const int ui = bb_pop(&req_bb);	/* get index of process who has a request */
		struct user * usr = &ossaddr->users[ui];

		if(usr->pid > 0){	//process is running

			if((usr->request.state != BLOCKED) && (usr->request.val> 0)){
				if(arg_verbose){
					num_lines++;
							printf("[%li.%li] Master has detected P%d requesting R%d=%d\n",
						ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec, usr->ID, usr->request.id, usr->request.val);
				}
			}

			msg.val = req_process(usr);

			if(msg.val != BLOCKED){	//if request was accepted or denied
				count++;

				//send message to user to unblock
				msg.mtype = usr->pid;
				if(msg_snd(&msg) == -1){
					break;
				}
			}else{
				/* push the blocked request at end of queue */
				bb_push(&req_bb, ui);
			}

    	if((arg_verbose == 1) && ((num_req % 20) == 0))
    		show_available();
		}

    //add request processing to clock
    tdisp.tv_nsec = rand() % 100;
    timeradd(&ossaddr->clock, &tdisp);
	}

	return count;	//return number of dispatched procs
}

static int req_gather(){
	int n = 0;	/* new request */
	struct msgbuf msg;

	usleep(20);	/* give some time of users to enqueue more requests */

	while(1){
		if(msg_rcv_nb(&msg) < 0){
			if(errno == ENOMSG){
				break;	//stop
			}else{
				return -1;	//return error
			}
		}

		if(bb_push(&req_bb, msg.val) < 0){	/* if queue is full */
			break;	//stop
		}
		n++;
	}

	if(arg_verbose && (n > 0)){
		printf("[%li.%li] Master added %d new request. Queue has %d\n", ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec, n, bb_size(&req_bb));
	}
	return n;
}

static void check_exited(){
	int i;

	for(i=0; i < USER_LIMIT; i++){

		struct user * usr = &ossaddr->users[i];

		if(usr->state == USER_EXITED){
			num_exited++;
			printf("[%li.%li] Master has detected P%d exited\n",
				ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec, usr->ID);

			ui_clear(ossaddr->users, i);
		}
	}
}

int main(const int argc, char * argv[]){

	if((argc == 2) && (strcmp(argv[1], "-v") == 0)){
		arg_verbose = 1;
	}

  signal(SIGINT,  signal_handler);
  signal(SIGTERM, signal_handler);
  signal(SIGALRM, signal_handler);
  signal(SIGCHLD, SIG_IGN);

	stdout = freopen(logfile, "w", stdout);
  if(stdout == NULL){
		perror("freopen");
		return -1;
	}

  ossaddr = oss_init(1);
  if(ossaddr == NULL){
    return -1;
  }


  bb_init(&req_bb);
	generate_resources(ossaddr->R);
	show_available();

	//alarm(3);

	struct timespec inc;		//clock increment
  struct timespec forkat;	//when to fork another process

	const unsigned int ns_step = 10000;

	inc.tv_sec = 1;
	timerzero(&forkat);

  while(!sig_flag){

		if(num_exited >= max_users)
			break;

	  //increment system clock
	  inc.tv_nsec = rand() % ns_step;
	  timeradd(&ossaddr->clock, &inc);

		//if we are ready to fork, start a process
    if(fork_ready(&forkat) && (user_fork() < 0))
      break;

		//resource logic
		const int new_req = req_gather();
    const int new_disp = req_dispatch();

		//if we have no new requests and didn't dispatch anybody
    if((new_req + new_disp) == 0){
			/* maybe we are in a deadlock */
    	resolve_deadlock();
    }

  	if(num_lines >= 100000){
  		printf("[%li.%li] OSS: Closing output, due to line limit ...\n", ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec);
  		stdout = freopen("/dev/null", "w", stdout);
  	}

		check_exited();
  }

  printf("Runtime: %li:%li\n", ossaddr->clock.tv_sec, ossaddr->clock.tv_nsec);
	printf("Users: %d/%d\n", num_users, num_exited);
	printf("Requested: %u\n", num_req);
  printf("Released: %u\n", num_rel);
  printf("Accepted: %u\n", num_accept);
  printf("Blocked: %u\n", num_block);
  printf("Denied: %u\n", num_deny);
  printf("Deadlocks: %u\n", num_deadlock);
  printf("Kills: %u\n", num_kills);

  stop_users(-1);
  oss_deinit(1);
  return 0;
}
