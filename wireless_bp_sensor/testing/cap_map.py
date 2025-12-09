import numpy as np
import matplotlib.pyplot as plt

# Inductor value
L = 2800e-9  # 2800 nH

# Fitted parasitic capacitance from curve_fit
Cp = 5.98266765e-13  # ~0.598 pF

# Generate smooth capacitance sweep for fitted curve
C_vals = np.linspace(1e-12, 5e-12, 400)
f_fit = 1 / (2 * np.pi * np.sqrt(L * (C_vals + Cp)))

# -----------------------------------
# Original measured calibration points
# -----------------------------------
C_actual = np.array([3e-12, 5e-12, 1e-12])
f_actual = np.array([50.57e6, 41.17e6, 74.7e6])

# -----------------------------------
# New experimental points
# -----------------------------------
C_new = np.array([2.5e-12, 4e-12, 1.5e-12])
f_new = np.array([55.1e6, 45.37e6, 66.7e6])

# Model predictions at new points
f_pred_new = 1 / (2 * np.pi * np.sqrt(L * (C_new + Cp)))

# Error magnitude for error bars (MHz)
errors = np.abs((f_new - f_pred_new) / 1e6)

# Percent deviation from model
percent_error = ((f_new - f_pred_new) / f_pred_new) * 100

# -----------------------------------
# Plotting
# -----------------------------------
plt.figure(figsize=(9,6))

# Fitted LC model curve
plt.plot(C_vals * 1e12, f_fit / 1e6, label="Fitted LC Model", linewidth=2, color='goldenrod')

# Original calibration points (orange X markers)
plt.scatter(C_actual * 1e12, f_actual / 1e6,
            color='orange', s=70, marker='x', label="Original Data")

# New experimental points with error bars
plt.errorbar(C_new * 1e12,
             f_new / 1e6,
             yerr=errors,
             fmt='o',
             color='blue',
             ecolor='blue',
             capsize=5,
             markersize=7,
             label="New Experimental Data")

# Annotate percent errors next to each point
for c, f, pe in zip(C_new, f_new, percent_error):
    plt.text(c*1e12 + 0.05, f/1e6 + 0.3, f"{pe:.2f}%", fontsize=10)

plt.xlabel("Capacitance (pF)")
plt.ylabel("Frequency (MHz)")
plt.title("LC Resonance Model vs. Experimental Data\n(Error bars = deviation from model)")
plt.grid(True)
plt.legend()
plt.tight_layout()
plt.savefig("cap_plot.png")
plt.show()

