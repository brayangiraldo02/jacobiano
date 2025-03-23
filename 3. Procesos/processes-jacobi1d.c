#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <semaphore.h>
#include <sys/wait.h>
#include "timing.h"

/* Barrera reutilizable basada en dos turnstiles */
typedef struct {
    int count;
    int n; // número total de procesos
    sem_t mutex;
    sem_t turnstile1;
    sem_t turnstile2;
} barrier_t;

/* Inicializa la barrera en memoria compartida */
void barrier_init(barrier_t *b, int n) {
    b->n = n;
    b->count = 0;
    sem_init(&b->mutex, 1, 1);       // pshared = 1
    sem_init(&b->turnstile1, 1, 0);    // cerrado
    sem_init(&b->turnstile2, 1, 1);    // abierto
}

/* Espera en la barrera; algoritmo de dos fases */
void barrier_wait(barrier_t *b) {
    // Fase 1
    sem_wait(&b->mutex);
    b->count++;
    if(b->count == b->n) {
        sem_wait(&b->turnstile2);  // cerrar la segunda puerta
        sem_post(&b->turnstile1);  // abrir la primera puerta
    }
    sem_post(&b->mutex);

    sem_wait(&b->turnstile1);
    sem_post(&b->turnstile1);

    // Fase 2
    sem_wait(&b->mutex);
    b->count--;
    if(b->count == 0) {
        sem_wait(&b->turnstile1);  // cerrar la primera puerta
        sem_post(&b->turnstile2);  // abrir la segunda puerta
    }
    sem_post(&b->mutex);

    sem_wait(&b->turnstile2);
    sem_post(&b->turnstile2);
}

void barrier_destroy(barrier_t *b) {
    sem_destroy(&b->mutex);
    sem_destroy(&b->turnstile1);
    sem_destroy(&b->turnstile2);
}

void write_solution(int n, double* u, const char* fname) {
    int i;
    FILE* fp = fopen(fname, "w+");
    if(fp == NULL){
        fprintf(stderr, "Error al abrir el archivo %s\n", fname);
        return;
    }
    for (i = 0; i <= n; ++i)
        fprintf(fp, "%g %g\n", i * (1.0/n), u[i]);
    fclose(fp);
}

int main(int argc, char** argv) {
    int i;
    char* fname;
    timing_t tstart, tend;
    
    // Procesar argumentos:
    // Uso: ./jacobi_proc [n] [nsteps] [num_procs] [fname-opcional]
    int n = (argc > 1) ? atoi(argv[1]) : 100;
    int nsteps = (argc > 2) ? atoi(argv[2]) : 100;
    int num_procs = (argc > 3) ? atoi(argv[3]) : 2;
    fname  = (argc > 4) ? argv[4] : NULL;
    double h = 1.0 / n;
    double h2 = h * h;
    
    // Crear memoria compartida para los arreglos u, f, utmp
    double *u = mmap(NULL, (n+1)*sizeof(double),
                     PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    double *f = mmap(NULL, (n+1)*sizeof(double),
                     PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    double *utmp = mmap(NULL, (n+1)*sizeof(double),
                        PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(u == MAP_FAILED || f == MAP_FAILED || utmp == MAP_FAILED){
        perror("mmap");
        exit(EXIT_FAILURE);
    }
    
    // Inicializar los arreglos
    memset(u, 0, (n+1)*sizeof(double));
    for(i = 0; i <= n; i++){
        f[i] = i * h;
    }
    utmp[0] = u[0];
    utmp[n] = u[n];
    
    // Crear la barrera en memoria compartida
    barrier_t *barrier = mmap(NULL, sizeof(barrier_t),
                              PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(barrier == MAP_FAILED){
        perror("mmap barrier");
        exit(EXIT_FAILURE);
    }
    barrier_init(barrier, num_procs);
    
    // Dividir el dominio entre los procesos (índices [1, n))
    int chunk = (n - 1) / num_procs;
    int remainder = (n - 1) % num_procs;
    int start = 1;
    
    // Arreglo para almacenar los pids de los procesos hijos
    pid_t *pids = malloc(num_procs * sizeof(pid_t));
    if(pids == NULL){
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    
    // Iniciar la medición del tiempo
    get_time(&tstart);
    
    for(i = 0; i < num_procs; i++){
        int extra = (i < remainder) ? 1 : 0;
        int end = start + chunk + extra;
        pid_t pid = fork();
        if(pid < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        } else if(pid == 0) {
            // Proceso hijo: ejecutar su porción
            int sweep, j;
            for(sweep = 0; sweep < nsteps - 1; sweep += 2) {
                // Primer sweep: calcular utmp a partir de u
                for(j = start; j < end; j++){
                    utmp[j] = (u[j-1] + u[j+1] + h2 * f[j]) / 2.0;
                }
                barrier_wait(barrier);
                // Segundo sweep: calcular u a partir de utmp
                for(j = start; j < end; j++){
                    u[j] = (utmp[j-1] + utmp[j+1] + h2 * f[j]) / 2.0;
                }
                barrier_wait(barrier);
            }
            // Si nsteps es impar, se realiza un sweep extra
            if(nsteps % 2 != 0) {
                for(j = start; j < end; j++){
                    utmp[j] = (u[j-1] + u[j+1] + h2 * f[j]) / 2.0;
                }
                barrier_wait(barrier);
                for(j = start; j < end; j++){
                    u[j] = utmp[j];
                }
                barrier_wait(barrier);
            }
            exit(EXIT_SUCCESS);
        } else {
            // Proceso padre guarda el pid del hijo
            pids[i] = pid;
        }
        start = end;
    }
    
    // El proceso padre espera a que todos los hijos finalicen
    for(i = 0; i < num_procs; i++){
        waitpid(pids[i], NULL, 0);
    }
    
    get_time(&tend);
    printf("n: %d\nnsteps: %d\nnum_procs: %d\nElapsed time: %g s\n",
           n, nsteps, num_procs, timespec_diff(tstart, tend));
    
    if(fname)
        write_solution(n, u, fname);
    
    // Limpieza: destruir la barrera y liberar la memoria compartida
    barrier_destroy(barrier);
    munmap(u, (n+1)*sizeof(double));
    munmap(f, (n+1)*sizeof(double));
    munmap(utmp, (n+1)*sizeof(double));
    munmap(barrier, sizeof(barrier_t));
    free(pids);
    
    return 0;
}