#!/bin/bash
# Genera CSV con tiempos usando OpenMP, separando resultados por número de hilos

N_VALUES=(10000 50000 100000 500000 1000000)
NSTEPS_VALUES=(100 500 1000 2000 5000)
THREADS=(4 12)

for T in "${THREADS[@]}"; do
  OUTFILE="resultados_benchmark_omp_${T}.csv"
  echo "N,NSTEPS,TIEMPO(s)" > "$OUTFILE"

  for N in "${N_VALUES[@]}"; do
    for STEPS in "${NSTEPS_VALUES[@]}"; do
      echo "Ejecutando N=$N, STEPS=$STEPS, HILOS=$T"
      TIEMPO=$(./jacobi1d_openmp $N $STEPS $T | grep "Elapsed time" | awk '{print $3}')
      echo "$N,$STEPS,$TIEMPO" >> "$OUTFILE"
      sleep 1
    done
  done

  echo "Benchmark OpenMP con $T hilos completado. Resultados en $OUTFILE"
done
