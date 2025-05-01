CC = gcc
CFLAGS = -O3 -march=native -funroll-loops -fopenmp

all: jacobi1d_openmp

jacobi1d_openmp: jacobi1d_openmp.c timing.c timing.h
	$(CC) $(CFLAGS) jacobi1d_openmp.c timing.c -o jacobi1d_openmp

clean:
	rm -f jacobi1d_openmp resultados_benchmark_omp.csv