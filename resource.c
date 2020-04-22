#include <stdlib.h>
#include <stdio.h>
#include "resource.h"

#define R_MAX 10

int generate_resources(struct resource R[R_SIZE]){

  /* 20% +- 5 are shared */
  int nshared = 20 + ((rand() % 10) - 5);

  nshared = R_SIZE / (100 / nshared);
  printf("Sharing %d resources\n", nshared);

  while(nshared > 0){
    const int i = rand() % R_SIZE;
    if(R[i].shared == 0){
      R[i].shared = 1;
      --nshared;
    }
  }

	int i;
	for(i=0; i < R_SIZE; i++){
    /* if its shared, we put a random value, otherwise 1 */
		R[i].total     =
		R[i].available = (R[i].shared)? 1 + (rand() % R_MAX) : 1;
  }

	return 0;
}
