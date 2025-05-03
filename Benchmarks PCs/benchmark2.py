import subprocess
import re
import statistics
import sys
import logging
import datetime
import os
import matplotlib.pyplot as plt

# --- Configuración ---
# NUM_RUNS = 1 # Ya no se necesita promediar, solo 1 ejecución por thread
TEST_TIME = 60 # Duración de cada test en segundos
MAX_THREADS_M1 = 12
MEM_BLOCK_SIZE_M1 = "1M"
MEM_TOTAL_SIZE_M1 = "24G"

LOG_FILENAME = 'benchmark_machine1_plot.log'
PLOT_DIR = 'Máquina1' # Directorio para guardar plots
MACHINE_NAME = "Máquina 1"

# --- Configuración del Logging ---
os.makedirs(PLOT_DIR, exist_ok=True) # Crear directorio de plots si no existe
log_file_path = os.path.join(PLOT_DIR, LOG_FILENAME) # Poner log dentro del dir de plots

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler(log_file_path, mode='w'), # Log a archivo
        logging.StreamHandler(sys.stdout) # Log también a consola
    ]
)

# --- Función para ejecutar Sysbench ---
def run_sysbench(args):
    """Ejecuta sysbench con los argumentos dados y devuelve la salida."""
    command = ['sysbench'] + args
    command_str = ' '.join(command)
    logging.info(f"  Ejecutando: {command_str}")
    try:
        process = subprocess.run(command,
                                 capture_output=True,
                                 text=True,
                                 check=True,
                                 encoding='utf-8',
                                 errors='ignore')
        return process.stdout
    except FileNotFoundError:
        logging.error(f"Error CRÍTICO: El comando 'sysbench' no se encontró.", exc_info=True)
        raise # Re-lanza la excepción para detener el script
    except subprocess.CalledProcessError as e:
        logging.error(f"  ERROR: sysbench falló con código {e.returncode}.")
        logging.error(f"  Command: {command_str}")
        logging.error(f"  Stderr:\n{e.stderr}")
        logging.error(f"  Stdout:\n{e.stdout}")
        return None # Indica fallo
    except Exception as e:
        logging.error(f"  ERROR inesperado ejecutando sysbench: {e}", exc_info=True)
        logging.error(f"  Command: {command_str}")
        return None # Indica fallo

# --- Funciones de Parseo ---
def parse_cpu_output(output):
    """Extrae events per second de la salida de sysbench cpu."""
    if output is None: return None
    try:
        match = re.search(r"events per second:\s*([\d.]+)", output)
        if match:
            return float(match.group(1))
        else:
            logging.warning(f"No se pudo parsear 'events per second' de la salida:\n{output}")
            return None
    except Exception as e:
        logging.error(f"Error parseando CPU output: {e}", exc_info=True)
        return None

def parse_memory_output(output):
    """Extrae MiB/sec de la salida de sysbench memory."""
    if output is None: return None
    try:
        # Buscar la línea "XXX MiB transferred (YYY MiB/sec)"
        match = re.search(r"MiB transferred.*?\(\s*([\d.]+)\s*MiB/sec\)", output)
        if match:
            return float(match.group(1))
        else:
             # Intento alternativo si el formato cambia ligeramente o no hay paréntesis
             match_alt = re.search(r"transferred \d+\.\d+ MiB/sec", output) # Ajustar si es necesario
             if match_alt:
                  val_str = match_alt.group(0).split()[-2] # Obtener el número
                  return float(val_str)
             logging.warning(f"No se pudo parsear 'MiB/sec' de la salida:\n{output}")
             return None
    except Exception as e:
        logging.error(f"Error parseando Memory output: {e}", exc_info=True)
        return None

# --- Función de Ploteo ---
def plot_results(x_data, y_data, title, xlabel, ylabel, filename):
    """Genera y guarda un gráfico."""
    if not x_data or not y_data or len(x_data) != len(y_data):
        logging.warning(f"No se generará el gráfico '{filename}'. Datos insuficientes o inconsistentes.")
        return

    # Asegurar que solo ploteamos puntos con datos válidos en Y
    valid_indices = [i for i, y in enumerate(y_data) if y is not None]
    if not valid_indices:
        logging.warning(f"No se generará el gráfico '{filename}'. No hay datos válidos en Y.")
        return

    plot_x = [x_data[i] for i in valid_indices]
    plot_y = [y_data[i] for i in valid_indices]

    plt.figure(figsize=(10, 6))
    plt.plot(plot_x, plot_y, marker='o', linestyle='-')
    plt.title(title)
    plt.xlabel(xlabel)
    plt.ylabel(ylabel)
    plt.xticks(plot_x) # Asegura que todos los números de threads aparezcan en el eje X
    plt.grid(True, which='both', linestyle='--', linewidth=0.5)
    plt.tight_layout()
    filepath = os.path.join(PLOT_DIR, filename)
    try:
        plt.savefig(filepath)
        logging.info(f"Gráfico guardado en: {filepath}")
    except Exception as e:
        logging.error(f"Error al guardar el gráfico {filepath}: {e}", exc_info=True)
    plt.close() # Cierra la figura para liberar memoria

# --- Bucle Principal ---
logging.info(f"--- Iniciando Benchmarks {MACHINE_NAME} ---")
logging.info(f"Resultados y logs en: {PLOT_DIR}")
logging.info(f"Duración de cada test: {TEST_TIME}s")

# 1. Test CPU
logging.info("\n===== Iniciando Test CPU =====")
cpu_threads_list = list(range(1, MAX_THREADS_M1 + 1))
cpu_events_per_sec_list = []

for threads in cpu_threads_list:
    logging.info(f"\n--- CPU Test: {threads} thread(s) ---")
    args = ["cpu", f"--threads={threads}", f"--time={TEST_TIME}", "run"]
    output = run_sysbench(args)
    events_per_sec = parse_cpu_output(output)
    cpu_events_per_sec_list.append(events_per_sec)
    if events_per_sec is not None:
        logging.info(f"  Resultado CPU ({threads} threads): {events_per_sec:.2f} events/sec")
    else:
        logging.warning(f"  Resultado CPU ({threads} threads): Fallido o no parseado")

# Generar plot CPU
plot_results(
    x_data=cpu_threads_list,
    y_data=cpu_events_per_sec_list,
    title=f'{MACHINE_NAME} - Sysbench CPU Benchmark (Time={TEST_TIME}s)',
    xlabel='Número de Threads',
    ylabel='Events Per Second',
    filename=f'{MACHINE_NAME.replace(" ", "_").lower()}_cpu_performance.png'
)

# 2. Test Memory
logging.info(f"\n===== Iniciando Test Memory ({MEM_BLOCK_SIZE_M1} block, {MEM_TOTAL_SIZE_M1} total) =====")
mem_threads_list = list(range(1, MAX_THREADS_M1 + 1))
mem_transfer_rate_list = []

for threads in mem_threads_list:
    logging.info(f"\n--- Memory Test ({MEM_TOTAL_SIZE_M1}): {threads} thread(s) ---")
    args = [
        "memory",
        f"--threads={threads}",
        f"--memory-block-size={MEM_BLOCK_SIZE_M1}",
        f"--memory-total-size={MEM_TOTAL_SIZE_M1}",
        f"--time={TEST_TIME}",
        "run"
    ]
    output = run_sysbench(args)
    transfer_rate = parse_memory_output(output)
    mem_transfer_rate_list.append(transfer_rate)
    if transfer_rate is not None:
        logging.info(f"  Resultado Memory ({threads} threads, {MEM_TOTAL_SIZE_M1}): {transfer_rate:.2f} MiB/sec")
    else:
        logging.warning(f"  Resultado Memory ({threads} threads, {MEM_TOTAL_SIZE_M1}): Fallido o no parseado")

# Generar plot Memory
plot_results(
    x_data=mem_threads_list,
    y_data=mem_transfer_rate_list,
    title=f'{MACHINE_NAME} - Sysbench Memory Benchmark ({MEM_TOTAL_SIZE_M1}, {MEM_BLOCK_SIZE_M1} block, Time={TEST_TIME}s)',
    xlabel='Número de Threads',
    ylabel='Transferencia (MiB/sec)',
    filename=f'{MACHINE_NAME.replace(" ", "_").lower()}_memory_{MEM_TOTAL_SIZE_M1}_performance.png'
)

logging.info(f"\n--- Benchmarks {MACHINE_NAME} Completados ---")
