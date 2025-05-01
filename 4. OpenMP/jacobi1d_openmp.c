#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <omp.h>
#include "timing.h"

int main(int argc, char** argv) {
    int n     = (argc > 1) ? atoi(argv[1]) : 100;
    int nsteps= (argc > 2) ? atoi(argv[2]) : 100;
    int num_threads = (argc > 3) ? atoi(argv[3]) : omp_get_max_threads();
    const char* fname = (argc > 4) ? argv[4] : NULL;

    omp_set_num_threads(num_threads);

    double h  = 1.0 / n;
    double h2 = h * h;

    // Reservar y inicializar
    double *u    = malloc((n+1)*sizeof(double));
    double *utmp = malloc((n+1)*sizeof(double));
    double *f    = malloc((n+1)*sizeof(double));
    if(!u||!utmp||!f) { fprintf(stderr, "Error en malloc\n"); return EXIT_FAILURE; }

    memset(u, 0, (n+1)*sizeof(double));
    for(int i=0;i<=n;i++) {
        f[i] = i*h;
        utmp[i] = 0.0;
    }
    utmp[0] = u[0];
    utmp[n] = u[n];

    timing_t tstart, tend;
    get_time(&tstart);

    // Iteraciones Jacobi
    for(int step=0; step < nsteps; step++) {
        // primer sweep
        #pragma omp parallel for schedule(static)
        for(int i=1; i<n; i++)
            utmp[i] = (u[i-1] + u[i+1] + h2*f[i]) * 0.5;
        // segundo sweep
        #pragma omp parallel for schedule(static)
        for(int i=1; i<n; i++)
            u[i] = (utmp[i-1] + utmp[i+1] + h2*f[i]) * 0.5;
    }

    get_time(&tend);

    printf("n: %d\nnsteps: %d\nnum_threads: %d\nElapsed time: %Lf s\n",
           n, nsteps, num_threads, timespec_diff(tstart, tend));

    if(fname) {
        FILE* fp = fopen(fname, "w");
        for(int i=0;i<=n;i++) fprintf(fp, "%g %g\n", i*h, u[i]);
        fclose(fp);
    }

    free(u); free(utmp); free(f);
    return 0;
}