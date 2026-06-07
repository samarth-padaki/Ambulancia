import serial
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.animation import FuncAnimation
import argparse

# --- CONFIGURATION ---
# Labels must match your model's IMAI_DATA_OUT_SYMBOLS order!
# Assuming: Unlabeled, N, NE, E, SE, S, SW, W, NW
LABELS = ["None", "N", "NE", "E", "SE", "S", "SW", "W", "NW"]
THRESHOLD = 0.4

def main():
    parser = argparse.ArgumentParser(description='Visualize AI direction data from serial port.')
    parser.add_argument('--port', type=str, default='/dev/cu.usbmodem1103', help='Serial port (e.g., COM3 or /dev/cu.usbmodem1103)')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate (default: 115200)')
    args = parser.parse_args()

    # --- SETUP ---
    try:
        ser = serial.Serial(args.port, args.baud, timeout=0.1)
    except Exception as e:
        print(f"Error: Could not open serial port {args.port}. {e}")
        return

    fig, ax = plt.subplots(subplot_kw={'projection': 'polar'})
    ax.set_theta_zero_location("N")  # North at top
    ax.set_theta_direction(-1)       # Clockwise
    ax.set_ylim(0, 1)                # Score range 0 to 1

    # Create bars for each direction (skip "None" for the compass plot)
    num_directions = len(LABELS) - 1
    angles = np.linspace(0, 2 * np.pi, num_directions, endpoint=False)
    bars = ax.bar(angles, [0]*num_directions, width=0.5, bottom=0.0, alpha=0.5)

    def update(frame):
        try:
            line = ser.readline().decode('utf-8').strip()
            if line.startswith("AI_DATA:"):
                # Parse scores: AI_DATA:0.1,0.8,0.0...
                data = line.split(":")[1]
                scores = [float(x) for x in data.split(",")]
                
                # Map scores to the UI (ignoring "None/Unlabeled" which is usually index 0)
                directional_scores = scores[1:] 
                
                max_val = max(directional_scores)
                max_idx = directional_scores.index(max_val)

                for i, bar in enumerate(bars):
                    val = directional_scores[i]
                    bar.set_height(val)
                    # Highlight the strongest direction in Green, others in Blue
                    if val == max_val and val > THRESHOLD:
                        bar.set_facecolor('green')
                        bar.set_alpha(1.0)
                    else:
                        bar.set_facecolor('blue')
                        bar.set_alpha(0.3)
                
                # Update title with best result
                if max_val > THRESHOLD:
                    plt.title(f"Direction: {LABELS[max_idx+1]} ({max_val:.2f})", size=15)
                else:
                    plt.title("Listening...", size=15)
                    
        except Exception as e:
            pass
        return bars

    print(f"Connecting to {args.port} at {args.baud} baud...")
    print("Close the plot window to exit.")
    ani = FuncAnimation(fig, update, interval=50, blit=False)
    plt.show()
    ser.close()

if __name__ == "__main__":
    main()
