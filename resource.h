
//variable B - 70/30 request/release chance
#define B_MAX 70

//number and max value of resource descriptors
#define R_SIZE	20

enum request_state { ACCEPTED=0, BLOCKED, WAITING, DENIED};

struct request{
	int id;
	int val;
	int state;
};

struct resource{
	int total;
	int available;
	int shared;
};

int generate_resources(struct resource R[R_SIZE]);
