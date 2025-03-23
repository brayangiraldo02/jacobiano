import numpy as np
import matplotlib.pyplot as plt

# Cargar los datos desde el archivo
data = np.loadtxt('u_serial.out')

# Eliminar la última línea donde x es igual a 1
# data = data[data[:, 0] < 1]

x = data[:, 0]
y = data[:, 1]

# Para evitar problemas con el logaritmo, sustituir los ceros por un valor muy pequeño
epsilon = 1e-12  
y_plot = np.where(y == 0, epsilon, y)

plt.figure(figsize=(10, 6))
plt.semilogy(x, y_plot, lw=1)
plt.gca().invert_yaxis()  # Invierte el eje y para que el valor 0 aparezca en la parte superior
plt.xlabel('x')
plt.ylabel('y (escala logarítmica invertida)')
plt.title('Curva de datos en escala logarítmica invertida')

# Guardar la imagen antes de mostrarla
plt.savefig('jacobi_solucion.png', dpi=300, bbox_inches='tight')

# Luego mostrarla (opcional, puedes eliminar esta línea si solo quieres guardarla)
plt.show()