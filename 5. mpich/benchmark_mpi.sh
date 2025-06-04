#!/bin/bash
# Benchmark completo para producción

N_VALUES=(10000 50000 100000 500000 1000000)
NSTEPS_VALUES=(100 500 1000 2000 5000)
PROCESSES=(2 4)
THREADS_PER_PROCESS=4

echo "=== BENCHMARK MPI + OpenMP - PRODUCCION ==="
echo "Fecha: $(date)"
echo "Usuario: $(whoami)"
echo "Directorio: $(pwd)"
echo ""

echo "Verificando nodos del cluster..."
for node in head wn1 wn2 wn3; do
    if [ "$node" = "head" ]; then
        echo "[LOCAL] $node: $(hostname)"
    else
        if ssh -o ConnectTimeout=5 $node "hostname" 2>/dev/null; then
            REMOTE_HOST=$(ssh -o ConnectTimeout=5 $node "hostname" 2>/dev/null)
            echo "[OK] $node -> $REMOTE_HOST"
        else
            echo "[ERROR] $node: no accesible"
        fi
    fi
done

echo ""
echo "Configuracion del benchmark:"
echo "  - Valores N: ${N_VALUES[@]}"
echo "  - Valores NSTEPS: ${NSTEPS_VALUES[@]}"
echo "  - Procesos MPI: ${PROCESSES[@]}"
echo "  - Hilos OpenMP por proceso: $THREADS_PER_PROCESS"
echo ""

TOTAL_TESTS=$((${#PROCESSES[@]} * ${#N_VALUES[@]} * ${#NSTEPS_VALUES[@]}))
echo "Total de pruebas a realizar: $TOTAL_TESTS"
echo "=================================================="

TEST_COUNT=0

for P in "${PROCESSES[@]}"; do
  OUTFILE="resultados_benchmark_mpi_${P}p_${THREADS_PER_PROCESS}t.csv"
  echo "N,NSTEPS,TIEMPO(s),PROCESOS,HILOS_POR_PROCESO,MAQUINAS,FECHA" > "$OUTFILE"

  echo ""
  echo "INICIANDO SERIE CON $P PROCESOS MPI"
  
  if [ $P -eq 2 ]; then
    HOSTS_USED="head,wn1"
    DISTRIBUTION="head(1)+wn1(1)"
  elif [ $P -eq 4 ]; then
    HOSTS_USED="head,wn1,wn2,wn3"
    DISTRIBUTION="head(1)+wn1(1)+wn2(1)+wn3(1)"
  else
    HOSTS_USED="head,wn1,wn2,wn3"
    DISTRIBUTION="automatica"
  fi
  
  echo "Distribucion: $DISTRIBUTION"
  echo "Cores totales: $((P * THREADS_PER_PROCESS))"
  echo "Archivo de salida: $OUTFILE"
  echo "--------------------------------------------------"

  for N in "${N_VALUES[@]}"; do
    for STEPS in "${NSTEPS_VALUES[@]}"; do
      TEST_COUNT=$((TEST_COUNT + 1))
      printf "[%3d/%3d] N=%-8s STEPS=%-6s -> " "$TEST_COUNT" "$TOTAL_TESTS" "$N" "$STEPS"
      
      START_TIME=$(date +%s)
      RESULTADO=$(mpirun -np $P -host $HOSTS_USED ./jacobi1d_mpi_openmp $N $STEPS $THREADS_PER_PROCESS 2>&1)
      EXIT_CODE=$?
      END_TIME=$(date +%s)
      WALL_TIME=$((END_TIME - START_TIME))
      
      if [ $EXIT_CODE -eq 0 ]; then
        TIEMPO=$(echo "$RESULTADO" | grep "Elapsed time" | awk '{print $3}')
        if [ -n "$TIEMPO" ]; then
          printf "OK %8.4f s (wall: %ds) [%s]\n" "$TIEMPO" "$WALL_TIME" "$DISTRIBUTION"
          echo "$N,$STEPS,$TIEMPO,$P,$THREADS_PER_PROCESS,$DISTRIBUTION,$(date)" >> "$OUTFILE"
        else
          echo "ERROR: no se pudo extraer el tiempo"
          echo "$N,$STEPS,ERROR_PARSE,$P,$THREADS_PER_PROCESS,$DISTRIBUTION,$(date)" >> "$OUTFILE"
        fi
      else
        echo "ERROR: fallo en ejecucion MPI (exit code: $EXIT_CODE)"
        echo "$N,$STEPS,ERROR_MPI,$P,$THREADS_PER_PROCESS,$DISTRIBUTION,$(date)" >> "$OUTFILE"
      fi
      
      sleep 2
    done
    echo ""
  done

  echo "SERIE CON $P PROCESOS COMPLETADA"
  echo "Resultados en: $OUTFILE"
  echo "=================================================="
done

echo ""
echo "BENCHMARK COMPLETO FINALIZADO"
echo "Hora de finalizacion: $(date)"
echo ""
echo "Resumen de archivos generados:"
for P in "${PROCESSES[@]}"; do
  OUTFILE="resultados_benchmark_mpi_${P}p_${THREADS_PER_PROCESS}t.csv"
  if [ -f "$OUTFILE" ]; then
    LINES=$(wc -l < "$OUTFILE")
    SIZE=$(du -h "$OUTFILE" | cut -f1)
    echo "  $OUTFILE: $((LINES-1)) mediciones ($SIZE)"
  fi
done