import matplotlib.pyplot as plt
import numpy as np

def plot_tiempo_promedio(n_values, tiempo_data, labels, title="Tiempo Promedio vs Tamaño del Problema (N)"):
    """
    Grafica el tiempo promedio en función de N para diferentes configuraciones.
    
    Args:
        n_values: Lista de valores de N (tamaños del problema)
        tiempo_data: Lista de listas con los tiempos promedio para cada configuración
        labels: Nombres de las configuraciones para la leyenda
        title: Título del gráfico
    """
    plt.figure(figsize=(12, 8))
    markers = ['o', 's', '^', 'D', 'x']
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd']
    
    for i, (data, label) in enumerate(zip(tiempo_data, labels)):
        plt.plot(n_values, data, marker=markers[i], label=label, color=colors[i], 
                 linewidth=2, markersize=8)
    
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.xlabel('Tamaño del problema (N)', fontsize=12)
    plt.ylabel('Tiempo Promedio (segundos)', fontsize=12)
    plt.title(title, fontsize=14)
    plt.legend(fontsize=10, loc='upper left')
    
    # Usar escala logarítmica en ambos ejes para mejor visualización
    plt.xscale('log')
    plt.yscale('log')
    
    # Mejorar formato de los ejes
    plt.gca().xaxis.set_major_formatter(plt.FuncFormatter(lambda x, p: f'{int(x):,}'))
    
    plt.tight_layout()
    plt.savefig('jacobi_tiempo_promedio.png', dpi=300, bbox_inches='tight')
    plt.show()

def plot_tiempo_comparativo(n_values, tiempo_data, labels, title="Comparación de Tiempos - Método Jacobi-Poisson 1D"):
    """
    Versión alternativa con mejor visualización para comparar tiempos
    """
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(16, 6))
    
    markers = ['o', 's', '^', 'D', 'x']
    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd']
    
    # Gráfico 1: Escala normal
    for i, (data, label) in enumerate(zip(tiempo_data, labels)):
        ax1.plot(n_values, data, marker=markers[i], label=label, color=colors[i], 
                 linewidth=2, markersize=8)
    
    ax1.grid(True, linestyle='--', alpha=0.7)
    ax1.set_xlabel('Tamaño del problema (N)', fontsize=12)
    ax1.set_ylabel('Tiempo Promedio (segundos)', fontsize=12)
    ax1.set_title('Escala Linear', fontsize=14)
    ax1.legend(fontsize=9)
    ax1.xaxis.set_major_formatter(plt.FuncFormatter(lambda x, p: f'{int(x/1000):,}K'))
    
    # Gráfico 2: Escala logarítmica
    for i, (data, label) in enumerate(zip(tiempo_data, labels)):
        ax2.loglog(n_values, data, marker=markers[i], label=label, color=colors[i], 
                   linewidth=2, markersize=8)
    
    ax2.grid(True, linestyle='--', alpha=0.7)
    ax2.set_xlabel('Tamaño del problema (N)', fontsize=12)
    ax2.set_ylabel('Tiempo Promedio (segundos)', fontsize=12)
    ax2.set_title('Escala Logarítmica', fontsize=14)
    ax2.legend(fontsize=9)
    ax2.xaxis.set_major_formatter(plt.FuncFormatter(lambda x, p: f'{int(x/1000):,}K'))
    
    plt.suptitle(title, fontsize=16)
    plt.tight_layout()
    plt.savefig('jacobi_tiempo_comparativo.png', dpi=300, bbox_inches='tight')
    plt.show()

# Valores de N (tamaños del problema)
n_values = [10000, 50000, 100000, 500000, 1000000]

# Tiempos promedio para cada configuración (en segundos)
# NOTA: Reemplaza estos valores con tus datos reales
tiempo_pthreads_4 = [0.025462, 0.058525, 0.094212, 3.005759, 8.025866]      # Máquina 1 - 4 hilos pthreads
tiempo_pthreads_12 = [0.025550, 0.058098, 0.097355, 2.780131, 8.108239]     # Máquina 1 - 12 hilos pthreads
tiempo_openmp_4 = [0.007783, 0.0177430, 0.030177, 1.379991, 3.882430]        # Máquina 1 - 4 hilos openmp
tiempo_openmp_12 = [0.028022, 0.049433, 0.060699, 1.478105, 4.946107]       # Máquina 1 - 12 hilos openmp
tiempo_mpi_4maq = [1.272224, 1.761694, 1.388986, 2.024053, 2.477306]        # Máquina 2 - 4 máquinas MPI+OpenMP

# Agrupar todos los datos de tiempo
tiempo_data = [
    tiempo_pthreads_4,
    tiempo_pthreads_12,
    tiempo_openmp_4,
    tiempo_openmp_12,
    tiempo_mpi_4maq
]

# Etiquetas para la leyenda
labels = [
    "Máquina 1 - 4 hilos pthreads",
    "Máquina 1 - 12 hilos pthreads", 
    "Máquina 1 - 4 hilos openmp",
    "Máquina 1 - 12 hilos openmp",
    "Máquina 2 - 4 máquinas MPI+OpenMP"
]

# Generar los gráficos
print("Generando gráfico de tiempos promedio...")
plot_tiempo_promedio(n_values, tiempo_data, labels, "Tiempos de Ejecución - Método Jacobi-Poisson 1D")

print("Generando gráfico comparativo...")
plot_tiempo_comparativo(n_values, tiempo_data, labels)

# Función adicional para mostrar estadísticas
def mostrar_estadisticas(n_values, tiempo_data, labels):
    """
    Muestra estadísticas básicas de los tiempos
    """
    print("\n=== ESTADÍSTICAS DE TIEMPOS ===")
    print(f"{'Configuración':<35} {'Min (s)':<10} {'Max (s)':<10} {'Promedio (s)':<12}")
    print("-" * 70)
    
    for i, (data, label) in enumerate(zip(tiempo_data, labels)):
        min_time = min(data)
        max_time = max(data)
        avg_time = sum(data) / len(data)
        print(f"{label:<35} {min_time:<10.4f} {max_time:<10.4f} {avg_time:<12.4f}")
    
    print("\n=== MEJOR RENDIMIENTO POR TAMAÑO ===")
    print(f"{'N':<10} {'Mejor Configuración':<35} {'Tiempo (s)':<12}")
    print("-" * 58)
    
    for i, n in enumerate(n_values):
        tiempos_n = [data[i] for data in tiempo_data]
        min_idx = tiempos_n.index(min(tiempos_n))
        print(f"{n:<10} {labels[min_idx]:<35} {tiempos_n[min_idx]:<12.4f}")

# Mostrar estadísticas
mostrar_estadisticas(n_values, tiempo_data, labels)