#include <stdio.h>
#include <stdlib.h>
#include <time.h>



int main(int argc, char** argv) {

  int len = strtol(argv[1], NULL, 10);  
  srand(time(NULL));
  char chars[27] = {
    32, 65, 66, 67, 68, 69, 70, 71, 72, 73, \
    74, 75, 76, 77, 78, 79, 80, 81, 82, 83, \
    84, 85, 86, 87, 88, 89, 90
  };

// TODO: Maybe check for int argument here

  int i;
  for (i = 0; i < len; i++) {
    int index = rand() % 27;
    printf("%c", chars[index]);
  }

  printf("\n");
}
