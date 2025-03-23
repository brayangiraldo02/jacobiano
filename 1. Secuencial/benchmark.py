import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import seaborn as sns

# Leer datos
df = pd.read_csv('resultados_benchmark.csv')

# Convertir columnas a valores numéricos
df['N'] = pd.to_numeric(df['N'])
df['NSTEPS'] = pd.to_numeric(df['NSTEPS'])
df['TIEMPO(s)'] = pd.to_numeric(df['TIEMPO(s)'])

# 1. Tabla pivote para mostrar resultados organizados
pivot_table = df.pivot_table(index='N', columns='NSTEPS', values='TIEMPO(s)')
print("Tabla de tiempos (segundos):")
print(pivot_table)

# Guardar tabla como CSV
pivot_table.to_csv('tabla_tiempos.csv')

# Guardar tabla como HTML con formato
html_table = pivot_table.style.background_gradient(cmap='viridis').format("{:.3f}").to_html()
with open('tabla_tiempos.html', 'w') as f:
    f.write('<html><head><title>Tabla de Tiempos</title>')
    f.write('<style>table {border-collapse: collapse; font-family: Arial;} th, td {padding: 8px; border: 1px solid #ddd;} th {background-color: #f2f2f2;}</style>')
    f.write('</head><body>')
    f.write('<h2>Tabla de Tiempos (segundos)</h2>')
    f.write(html_table)
    f.write('</body></html>')

# Guardar tabla como imagen
plt.figure(figsize=(12, 8))
ax = plt.subplot(111, frame_on=False)
ax.xaxis.set_visible(False)
ax.yaxis.set_visible(False)
tabla = pd.plotting.table(ax, pivot_table, loc='center')
tabla.auto_set_font_size(False)
tabla.set_fontsize(10)
tabla.scale(1.2, 1.2)
plt.savefig('tabla_tiempos.png', bbox_inches='tight', dpi=200)

# 2. Gráfico: Tiempo vs N para diferentes NSTEPS
plt.figure(figsize=(10, 6))
for steps in df['NSTEPS'].unique():
    subset = df[df['NSTEPS'] == steps]
    plt.plot(subset['N'], subset['TIEMPO(s)'], 'o-', label=f'NSTEPS={steps}')

plt.xlabel('Tamaño de la malla (N)')
plt.ylabel('Tiempo de ejecución (s)')
plt.title('Escalado del tiempo con el tamaño de la malla')
plt.grid(True)
plt.legend()
plt.xscale('log')
plt.yscale('log')
plt.savefig('tiempo_vs_N.png')

# 3. Gráfico: Tiempo vs NSTEPS para diferentes N
plt.figure(figsize=(10, 6))
for n in df['N'].unique():
    subset = df[df['N'] == n]
    plt.plot(subset['NSTEPS'], subset['TIEMPO(s)'], 'o-', label=f'N={n}')

plt.xlabel('Número de iteraciones (NSTEPS)')
plt.ylabel('Tiempo de ejecución (s)')
plt.title('Escalado del tiempo con el número de iteraciones')
plt.grid(True)
plt.legend()
plt.xscale('log')
plt.yscale('log')
plt.savefig('tiempo_vs_NSTEPS.png')

# 4. Gráfico de calor para visualizar toda la matriz de resultados
plt.figure(figsize=(10, 8))
pivot_df = df.pivot(index='N', columns='NSTEPS', values='TIEMPO(s)')
sns.heatmap(pivot_df, annot=True, fmt='.3g', cmap='viridis')
plt.title('Mapa de calor: Tiempo de ejecución (s)')
plt.savefig('heatmap_tiempos.png')

print("Gráficos generados: tiempo_vs_N.png, tiempo_vs_NSTEPS.png, heatmap_tiempos.png")