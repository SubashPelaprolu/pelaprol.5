To compile use
make command
after compilation we will get 
gcc -Wall -g -c bb.c
gcc -Wall -g -c common.c
gcc -Wall -g -c resource.c
gcc -Wall -g oss.c timer.o bb.o resource.o common.o -o oss -lrt -pthread
gcc -Wall -g -c user.c
gcc -Wall -g user.o timer.o resource.o common.o -o user -lrt -pthread
 
 To Execute:
 ./oss command
output will be in output.txt file

Shared resources have value > 1.
On a deadlock, we kill first user in the deadlock.

For the version control 
git log: /classes/OS/pelaprol/pelaprol.1/log
