#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "mpi.h"

unsigned char** new = NULL;

unsigned char** empty_univ(int w, int h) {
  unsigned char** univ = (unsigned char **) malloc(h * sizeof(unsigned char*));
  for(int i = 0; i < h; i++) {
    univ[i] = (unsigned char *)malloc(w * sizeof(unsigned char));
    for (int j = 0; j < w; j++) {
      univ[i][j] = 0;
    }
  }

  return univ;
}

void show(unsigned char** univ, int w, int h) {
  system("clear");
	printf("\033[H");
	for (int y = 0; y < h; y++) {
	  for (int x = 0; x < w; x++) {
		  printf(univ[y][x] ? "\033[07m  \033[m" : "  ");
	  }
		printf("\033[E");
	}
	printf("W=%d, H=%d, H/2=%d, H/2+1=%d\n", w, h, h / 2, h / 2 + 1);
	fflush(stdout);
}

void evolve(unsigned char ** univ, int w, int h, int startLine, int endLine) {
  if (NULL == new) {
    new = empty_univ(w, h);
  }

  printf("start = %d, end = %d || %d\n", startLine * w, endLine * w, w * h);
  for (int xy = startLine * w; xy < endLine * w && xy < w * h; ++xy) {
    int x = xy / h;
    int y = xy % h;

    int n = 0;
    for (int y1 = y - 1; y1 <= y + 1; y1++) {
      for (int x1 = x - 1; x1 <= x + 1; x1++) {
        if (univ[(y1 + h) % h][(x1 + w) % w]) {
          n++;
        }
      }
    }

    if (univ[y][x]) {
      n--;
    }
    new[y][x] = (n == 3 || (n == 2 && univ[y][x]));
  }
}

// Send initial state to each node [MASTER]
//   startLine, endLine, total_width, total_height, cycle_count, partition (part of the universe the slave is interested in)
// Allocate (endLine - startLine + 2) * width [SLAVE]
// Copy received partition to local array starting at line 1 [SLAVE]
// while (c < cicleCount) [MASTER]
//   Send next signal [MASTER]
//   [SLAVE]
//     Send current top line to left (if exists)
//     Send current down line to right (if exists)
//     Receives line from right, uses as last + 1
//     Receives line from left, uses as first - 1
//     Evolve
//     Notify master when done
//   Gets notification from each node, increment C [MASTER] 
//     (is it needed? If the slave got something from both right and left, it can just evolve it)
// Send collect signal to slaves [MASTER]
//   Each slave send its array line = 1 to localH and kill itself [SLAVE]

// MASTER
void game(int w, int h, unsigned char** univ, int cycles, int print_result, int display_timer) {
  int c = 0;
  double starttime, stoptime;

  printf("Universe:\n");
  for (int j = 0; j < w * h; j++) {
    printf("%d ", univ[j % h][j / h]);
    if ((j + 1) % w == 0) {
      printf("\n");
    }
  }

  int partitions = 4;//Partition count is NODE_COUNT - 1;
  // Temporary array
  //   temp[0] = total_width
  //   temp[1] = total_height
  //   temp[2] = firstLine
  //   temp[3] = lastLine
  for (int i = 0; i < partitions; i++) {
    int initialLine = (w * i) / partitions;
    int endLine = (w * (i + 1)) / partitions;
    if (i == partitions - 1 || endLine > h) {
      endLine = h;
    }

    printf("\nPartition[%d], Start=%d, End=%d:\n", i, initialLine, endLine);
    for (int j = initialLine * w; j < endLine * w; j++) {
      printf("%d ", univ[j % h][j / h]);
      if ((j + 1) % w == 0) {
        printf("\n");
      }
    }
  }

  while (c < cycles) {
    for (int xy = 0; xy < w * h; ++xy) {
      int x = xy / h;
      int y = xy % h;
      //univ[y][x] = new[y][x];
    }
    c++;
  }

  // Should we print the universe when simulation is over?
  if (1 == print_result) {
    show(univ, w, h);
  }
  printf("Simulation completed after %d cycles using %d threads. Environment size: width=%d, height=%d. Execution time: %4.5f seconds.\n\n", cycles, 1, w, h, 0); // TODO: review
}

unsigned char** random_univ(int w, int h) {
  unsigned char** univ = empty_univ(w, h);

  int seed = time(NULL);
  srand(seed);

  for (int xy = 0; xy < w * h; ++xy) {
    int x = xy / h;
    int y = xy % h;
    if (rand() < (RAND_MAX / 5)) {
      univ[y][x] = 1;
    } else {
      univ[y][x] = 0;
    }
  }
  return univ;
}

unsigned char** read_from_file(char* filename, int* w, int* h) {
  FILE* file = fopen(filename, "r");
  char line[1024];

  if (file == NULL) {
    return NULL;
  }

  if(fgets(line, sizeof(line), file)) {
    char *currnum;
    int numbers[2], i = 0;

    while ((currnum = strtok(i ? NULL : line, " ,")) != NULL && i < 2) {
      numbers[i++] = atoi(currnum);
    }

    *h = numbers[0];
    *w = numbers[1];
  }

  unsigned char** univ = empty_univ(*w, *h);

  for (int y = 0; y < *h; y++) {
    fgets(line, sizeof(line), file);
    for (int x = 0; x < *w; x++) {
      if (line[x] == '1') {
        univ[y][x] = 1;
      }
    }
  }

  fclose(file);

  return univ;
}

int main(int c, char **v) {
  if (c < 2) {
    printf("Invalid number of arguments.\n");
    return -1;
  }

  int w = 0, h = 0, cycles = 10, display_timer = 0, print_result = 0, num_threads = 0;
  unsigned char** univ;

  if (0 == strcmp(v[1], "random")) {
    printf("Using randomly generated pattern.\n");

    if (c > 2) w = atoi(v[2]);
    if (c > 3) h = atoi(v[3]);
    if (c > 4) cycles = atoi(v[4]);
    if (c > 5) num_threads = atoi(v[5]);
    if (c > 6) print_result = atoi(v[6]);
    if (c > 7) display_timer = atoi(v[7]);

    if (w <= 0) w = 30;
    if (h <= 0) h = 30;

    univ = random_univ(w, h);
  } else if (0 == strcmp(v[1], "file")) {
    if (c < 3) {
      printf("Missing file path.\n");
      return -1;
    }

    univ = read_from_file(v[2], &w, &h);

    if (NULL == univ) {
      printf("Invalid input file.\n");
      return -1;
    }

    if (c > 3) cycles = atoi(v[3]);
    if (c > 4) num_threads = atoi(v[4]);
    if (c > 5) print_result = atoi(v[5]);
    if (c > 6) display_timer = atoi(v[6]);
  } else {
    printf("Missing arguments.\n");
    return -1;
  }

  if (cycles <= 0) cycles = 10;
  if (display_timer < 0) display_timer = 0;

  if (print_result < 0) print_result = 0;
  else if (print_result > 1) print_result = 1;

  if (num_threads <= 0) num_threads = 1;

  game(w, h, univ, cycles, print_result, display_timer);

  return 0;
}
