#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#include "timing.h"

/* Shared memory structure for the arrays used in Jacobi method */
typedef struct {
    double* u;      // Solution array in shared memory
    double* utmp;   // Temporary array in shared memory
    double* f;      // Right-hand side array in shared memory
    int n;          // Size of the grid
} shared_data_t;

/* Thread argument structure to pass all necessary data to each thread */
typedef struct {
    int thread_id;       // ID of the thread
    int n;               // Size of the grid
    int nsweeps;         // Number of sweeps
    double* u;           // Solution array (shared memory)
    double* utmp;        // Temporary array (shared memory)
    double* f;           // Right-hand side (shared memory)
    double h2;           // Square of the grid spacing
    int start;           // Start index for this thread
    int end;             // End index for this thread
    pthread_barrier_t* barrier; // Synchronization barrier
} thread_data_t;

/* Create shared memory segment and map it to process address space */
void* create_shared_memory(const char* name, size_t size) {
    int fd;
    void* ptr;
    
    // Create shared memory object
    fd = shm_open(name, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }
    
    // Set the size of the shared memory object
    if (ftruncate(fd, size) == -1) {
        perror("ftruncate failed");
        exit(EXIT_FAILURE);
    }
    
    // Map the shared memory object into this process's address space
    ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        perror("mmap failed");
        exit(EXIT_FAILURE);
    }
    
    // Close the file descriptor (the mapping remains valid)
    close(fd);
    
    return ptr;
}

/* Unmap the shared memory from the process address space */
void destroy_shared_memory(const char* name, void* ptr, size_t size) {
    // Unmap the shared memory
    if (munmap(ptr, size) == -1) {
        perror("munmap failed");
        exit(EXIT_FAILURE);
    }
    
    // Remove the shared memory object
    if (shm_unlink(name) == -1) {
        perror("shm_unlink failed");
        // Continue execution even if unlink fails
    }
}

/* Initialize the shared memory for Jacobi method */
shared_data_t init_shared_memory(int n) {
    shared_data_t shared;
    size_t size = (n+1) * sizeof(double);
    
    // Create shared memory for u array
    shared.u = (double*)create_shared_memory("/jacobi_u", size);
    
    // Create shared memory for utmp array
    shared.utmp = (double*)create_shared_memory("/jacobi_utmp", size);
    
    // Create shared memory for f array
    shared.f = (double*)create_shared_memory("/jacobi_f", size);
    
    shared.n = n;
    
    return shared;
}

/* Clean up shared memory resources */
void cleanup_shared_memory(shared_data_t shared) {
    size_t size = (shared.n+1) * sizeof(double);
    
    destroy_shared_memory("/jacobi_u", shared.u, size);
    destroy_shared_memory("/jacobi_utmp", shared.utmp, size);
    destroy_shared_memory("/jacobi_f", shared.f, size);
}

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
 * Multi-threaded Jacobi iteration method with shared memory
 * num_threads specifies how many threads to use for the computation
 */
void jacobi_parallel_shared(int nsweeps, int n, double* u_orig, double* f_orig, int num_threads) {
    int i;
    double h = 1.0 / n;
    double h2 = h*h;
    pthread_t* threads;
    thread_data_t* thread_data;
    pthread_barrier_t barrier;
    
    // Initialize shared memory
    shared_data_t shared = init_shared_memory(n);
    
    // Copy input data to shared memory
    memcpy(shared.u, u_orig, (n+1) * sizeof(double));
    memcpy(shared.f, f_orig, (n+1) * sizeof(double));
    
    /* Initialize the temporary array with boundary conditions */
    shared.utmp[0] = shared.u[0];
    shared.utmp[n] = shared.u[n];
    
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
        thread_data[i].u = shared.u;
        thread_data[i].utmp = shared.utmp;
        thread_data[i].f = shared.f;
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
    
    // Copy results back from shared memory
    memcpy(u_orig, shared.u, (n+1) * sizeof(double));
    
    /* Clean up */
    pthread_barrier_destroy(&barrier);
    free(threads);
    free(thread_data);
    cleanup_shared_memory(shared);
}

/* The main function - now modified to use shared memory */
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
    
    // Check if we should use shared memory
    char* use_shared_env = getenv("JACOBI_USE_SHARED");
    int use_shared = (use_shared_env != NULL && atoi(use_shared_env) > 0);
    
    if (use_shared) {
        // Use parallel version with shared memory
        jacobi_parallel_shared(nsweeps, n, u, f, num_threads);
    } else {
        // Original implementation for when shared memory isn't needed
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
    int num_threads = 8; // Default number of threads
    int use_shared = 1;  // Default to not using shared memory

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
    
    // Optional 5th argument for using shared memory
    if (argc > 5) {
        use_shared = atoi(argv[5]);
        // Set environment variable for jacobi function to use
        char shared_env[32];
        snprintf(shared_env, sizeof(shared_env), "%d", use_shared);
        setenv("JACOBI_USE_SHARED", shared_env, 1);
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
           "shared memory: %s\n"
           "Elapsed time: %g s\n", 
           n, nsteps, num_threads, 
           use_shared ? "enabled" : "disabled",
           timespec_diff(tstart, tend));

    /* Write the results */
    if (fname)
        write_solution(n, u, fname);

    free(f);
    free(u);
    return 0;
}