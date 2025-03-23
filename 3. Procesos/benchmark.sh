#!/bin/bash

# Crear archivo para resultados
echo "N,NSTEPS,TIEMPO(s)" > resultados_benchmark.csv

# Array de valores N a probar
# N_VALUES=(10000 50000 100000 500000 1000000)
N_VALUES=(833333332)

# Array de valores NSTEPS a probar
# NSTEPS_VALUES=(100 500 1000 2000 5000)
NSTEPS_VALUES=(1000)

# Número de procesos a utilizar
NUM_PROCS=12

# Compilar el programa (se requiere -lrt para semáforos y tiempo, en algunos sistemas)
gcc -DUSE_CLOCK -O3 processes-jacobi1d.c timing.c -o jacobi -lrt

# Ejecutar benchmarks
for N in "${N_VALUES[@]}"; do
    for STEPS in "${NSTEPS_VALUES[@]}"; do
        echo "Ejecutando con N=$N, NSTEPS=$STEPS"
        TIEMPO=$(./jacobi $N $STEPS $NUM_PROCS | grep "Elapsed time" | awk '{print $3}')
        echo "$N,$STEPS,$TIEMPO" >> resultados_benchmark.csv
        # Pequeña pausa para que el sistema se recupere
        sleep 1
    done
done

echo "Benchmark completado. Resultados en resultados_benchmark.csv"