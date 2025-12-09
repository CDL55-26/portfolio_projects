import matplotlib.pyplot as plt
import numpy as np
from sklearn.linear_model import LinearRegression
from sklearn.metrics import r2_score

# -----------------------------
# Data
# -----------------------------
expected = np.array([49.5, 41, 74.3]).reshape(-1, 1)

data = [
    [50000,50500,50000,50500,51000,50500,50500,50500,51000,50500,50500,51000,50500,51000],
    [41000,41500,41000,41500,41500,41500,41000,41000,41500,41500,41000,41000,41000,41000,41500,
     41000,41000,41000,41000,41000,41500,41000,41000],
    [74000,74500,75000,75000,75000,74000,75000,74500,75500,74500,74500,75000,73000,75000,75500,
     75000,74500,75500,74500,74500,75000,74500,75000]
]

# Convert kHz → MHz
actual_means = np.array([np.mean(d)/1000 for d in data]).reshape(-1, 1)
actual_stds = np.array([np.std(d)/1000 for d in data])

# -----------------------------
# Error Metrics (Method A)
# -----------------------------
abs_errors = np.abs(actual_means.flatten() - expected.flatten())
percent_errors = abs_errors / expected.flatten() * 100

rmse = np.sqrt(np.mean((actual_means.flatten() - expected.flatten())**2))
mae = np.mean(abs_errors)
mpe = np.mean(percent_errors)

# -----------------------------
# Fit line of best fit
# -----------------------------
reg = LinearRegression().fit(expected, actual_means)
pred_line = reg.predict(expected)
r2 = r2_score(actual_means, pred_line)

# -----------------------------
# Plot with side-by-side layout
# -----------------------------
fig, axes = plt.subplots(1, 2, figsize=(14, 5))

# LEFT PLOT
ax = axes[0]
ax.errorbar(expected.flatten(), actual_means.flatten(),
            yerr=actual_stds, fmt='o', color='blue', ecolor='blue',
            capsize=5, label='Actual Frequency (MHz)')
ax.plot(expected, expected, 'o', color='red', label='Expected Frequency (MHz)')
ax.plot(expected, pred_line, linestyle=':', color='black', linewidth=2,
        label='Line of Best Fit')

ax.set_xlabel("Expected Frequency (MHz)")
ax.set_ylabel("Frequency (MHz)")
ax.set_title(f"Expected vs Actual Frequency\nR² = {r2:.5f}")
ax.grid(True)
ax.legend()

# RIGHT TABLE
ax2 = axes[1]
ax2.axis('off')

table_data = [
    ["Frequency", "Abs Error (MHz)", "Percent Error (%)"],
    [f"{expected[0][0]:.1f}", f"{abs_errors[0]:.3f}", f"{percent_errors[0]:.2f}%"],
    [f"{expected[1][0]:.1f}", f"{abs_errors[1]:.3f}", f"{percent_errors[1]:.2f}%"],
    [f"{expected[2][0]:.1f}", f"{abs_errors[2]:.3f}", f"{percent_errors[2]:.2f}%"],
    ["---", "---", "---"],
    ["MAE", f"{mae:.3f}", ""],
    ["MPE", "", f"{mpe:.2f}%"],
    ["RMSE", f"{rmse:.3f}", ""]
]

table = ax2.table(cellText=table_data, loc='center', cellLoc='center')
table.auto_set_font_size(False)
table.set_fontsize(11)
table.scale(1.2, 1.6)

plt.tight_layout()
plt.savefig("frequency_analysis.png") 
plt.show()

print(actual_means)
