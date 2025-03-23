#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include "timing.h"

// Variables globales compartidas entre hilos
int    n, nsteps;
double *u, *f, *utmp;
double h, h2;
int num_threads;

// Barrera para sincronización de hilos
pthread_barrier_t barrier;

// Estructura para enviar datos a cada hilo
typedef struct {
    int tid;      // ID del hilo
    int istart;   // índice de inicio de la porción a computar (excluyendo 0)
    int iend;     // índice final (exclusivo) de la porción a computar
} thread_data_t;

// Función que realizan los hilos para computar la iteración de Jacobi
void* jacobi_thread(void* arg) {
    thread_data_t *data = (thread_data_t*) arg;
    int i, sweep;
    // Ejecutamos nsweeps en grupos de dos (como en la versión secuencial)
    for (sweep = 0; sweep < nsteps - 1; sweep += 2) {
        // Primer sweep: calcular utmp basado en u
        for (i = data->istart; i < data->iend; i++) {
            utmp[i] = (u[i-1] + u[i+1] + h2 * f[i]) / 2.0;
        }
        // Sincronización: esperar a que todos hayan escrito en utmp
        pthread_barrier_wait(&barrier);
        // Segundo sweep: calcular u basado en utmp
        for (i = data->istart; i < data->iend; i++) {
            u[i] = (utmp[i-1] + utmp[i+1] + h2 * f[i]) / 2.0;
        }
        // Sincronización: esperar a que todos hayan escrito en u
        pthread_barrier_wait(&barrier);
    }
    // Si nsteps es impar, se realiza un sweep extra
    if(nsteps % 2 != 0) {
        for (i = data->istart; i < data->iend; i++) {
            utmp[i] = (u[i-1] + u[i+1] + h2 * f[i]) / 2.0;
        }
        pthread_barrier_wait(&barrier);
        // Copiar la frontera calculada en utmp de vuelta a u
        for (i = data->istart; i < data->iend; i++) {
            u[i] = utmp[i];
        }
        pthread_barrier_wait(&barrier);
    }
    pthread_exit(NULL);
}

// Función para escribir la solución en un archivo
void write_solution(int n, double* u, const char* fname) {
    int i;
    FILE* fp = fopen(fname, "w+");
    if(fp == NULL){
        fprintf(stderr, "Error al abrir el archivo %s\n", fname);
        return;
    }
    for (i = 0; i <= n; ++i)
        fprintf(fp, "%g %g\n", i * h, u[i]);
    fclose(fp);
}

int main(int argc, char** argv) {
    int i;
    char* fname;
    pthread_t *threads;
    thread_data_t *thread_data;
    timing_t tstart, tend;

    // Procesar argumentos  
    // Uso: ./jacobi_pthread [n] [nsteps] [num_threads] [fname-opcional]
    n         = (argc > 1) ? atoi(argv[1]) : 100;
    nsteps    = (argc > 2) ? atoi(argv[2]) : 100;
    num_threads = (argc > 3) ? atoi(argv[3]) : 2;
    fname     = (argc > 4) ? argv[4] : NULL;
    h         = 1.0 / n;
    h2        = h * h;
    
    // Asignar e inicializar arreglos
    u    = (double*) malloc((n+1) * sizeof(double));
    f    = (double*) malloc((n+1) * sizeof(double));
    utmp = (double*) malloc((n+1) * sizeof(double));
    if(u == NULL || f == NULL || utmp == NULL) {
        fprintf(stderr, "Error al asignar memoria\n");
        exit(EXIT_FAILURE);
    }
    memset(u, 0, (n+1) * sizeof(double));
    // Se definen condiciones de frontera: u[0] y u[n] se mantienen constantes
    for (i = 0; i <= n; ++i) {
        f[i] = i * h;
    }
    utmp[0] = u[0];
    utmp[n] = u[n];

    // Inicializar la barrera con el número de hilos
    pthread_barrier_init(&barrier, NULL, num_threads);

    // Reservar memoria para hilos y datos
    threads = (pthread_t*) malloc(num_threads * sizeof(pthread_t));
    thread_data = (thread_data_t*) malloc(num_threads * sizeof(thread_data_t));
    if(threads == NULL || thread_data == NULL) {
        fprintf(stderr, "Error al asignar memoria para los hilos\n");
        exit(EXIT_FAILURE);
    }
    
    // Calcular el tamaño del trabajo de cada hilo
    // Los índices [1, n) se dividen entre los hilos
    int chunk = (n - 1) / num_threads;
    int remainder = (n - 1) % num_threads;
    int start = 1;
    
    // Iniciar la medición del tiempo
    get_time(&tstart);
    
    // Crear hilos
    for (i = 0; i < num_threads; i++) {
        thread_data[i].tid = i;
        thread_data[i].istart = start;
        // Distribuir el resto si es necesario
        int extra = (i < remainder) ? 1 : 0;
        thread_data[i].iend = start + chunk + extra;
        start = thread_data[i].iend;
        if(pthread_create(&threads[i], NULL, jacobi_thread, (void*) &thread_data[i]) != 0) {
            fprintf(stderr, "Error al crear el hilo %d\n", i);
            exit(EXIT_FAILURE);
        }
        // Fijar la afinidad del hilo al CPU (de forma cíclica si es necesario)
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        int cpu = i % sysconf(_SC_NPROCESSORS_ONLN);
        CPU_SET(cpu, &cpuset);
        if(pthread_setaffinity_np(threads[i], sizeof(cpu_set_t), &cpuset) != 0) {
            fprintf(stderr, "Error al fijar la afinidad del hilo %d\n", i);
        }
    }
    
    // Esperar a que todos los hilos terminen
    for (i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Finalizar la medición del tiempo
    get_time(&tend);
    printf("n: %d\nnsteps: %d\nnum_threads: %d\nElapsed time: %g s\n", 
           n, nsteps, num_threads, timespec_diff(tstart, tend));
    
    // Escribir la solución en el archivo si se indicó un nombre
    if (fname)
        write_solution(n, u, fname);
    
    // Liberar memoria y destruir la barrera
    free(u);
    free(f);
    free(utmp);
    free(threads);
    free(thread_data);
    pthread_barrier_destroy(&barrier);
    
    return 0;
}