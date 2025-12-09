import ctypes
import numpy as np
import matplotlib.pyplot as plt
import os

# --- 1. Load the Shared Library ---
# Use .so for Linux/macOS, .dll for Windows
lib_path = './dip_detector.so' 
if os.name == 'nt': # 'nt' is the name for Windows
    lib_path = './dip_detector.dll'

# Load the compiled C library
try:
    c_lib = ctypes.CDLL(lib_path)
except OSError as e:
    print(f"Error loading shared library: {e}")
    print("Did you compile the C code first?")
    exit()

# --- 2. Define the C Function "Signature" ---
# This tells Python how to call the C function correctly.
# This MUST match the C function's prototype.
c_lib.detect_dip.restype = ctypes.c_float
c_lib.detect_dip.argtypes = [
    np.ctypeslib.ndpointer(dtype=np.uint16, flags="C_CONTIGUOUS"), # const uint16_t *samples
    np.ctypeslib.ndpointer(dtype=np.uint32, flags="C_CONTIGUOUS"), # const uint32_t *freqs
    ctypes.c_uint32,                                               # uint32_t buffer_size                                       
    np.ctypeslib.ndpointer(dtype=np.uint32, flags="C_CONTIGUOUS"), # uint32_t *out_freq
    np.ctypeslib.ndpointer(dtype=np.float32, flags="C_CONTIGUOUS") # float *out_val
]

# --- 3. Test Case Generation Function (Now in Python!) ---

def run_test_case(test_name, num_groups, samples_per_group, dip_index):
    """Generates data, calls C function, and plots the result."""
    print(f"--- Running Test: {test_name} ---")

    # --- Generate Test Data (much easier in NumPy) ---
    
    # Create the base frequencies (e.g., 100M, 110M, 120M...)
    base_freqs_hz = np.arange(
        100e6, 
        100e6 + num_groups * 10e6, 
        10e6, 
        dtype=np.uint32
    )
    
    # "Repeat" each frequency to create the full input array
    test_freqs = np.repeat(base_freqs_hz, samples_per_group).astype(np.uint32)
    
    # Create sample data with a base average and a dip average
    base_avg = 5000
    dip_avg = 1000
    noise_level = 300
    
    # Start with all samples at the base average
    test_samples = np.full(test_freqs.shape, base_avg, dtype=np.uint16)
    
    # Find the dip frequency and set all its samples to the dip average
    expected_dip_freq = base_freqs_hz[dip_index]
    dip_mask = (test_freqs == expected_dip_freq)
    test_samples[dip_mask] = dip_avg
    
    # Add random noise
    noise = np.random.randint(
        -noise_level, 
        noise_level, 
        size=test_freqs.shape, 
        dtype=np.int32
    )
    test_samples = (test_samples + noise).astype(np.uint16)
    
    N = len(test_samples)

    # --- 4. Allocate Output Buffers ---
    # These are the arrays the C function will write its results into
    out_freq_buf = np.zeros(num_groups, dtype=np.uint32)
    out_val_buf = np.zeros(num_groups, dtype=np.float32)

    # --- 5. Call the C Function ---
    print(f"Calling C function with {N} total samples...")
    returned_freq_float = c_lib.detect_dip(
        test_samples, 
        test_freqs, 
        N, 
        out_freq_buf, 
        out_val_buf
    )
    
    # --- 6. Verify the Results ---
    print(f"C function returned: {returned_freq_float / 1e6:.3f} MHz")
    print(f"Expected dip freq:   {expected_dip_freq / 1e6:.3f} MHz")

    if np.isclose(returned_freq_float, expected_dip_freq):
        print("[PASS] Dip detected at correct frequency.")
    else:
        print("[FAIL] Dip detected at WRONG frequency.")
        
    # --- 7. Visualize the Data ---
    plt.figure(figsize=(12, 7))
    plt.title(test_name)
    
    # Plot the noisy "raw" samples
    plt.scatter(
        test_freqs / 1e6, 
        test_samples, 
        alpha=0.1, 
        label="Raw Samples (with noise)"
    )
    
    # Plot the averaged data returned from the C function
    plt.plot(
        out_freq_buf / 1e6, 
        out_val_buf, 
        'r-o', 
        linewidth=2, 
        label="Averaged Data (from C)"
    )
    
    # Draw a line where the dip was detected
    plt.axvline(
        returned_freq_float / 1e6, 
        color='red', 
        linestyle='--', 
        label=f"Detected Dip ({returned_freq_float / 1e6:.1f} MHz)"
    )
    
    plt.xlabel("Frequency (MHz)")
    plt.ylabel("Sample Value")
    plt.legend()
    plt.grid(True)
    plt.show()


# --- Run our tests ---
if __name__ == "__main__":
    run_test_case(
        test_name="Test 1: Dip in middle",
        num_groups=10,
        samples_per_group=50,
        dip_index=4  # Dip at the 5th frequency
    )
    
    run_test_case(
        test_name="Test 2: Dip at start",
        num_groups=8,
        samples_per_group=20,
        dip_index=0  # Dip at the 1st frequency
    )
    run_test_case(
        test_name="Test 3: Dip in middle",
        num_groups=16,
        samples_per_group=100,
        dip_index=8  # Dip at the 1st frequency
    )