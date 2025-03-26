import matplotlib.pyplot as plt
import numpy as np

def plot_speedup(n_values, speedup_data, labels, title="Speedup vs Problem Size (N)"):
    """
    Grafica el speedup en función de N para diferentes configuraciones.
    
    Args:
        n_values: Lista de valores de N (tamaños del problema)
        speedup_data: Lista de listas con los speedups para cada configuración
        labels: Nombres de las configuraciones para la leyenda
        title: Título del gráfico
    """
    plt.figure(figsize=(10, 6))
    markers = ['o', 's', '^', 'D', 'x']
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd']
    
    for i, (data, label) in enumerate(zip(speedup_data, labels)):
        plt.plot(n_values, data, marker=markers[i], label=label, color=colors[i], 
                 linewidth=2, markersize=8)
    
    # Línea de referencia para speedup=1 (rendimiento igual al secuencial)
    # plt.axhline(y=1, color='gray', linestyle='--', alpha=0.7, label='Secuencial')
    
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.xlabel('Tamaño del problema (N)', fontsize=12)
    plt.ylabel('Speedup', fontsize=12)
    plt.title(title, fontsize=14)
    plt.legend(fontsize=10)
    
    # Mejorar la apariencia del eje x si hay muchos valores de N
    if len(n_values) > 10:
        plt.xticks(n_values[::len(n_values)//10])
    
    plt.yscale('log')
    plt.tight_layout()
    plt.savefig('jacobi_speedup.png', dpi=300)
    plt.show()

# Ejemplo de uso:
# Supongamos que estos son tus valores de N
n_values = [10000, 50000, 100000, 500000, 1000000]

# Y estos son los speedups para cada configuración
# (estos son valores de ejemplo, reemplázalos con tus datos reales)
speedup_2_threads = [0.470522, 1.288695, 1.394428, 0.500509, 0.385104]  # Ejemplo para 2 hilos
speedup_4_threads = [0.640994, 1.255446, 1.579055, 0.596547, 0.376401]  # Ejemplo para 4 hilos
speedup_8_threads = [0.659887, 0.261465, 1.265006, 0.478186, 0.364140]  # Ejemplo para 8 hilos
speedup_12_threads = [0.638786, 1.264673, 1.528077, 0.644961, 0.372577]  # Ejemplo para 12 hilos
speedup_processes = [16.756673, 68.348837, 137.111520, 1234.051617, 1408.365501]  # Ejemplo para procesos

# Agrupar todos los datos de speedup
speedup_data = [
    speedup_2_threads,
    speedup_4_threads,
    speedup_8_threads,
    speedup_12_threads,
    speedup_processes
]

labels = ["2 Hilos", "4 Hilos", "8 Hilos", "12 Hilos", "Procesos"]

# Generar el gráfico
plot_speedup(n_values, speedup_data, labels, "Speedup del método Jacobi-Poisson 1D")