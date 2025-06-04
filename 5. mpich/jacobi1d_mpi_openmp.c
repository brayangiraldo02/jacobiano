#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include <mpi.h>
#include "timing.h"

int main(int argc, char** argv) {
    int rank, size;
    
    // Inicializar MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    int n     = (argc > 1) ? atoi(argv[1]) : 100;
    int nsteps= (argc > 2) ? atoi(argv[2]) : 100;
    int num_threads = (argc > 3) ? atoi(argv[3]) : omp_get_max_threads();
    const char* fname = (argc > 4) ? argv[4] : NULL;

    omp_set_num_threads(num_threads);

    double h  = 1.0 / n;
    double h2 = h * h;

    // Calcular la distribución de trabajo por proceso MPI
    int local_n = n / size;
    int remainder = n % size;
    int local_start = rank * local_n + (rank < remainder ? rank : remainder);
    if (rank < remainder) local_n++;
    int local_end = local_start + local_n - 1;

    // Reservar memoria local (incluye puntos de frontera para comunicación)
    double *u_local    = malloc((local_n + 2) * sizeof(double));
    double *utmp_local = malloc((local_n + 2) * sizeof(double));
    double *f_local    = malloc((local_n + 2) * sizeof(double));
    
    if(!u_local || !utmp_local || !f_local) { 
        fprintf(stderr, "Error en malloc en proceso %d\n", rank); 
        MPI_Finalize();
        return EXIT_FAILURE; 
    }

    // Inicializar arrays locales
    memset(u_local, 0, (local_n + 2) * sizeof(double));
    for(int i = 1; i <= local_n; i++) {
        int global_i = local_start + i - 1;
        f_local[i] = global_i * h;
        utmp_local[i] = 0.0;
    }
    
    // Condiciones de frontera globales
    if (rank == 0) {
        u_local[1] = 0.0;  // u[0] = 0
        utmp_local[1] = 0.0;
    }
    if (rank == size - 1) {
        u_local[local_n] = 0.0;  // u[n] = 0
        utmp_local[local_n] = 0.0;
    }

    timing_t tstart, tend;
    if (rank == 0) get_time(&tstart);

    // Iteraciones Jacobi con MPI
    for(int step = 0; step < nsteps; step++) {
        // Intercambiar datos de frontera entre procesos vecinos
        MPI_Request requests[4];
        int req_count = 0;
        
        // Enviar/recibir con proceso anterior
        if (rank > 0) {
            MPI_Isend(&u_local[1], 1, MPI_DOUBLE, rank - 1, 0, MPI_COMM_WORLD, &requests[req_count++]);
            MPI_Irecv(&u_local[0], 1, MPI_DOUBLE, rank - 1, 1, MPI_COMM_WORLD, &requests[req_count++]);
        }
        
        // Enviar/recibir con proceso siguiente
        if (rank < size - 1) {
            MPI_Isend(&u_local[local_n], 1, MPI_DOUBLE, rank + 1, 1, MPI_COMM_WORLD, &requests[req_count++]);
            MPI_Irecv(&u_local[local_n + 1], 1, MPI_DOUBLE, rank + 1, 0, MPI_COMM_WORLD, &requests[req_count++]);
        }
        
        // Esperar comunicaciones
        MPI_Waitall(req_count, requests, MPI_STATUSES_IGNORE);
        
        // Primer sweep - OpenMP sobre puntos internos
        #pragma omp parallel for schedule(static)
        for(int i = 1; i <= local_n; i++) {
            int left = (i == 1 && rank == 0) ? u_local[1] : u_local[i-1];
            int right = (i == local_n && rank == size - 1) ? u_local[local_n] : u_local[i+1];
            utmp_local[i] = (left + right + h2 * f_local[i]) * 0.5;
        }
        
        // Segundo sweep
        #pragma omp parallel for schedule(static)
        for(int i = 1; i <= local_n; i++) {
            int left = (i == 1 && rank == 0) ? utmp_local[1] : utmp_local[i-1];
            int right = (i == local_n && rank == size - 1) ? utmp_local[local_n] : utmp_local[i+1];
            u_local[i] = (left + right + h2 * f_local[i]) * 0.5;
        }
    }

    if (rank == 0) {
        get_time(&tend);
        printf("n: %d\nnsteps: %d\nnum_processes: %d\nnum_threads_per_process: %d\nElapsed time: %Lf s\n",
               n, nsteps, size, num_threads, timespec_diff(tstart, tend));
    }

    // Recopilar resultados en el proceso 0 para escritura de archivo
    if(fname && rank == 0) {
        double *u_global = malloc((n + 1) * sizeof(double));
        
        // Copiar datos locales del proceso 0
        for(int i = 1; i <= local_n; i++) {
            u_global[local_start + i - 1] = u_local[i];
        }
        
        // Recibir datos de otros procesos
        for(int p = 1; p < size; p++) {
            int p_local_n = n / size;
            if (p < remainder) p_local_n++;
            int p_start = p * (n / size) + (p < remainder ? p : remainder);
            
            MPI_Recv(&u_global[p_start], p_local_n, MPI_DOUBLE, p, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
        
        // Escribir archivo
        FILE* fp = fopen(fname, "w");
        for(int i = 0; i <= n; i++) fprintf(fp, "%g %g\n", i*h, u_global[i]);
        fclose(fp);
        
        free(u_global);
    } else if(fname && rank != 0) {
        // Enviar datos locales al proceso 0
        MPI_Send(&u_local[1], local_n, MPI_DOUBLE, 0, 2, MPI_COMM_WORLD);
    }

    free(u_local); 
    free(utmp_local); 
    free(f_local);
    
    MPI_Finalize();
    return 0;
}