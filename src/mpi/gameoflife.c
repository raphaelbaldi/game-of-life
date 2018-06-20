#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include "mpi.h"

int tag = 42; /* Message tag */
int my_rank;  /* Process identifier */
int proc_n;   /* # of running processes */
unsigned char* new = NULL;

void show(unsigned char* univ, int w, int h) {
  system("clear");
	printf("\033[H");
	for (int y = 0; y < h; ++y) {
	  for (int x = 0; x < w; ++x) {
		  printf(univ[y * w + x] ? "\033[07m  \033[m" : "  ");
	  }
		printf("\033[E");
	}
	fflush(stdout);
}

unsigned char* empty_univ(int w, int h) {
  unsigned char* univ = (unsigned char*) malloc(w * h * sizeof(unsigned char));
  for(int xy = 0; xy < w * h; ++xy) {
    univ[xy] = 0;
  }
  return univ;
}

void evolve(unsigned char* univ, int w, int h) {
  if (NULL == new) {
    new = empty_univ(w, h);
  }

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      int n = 0;
      for (int y1 = y - 1; y1 <= y + 1; ++y1) {
        for (int x1 = x - 1; x1 <= x + 1; ++x1) {
          // Any cell outside the bounding box is considered to be dead.
          if (univ[((y1 + h) % h) * w + ((x1 + w) % w)]) {
            n++;
          }
        }
      }

      if (univ[y * w + x]) {
        n--;
      }
      new[y * w + x] = (n == 3 || (n == 2 && univ[y * w + x]));
    }
  }

  for (int xy = 0; xy < w * h; ++xy) {
    univ[xy] = new[xy];
  }
}

unsigned char* random_univ(int w, int h) {
  unsigned char* univ = empty_univ(w, h);

  int seed = time(NULL);
  srand(seed);
  for (int xy = 0; xy < w * h; ++xy) {
    if (rand() < (RAND_MAX / 5)) {
      univ[xy] = 1;
    } else {
      univ[xy] = 0;
    }
  }
  return univ;
}

unsigned char* read_from_file(char* filename, int* w, int* h) {
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

  unsigned char* univ = empty_univ(*w, *h);

  for (int y = 0; y < *h; ++y) {
    fgets(line, sizeof(line), file);
    for (int x = 0; x < *w; ++x) {
      if (line[x] == '1') {
        univ[y * *w + x] = 1;
      }
    }
  }

  fclose(file);

  return univ;
}

void run_master(int c, char** v) {
  int w = 0, h = 0, cycles = 10, display_timer = 0, print_result = 0;
  unsigned char* univ;

  if (0 == strcmp(v[1], "random")) {
    printf("Using randomly generated pattern.\n");

    if (c > 2) w = atoi(v[2]);
    if (c > 3) h = atoi(v[3]);
    if (c > 4) cycles = atoi(v[4]);
    if (c > 5) print_result = atoi(v[5]);
    if (c > 6) display_timer = atoi(v[6]);

    if (w <= 0) w = 30;
    if (h <= 0) h = 30;

    univ = random_univ(w, h);
  } else if (0 == strcmp(v[1], "file")) {
    if (c < 3) {
      printf("Missing file path.\n");
      return;
    }

    univ = read_from_file(v[2], &w, &h);

    if (NULL == univ) {
      printf("Invalid input file.\n");
      return;
    }

    if (c > 3) cycles = atoi(v[3]);
    if (c > 4) print_result = atoi(v[4]);
    if (c > 5) display_timer = atoi(v[5]);
  } else {
    printf("Missing arguments.\n");
    return;
  }

  if (cycles <= 0) cycles = 10;
  if (display_timer < 0) display_timer = 0;

  if (print_result < 0) print_result = 0;
  else if (print_result > 1) print_result = 1;

  // Partition the dataset and send to all nodes
  int temp[4];
  temp[0] = w;
  temp[1] = cycles;
  int partitions = proc_n - 1;
  int lines_per_partition = h / partitions;
  printf("Partitions=%d, LinesPerPartition=%d\n\n", partitions, lines_per_partition);
  int current_line = 0;
  for (int i = 1; i < proc_n; i++) {
    temp[2] = current_line;
    current_line += lines_per_partition;
    if (current_line > h || i == proc_n - 1) {
      current_line = h;
    }
    temp[3] = current_line;

    printf("Partition=%d, FirstLine=%d, LastLine=%d, Size=%d, ArrayEnd=%d, SendEnd=%d\n", i, temp[2], temp[3], (temp[3] - temp[2]) * w, h * w, temp[2] * w + ((temp[3] - temp[2]) * w));
    // FIRST MESSAGE
    //  temp[0] = width of each line
    //  temp[1] = cycles to run
    //  temp[2] = start line the slave will get
    //  temp[3] = last line the slave will get
    MPI_Send(temp, 4, MPI_INT, i, tag, MPI_COMM_WORLD);

    // SECOND MESSAGE
    //  partition
    MPI_Send(univ + temp[2] * w, (temp[3] - temp[2]) * w, MPI_CHAR, i, tag, MPI_COMM_WORLD);
  }

  MPI_Status status; /* MPI message status */

  // Receive data
  current_line = 0;
  for (int i = 1; i < proc_n; i++) {
    int initial_line = current_line;
    current_line += lines_per_partition;
    if (current_line > h || i == proc_n - 1) {
      current_line = h;
    }
    MPI_Recv(univ + initial_line * w, (current_line - initial_line) * w, MPI_CHAR, i, tag, MPI_COMM_WORLD, &status);
  }

  // Display the final result
  show(univ, w, h);

  printf("Simulation completed after %d cycles.\n\n", cycles);
}

void run_slave() {
  int universe_data[4];
  MPI_Status status; /* MPI message status */

  MPI_Recv(universe_data, 4, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
  // Alocate two additional lines
  int h = universe_data[3] - universe_data[2];
  int w = universe_data[0];
  int cycles = universe_data[1];
  unsigned char* local_univ = (unsigned char *)malloc ((h + 2) * w * sizeof(unsigned char));
  for (int i = 0; i < h * w; ++i) {
    local_univ[i] = 0;
  }

  // Start wrting at the second line
  MPI_Recv(local_univ + w, h * w, MPI_CHAR, 0, tag, MPI_COMM_WORLD, &status);

  int c = 0;
  while (c < cycles) {
    if (my_rank > 1) {
      // Send first line to up
      MPI_Send(local_univ + w, w, MPI_CHAR, my_rank - 1, tag, MPI_COMM_WORLD);
    }

    if (my_rank < proc_n - 1) {
      // Send last line to the down
      MPI_Send(local_univ + h * w, w, MPI_CHAR, my_rank + 1, tag, MPI_COMM_WORLD);
    }

    if (my_rank > 1) {
      // Receive from up
      MPI_Recv(local_univ, w, MPI_CHAR, my_rank - 1, tag, MPI_COMM_WORLD, &status);
    } else {
      // My first line should be empty
      for (int i = 0; i < w; ++i) {
        local_univ[i] = 0;
      }
    }

    if (my_rank < proc_n - 1) {
      // Receive from down
      MPI_Recv(local_univ + (h + 1) * w, w, MPI_CHAR, my_rank + 1, tag, MPI_COMM_WORLD, &status);
    } else {
      // My last line should be empty
      for (int i = (h + 1) * w; i < (h + 2) * w; ++i) {
        local_univ[i] = 0;
      }
    }

    evolve(local_univ, w, (h + 2));
    ++c;
  }

  // Send results to master
  MPI_Send(local_univ + w, h * w, MPI_CHAR, 0, tag, MPI_COMM_WORLD);
}

int main(int c, char** v) {
  if (c < 2) {
    printf("Invalid number of arguments.\n");
    return -1;
  }

  // Ensure we're not buffering data on stdout (print everything right away)
  setbuf(stdout, NULL);

  MPI_Init (&c, &v);

  MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &proc_n);

  if (my_rank == 0) {
    run_master(c, v);
  } else {
    run_slave();
  }

  MPI_Finalize();

  return 0;
}
