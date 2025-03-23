#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "timing.h"

/* Thread argument structure to pass all necessary data to each thread */
typedef struct {
    int thread_id;       // ID of the thread
    int n;               // Size of the grid
    int nsweeps;         // Number of sweeps
    double* u;           // Solution array
    double* utmp;        // Temporary array
    double* f;           // Right-hand side
    double h2;           // Square of the grid spacing
    int start;           // Start index for this thread
    int end;             // End index for this thread
    pthread_barrier_t* barrier; // Synchronization barrier
} thread_data_t;

/* Thread function for the Jacobi iteration */
void* jacobi_worker(void* arg) {
    thread_data_t* data = (thread_data_t*)arg;
    int i, sweep;
    
    // Extract data from the thread structure
    int start = data->start;
    int end = data->end;
    double* u = data->u;
    double* utmp = data->utmp;
    double* f = data->f;
    double h2 = data->h2;
    int nsweeps = data->nsweeps;
    pthread_barrier_t* barrier = data->barrier;
    
    for (sweep = 0; sweep < nsweeps; sweep += 2) {
        // First half-sweep: update utmp using values from u
        for (i = start; i < end; ++i) {
            utmp[i] = (u[i-1] + u[i+1] + h2*f[i])/2;
        }
        
        // Synchronize all threads before the second half-sweep
        pthread_barrier_wait(barrier);
        
        // Second half-sweep: update u using values from utmp
        for (i = start; i < end; ++i) {
            u[i] = (utmp[i-1] + utmp[i+1] + h2*f[i])/2;
        }
        
        // Synchronize all threads before the next sweep
        pthread_barrier_wait(barrier);
    }
    
    return NULL;
}

/* 
 * Multi-threaded Jacobi iteration method
 * num_threads specifies how many threads to use for the computation
 */
void jacobi_parallel(int nsweeps, int n, double* u, double* f, int num_threads) {
    int i;
    double h = 1.0 / n;
    double h2 = h*h;
    double* utmp = (double*) malloc((n+1) * sizeof(double));
    pthread_t* threads;
    thread_data_t* thread_data;
    pthread_barrier_t barrier;
    
    /* Initialize the temporary array with boundary conditions */
    utmp[0] = u[0];
    utmp[n] = u[n];
    
    /* Adjust the number of threads if we have too many compared to problem size */
    if (num_threads > n-1) {
        num_threads = n-1;
        printf("Reducing number of threads to %d based on problem size.\n", num_threads);
    }
    
    /* Allocate thread structures */
    threads = (pthread_t*) malloc(num_threads * sizeof(pthread_t));
    thread_data = (thread_data_t*) malloc(num_threads * sizeof(thread_data_t));
    
    /* Initialize the barrier for thread synchronization */
    pthread_barrier_init(&barrier, NULL, num_threads);
    
    /* Calculate the workload distribution */
    int points_per_thread = (n-1) / num_threads;
    int remainder = (n-1) % num_threads;
    
    /* Create and launch the threads */
    int start = 1; // Skip the first boundary point
    
    for (i = 0; i < num_threads; i++) {
        // Calculate the range for this thread
        thread_data[i].thread_id = i;
        thread_data[i].n = n;
        thread_data[i].nsweeps = nsweeps;
        thread_data[i].u = u;
        thread_data[i].utmp = utmp;
        thread_data[i].f = f;
        thread_data[i].h2 = h2;
        thread_data[i].start = start;
        
        // Calculate how many points this thread will process
        int points = points_per_thread;
        if (i < remainder) {
            points++; // Distribute the remainder among the first 'remainder' threads
        }
        
        thread_data[i].end = start + points;
        start = thread_data[i].end;
        
        // Note that we need to ensure that end <= n
        if (thread_data[i].end > n) {
            thread_data[i].end = n;
        }
        
        thread_data[i].barrier = &barrier;
        
        // Create the thread
        pthread_create(&threads[i], NULL, jacobi_worker, &thread_data[i]);
    }
    
    /* Wait for all threads to complete */
    for (i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    /* Clean up */
    pthread_barrier_destroy(&barrier);
    free(threads);
    free(thread_data);
    free(utmp);
}

/* Original sequential implementation kept for reference */
void jacobi_sequential(int nsweeps, int n, double* u, double* f) {
    int i, sweep;
    double h = 1.0 / n;
    double h2 = h*h;
    double* utmp = (double*) malloc((n+1) * sizeof(double));

    /* Fill boundary conditions into utmp */
    utmp[0] = u[0];
    utmp[n] = u[n];

    for (sweep = 0; sweep < nsweeps; sweep += 2) {
        /* Old data in u; new data in utmp */
        for (i = 1; i < n; ++i)
            utmp[i] = (u[i-1] + u[i+1] + h2*f[i])/2;
        
        /* Old data in utmp; new data in u */
        for (i = 1; i < n; ++i)
            u[i] = (utmp[i-1] + utmp[i+1] + h2*f[i])/2;
    }

    free(utmp);
}

/* The main function - now modified to accept number of threads as a parameter */
void jacobi(int nsweeps, int n, double* u, double* f) {
    // Default to 4 threads if not specified in the environment
    int num_threads = 4;
    
    // Check for environment variable to override the default
    char* threads_env = getenv("JACOBI_NUM_THREADS");
    if (threads_env != NULL) {
        num_threads = atoi(threads_env);
        if (num_threads <= 0) {
            num_threads = 4; // Reset to default if invalid value
        }
    }
    
    // Use parallel version
    jacobi_parallel(nsweeps, n, u, f, num_threads);
}

void write_solution(int n, double* u, const char* fname) {
    int i;
    double h = 1.0 / n;
    FILE* fp = fopen(fname, "w+");
    for (i = 0; i <= n; ++i)
        fprintf(fp, "%g %g\n", i*h, u[i]);
    fclose(fp);
}

int main(int argc, char** argv) {
    int i;
    int n, nsteps;
    double* u;
    double* f;
    double h;
    timing_t tstart, tend;
    char* fname;
    int num_threads = 4; // Default number of threads

    /* Process arguments */
    n      = (argc > 1) ? atoi(argv[1]) : 100;
    nsteps = (argc > 2) ? atoi(argv[2]) : 100;
    fname  = (argc > 3) ? argv[3] : NULL;
    
    // Optional 4th argument for number of threads
    if (argc > 4) {
        num_threads = atoi(argv[4]);
        // Set environment variable for jacobi function to use
        char thread_env[32];
        snprintf(thread_env, sizeof(thread_env), "%d", num_threads);
        setenv("JACOBI_NUM_THREADS", thread_env, 1);
    }
    
    h = 1.0/n;

    /* Allocate and initialize arrays */
    u = (double*) malloc((n+1) * sizeof(double));
    f = (double*) malloc((n+1) * sizeof(double));
    memset(u, 0, (n+1) * sizeof(double));
    for (i = 0; i <= n; ++i)
        f[i] = i * h;

    /* Run the solver */
    get_time(&tstart);
    jacobi(nsteps, n, u, f);
    get_time(&tend);

    /* Print results */    
    printf("n: %d\n"
           "nsteps: %d\n"
           "threads: %d\n"
           "Elapsed time: %g s\n", 
           n, nsteps, num_threads, timespec_diff(tstart, tend));

    /* Write the results */
    if (fname)
        write_solution(n, u, fname);

    free(f);
    free(u);
    return 0;
}