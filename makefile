CC=gcc
CFLAGS=-Wall -g

sim: oss user

oss: oss.c timer.o bb.o common.o resource.o
	$(CC) $(CFLAGS) oss.c timer.o bb.o resource.o common.o -o oss -lrt -pthread

user: user.o timer.o common.o resource.o
	$(CC) $(CFLAGS) user.o timer.o resource.o common.o -o user -lrt -pthread

user.o: user.c user.h
	$(CC) $(CFLAGS) -c user.c

bb.o: bb.c bb.h
	$(CC) $(CFLAGS) -c bb.c

timer.o: timer.h timer.c
	$(CC) $(CFLAGS) -c timer.c

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

resource.o: resource.c resource.h
	$(CC) $(CFLAGS) -c resource.c

clean:
	rm -f ./oss ./user *.log *.o output.txt
