import matplotlib.pyplot as plt
import numpy as np

def plot_data(file_path='u_serial.out', output_file='plot.png'):
    # Read data from the file
    x = []
    y = []
    
    with open(file_path, 'r') as file:
        for line in file:
            values = line.strip().split()
            if len(values) == 2:
                try:
                    x_val = float(values[0])
                    y_val = float(values[1])
                    x.append(x_val)
                    y.append(y_val)
                except ValueError:
                    # Skip lines that can't be converted to float
                    continue
    
    # Create the plot
    plt.figure(figsize=(10, 6))
    plt.plot(x, y, 'b-', linewidth=2)
    
    # Invert the y-axis so 0 is at the top
    plt.gca().invert_yaxis()
    
    plt.grid(True, linestyle='--', alpha=0.7)
    plt.xlabel('X', fontsize=12)
    plt.ylabel('Y', fontsize=12)
    plt.title(f'Plot from {file_path}', fontsize=14)
    
    # Improve layout
    plt.tight_layout()
    
    # Save the plot
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Plot saved to {output_file}")
    
    # Display the plot
    plt.show()

if __name__ == "__main__":
    plot_data()