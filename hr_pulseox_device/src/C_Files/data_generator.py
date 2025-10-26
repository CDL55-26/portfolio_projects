import numpy as np
import matplotlib.pyplot as plt

# ----------------------------------------------------------
# Step 1: Generate Noisy Data and Save to raw_data.csv
# ----------------------------------------------------------
N = 500            # Total number of data points
sample_rate = 100   # Sampling rate (samples per second)
t = np.arange(N) / sample_rate  # Time vector

frequency = 1.2     # Frequency of the sine wave in Hz
amplitude = 50

# Create a sinusoidal signal (shifted positive) and add uniform noise in [-5,5]
signal = amplitude * (1 + np.sin(2 * np.pi * frequency * t))
noise = np.random.uniform(-5, 5, N)
data = signal + noise

# Convert to integer values (matching expected input for the C code)
data_int = np.round(data).astype(int)

# Write the data to raw_data.csv (no header; comma-separated)
with open("raw_data.csv", "w") as f:
    f.write(",".join(map(str, data_int)))

# ----------------------------------------------------------
# Step 2: Moving Average Smoothing (Window size = 10)
# ----------------------------------------------------------
W = 10  # Window size for moving average
smoothed = np.zeros(N)

# For each index i, average the values from i-W to i+W (with boundary adjustments)
for i in range(N):
    start = max(0, i - W)
    end = min(N, i + W + 1)  # +1 because slicing is exclusive
    smoothed[i] = np.sum(data_int[start:end]) / (end - start)

# ----------------------------------------------------------
# Step 3: Identify Local Maximums Using 3 Neighbors on Each Side
# ----------------------------------------------------------
# A point is considered a local maximum if it is strictly greater than the 3 data points before and after it.
raw_peak_indices = []
for i in range(3, N - 3):
    if (all(smoothed[i] >= smoothed[j] for j in range(i - 3, i)) and 
        all(smoothed[i] >= smoothed[j] for j in range(i + 1, i + 4))):
        raw_peak_indices.append(i)

# ----------------------------------------------------------
# Step 4: Exclude Peaks That Are Within 10 Data Points of Each Other
# ----------------------------------------------------------
# Group peaks that are too close (within 10 data points) and keep only the highest value in each group.
filtered_peaks = []
if raw_peak_indices:
    current_cluster = [raw_peak_indices[0]]
    for peak in raw_peak_indices[1:]:
        # If the current peak is within 10 data points of the last one in the current cluster, add it.
        if peak - current_cluster[-1] < 10:
            current_cluster.append(peak)
        else:
            # End of current cluster: choose the peak with the highest smoothed value.
            best_peak = max(current_cluster, key=lambda i: smoothed[i])
            filtered_peaks.append(best_peak)
            current_cluster = [peak]
    # Process any remaining cluster.
    if current_cluster:
        best_peak = max(current_cluster, key=lambda i: smoothed[i])
        filtered_peaks.append(best_peak)

# ----------------------------------------------------------
# Step 5: Compute the Average Time Between Peaks
# ----------------------------------------------------------
if len(filtered_peaks) < 2:
    print("Not enough peaks found for spacing calculation.")
else:
    # Compute the differences (in data points) between consecutive filtered peaks
    distances = [filtered_peaks[i] - filtered_peaks[i - 1] for i in range(1, len(filtered_peaks))]
    avg_spacing = sum(distances) / len(distances)
    # Convert the spacing from data points to time (seconds)
    avg_time_between_peaks = avg_spacing / sample_rate
    print("Average time between peaks: {:.2f} seconds".format(avg_time_between_peaks))

# ----------------------------------------------------------
# Step 6: Save Smoothed Data to smooth_data.csv
# ----------------------------------------------------------
with open("smooth_data.csv", "w") as f:
    f.write("Index,Time,SmoothedData\n")
    for i, val in enumerate(smoothed):
        f.write(f"{i},{t[i]:.4f},{val:.2f}\n")

# ----------------------------------------------------------
# Step 7: Graph the Results
# ----------------------------------------------------------
plt.figure(figsize=(12, 8))

# Plot the original noisy data
plt.subplot(2, 1, 1)
plt.plot(t, data_int, 'o-', markersize=2, label="Noisy Signal")
plt.title("Generated Noisy Data at 1.2 Hz")
plt.xlabel("Time (s)")
plt.ylabel("Signal Value")
plt.legend()
plt.grid(True)

# Plot the smoothed data with filtered peaks highlighted
plt.subplot(2, 1, 2)
plt.plot(t, smoothed, 'o-', markersize=2, color="red", label="Smoothed Signal")
if filtered_peaks:
    plt.plot(t[filtered_peaks], smoothed[filtered_peaks], 'go', label="Filtered Peaks")
plt.title("Smoothed Data (Moving Average with W = 10)")
plt.xlabel("Time (s)")
plt.ylabel("Signal Value")
plt.legend()
plt.grid(True)

plt.tight_layout()
plt.show()
