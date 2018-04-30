#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <omp.h>

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
	fflush(stdout);
}

void evolve(unsigned char ** univ, int w, int h) {
  if (NULL == new) {
    new = empty_univ(w, h);
  }

  // Nested for converted to expanded for in order to support nested loop
  #pragma omp parallel for shared(univ, new)
  for (int xy = 0; xy < w * h; ++xy) {
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

  #pragma omp parallel for shared(univ, new)
  for (int xy = 0; xy < w * h; ++xy) {
    int x = xy / h;
    int y = xy % h;
    univ[y][x] = new[y][x];
  }
}

void game(int w, int h, unsigned char** univ, int cycles, int print_result, int display_timer) {
  int c = 0;
  double starttime, stoptime;

  starttime = omp_get_wtime();
  while (c < cycles) {
    if (display_timer > 0) {
      show(univ, w, h);
      usleep(display_timer);
    }
    evolve(univ, w, h);
    c++;
  }
  stoptime = omp_get_wtime();

  // Should we print the universe when simulation is over?
  if (1 == print_result) {
    show(univ, w, h);
  }
  printf("Simulation completed after %d cycles using %d threads. Environment size: width=%d, height=%d. Execution time: %4.5f seconds.\n\n", cycles, omp_get_max_threads(), w, h, stoptime-starttime);
}

unsigned char** random_univ(int w, int h) {
  unsigned char** univ = empty_univ(w, h);

  int seed = time(NULL);
  srand(seed);

  #pragma omp parallel for
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
  omp_set_num_threads(num_threads);

  game(w, h, univ, cycles, print_result, display_timer);

  return 0;
}
