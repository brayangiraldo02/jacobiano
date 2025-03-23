#!/bin/bash

# Crear archivo para resultados
echo "N,NSTEPS,TIEMPO(s)" > resultados_benchmark.csv

# Array de valores N a probar
N_VALUES=(10000 50000 100000 500000 1000000)
# N_VALUES=(833333332)

# Array de valores NSTEPS a probar
NSTEPS_VALUES=(100 500 1000 2000 5000)
# NSTEPS_VALUES=(1000)

# Número de hilos
NUM_THREADS=4

# Compilar el programa
gcc -DUSE_CLOCK -O3 threads4-jacobi1d.c timing.c -pthread -march=native -funroll-loops -o jacobi1d

# Ejecutar benchmarks
for N in "${N_VALUES[@]}"; do
    for STEPS in "${NSTEPS_VALUES[@]}"; do
        echo "Ejecutando con N=$N, NSTEPS=$STEPS"
        TIEMPO=$(./jacobi1d $N $STEPS $NUM_THREADS | grep "Elapsed time" | awk '{print $3}')
        echo "$N,$STEPS,$TIEMPO" >> resultados_benchmark.csv
        # Pequeña pausa para que el sistema se recupere
        sleep 1
    done
done

echo "Benchmark completado. Resultados en resultados_benchmark.csv"