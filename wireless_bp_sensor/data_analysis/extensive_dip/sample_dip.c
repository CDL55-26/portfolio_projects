// dip_detect.c
// Drop-in utility for "group-then-average" dip detection with per-frequency outlier rejection.

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <string.h>

// ---- Public configuration knobs -------------------------------------------------

typedef struct {
    // Reject points farther than k_sigma * stddev from the group mean (1st pass),
    // then recompute the mean of the kept points.
    float   k_sigma;           // e.g., 2.5f
    uint32_t min_group_for_reject; // minimum group size to even attempt outlier rejection (e.g., 6)

    // Optional smoothing over the compressed series (set to 0 to disable).
    // Window is odd; we’ll clamp and mirror at edges.
    uint32_t smooth_window;    // e.g., 5 (must be odd); 0 disables smoothing

    // If frequencies can differ by a tick/rounding but should be grouped,
    // set a tolerance in Hz (0 -> exact equality required).
    uint32_t freq_tolerance_hz; // e.g., 0 for exact match
} DipParams;

// ---- Core API -------------------------------------------------------------------

/**
 * Compress parallel arrays (samples[], freqs[]) into per-frequency averages with outlier rejection.
 *
 * Inputs:
 *   samples[]:  raw ADC samples (e.g., log-detector magnitude); length N
 *   freqs[]:    frequency in Hz for samples[i]; length N
 *   N:          number of samples
 *   params:     behavior knobs (see DipParams)
 *
 * Outputs:
 *   out_freq[]: unique frequency for each group (Hz)
 *   out_val[]:  averaged value for that frequency (float)
 *
 * Returns: number of groups written to out arrays (<= N)
 */
size_t compress_by_frequency(const uint16_t *samples,
                             const uint32_t *freqs,
                             size_t N,
                             const DipParams *params,
                             uint32_t *out_freq,
                             float *out_val)
{
    if (!samples || !freqs || !out_freq || !out_val || !params || N == 0) return 0;

    size_t write_idx = 0;
    size_t i = 0;

    while (i < N) {
        // Start a new group at i
        uint32_t f0 = freqs[i];
        size_t j = i + 1;

        // Extend group while frequency matches within tolerance
        while (j < N) {
            uint32_t df = (freqs[j] > f0) ? (freqs[j] - f0) : (f0 - freqs[j]);
            if (df <= params->freq_tolerance_hz) {
                j++;
            } else {
                break;
            }
        }

        size_t group_len = j - i;

        // First pass: mean and stddev
        double sum = 0.0, sum2 = 0.0;
        for (size_t k = i; k < j; k++) {
            double v = (double)samples[k];
            sum  += v;
            sum2 += v * v;
        }
        double mean = sum / (double)group_len;
        double var  = fmax(0.0, (sum2 / (double)group_len) - mean * mean);
        double std  = sqrt(var);

        // Second pass: reject outliers if group large enough
        double sum_keep = 0.0;
        size_t keep_cnt = 0;

        if (group_len >= params->min_group_for_reject && std > 1e-9 && params->k_sigma > 0.0f) {
            double thr = params->k_sigma * std;
            for (size_t k = i; k < j; k++) {
                double v = (double)samples[k];
                if (fabs(v - mean) <= thr) {
                    sum_keep += v;
                    keep_cnt++;
                }
            }
            if (keep_cnt == 0) { // fallback if everything got rejected
                sum_keep = sum;
                keep_cnt = group_len;
            }
        } else {
            sum_keep = sum;
            keep_cnt = group_len;
        }

        float avg = (float)(sum_keep / (double)keep_cnt);

        out_freq[write_idx] = f0; // representative frequency for the group
        out_val[write_idx]  = avg;
        write_idx++;

        i = j; // move to next group
    }

    return write_idx;
}

/**
 * Simple centered moving-average smoothing over y[], window must be odd.
 * Mirrors edges to keep same length.
 */
void smooth_moving_average(const float *in, float *out, size_t M, uint32_t window)
{
    if (!in || !out || M == 0 || window < 2 || (window % 2) == 0) {
        if (in && out && M) memcpy(out, in, M * sizeof(float));
        return;
    }

    int half = (int)window / 2;
    for (size_t idx = 0; idx < M; idx++) {
        double acc = 0.0;
        int count = 0;
        for (int w = -half; w <= half; w++) {
            int k = (int)idx + w;
            // mirror at edges
            if (k < 0) k = -k;
            if (k >= (int)M) k = (int)(2*M - 2 - k);
            acc += in[k];
            count++;
        }
        out[idx] = (float)(acc / (double)count);
    }
}

/**
 * Find minimum value and index in an array.
 * Returns index of min; writes the min value to *min_val if non-NULL.
 */
size_t argminf(const float *a, size_t M, float *min_val)
{
    size_t idx = 0;
    float mv = a[0];
    for (size_t i = 1; i < M; i++) {
        if (a[i] < mv) {
            mv = a[i];
            idx = i;
        }
    }
    if (min_val) *min_val = mv;
    return idx;
}

/**
 * Top-level helper to:
 *  - compress groups
 *  - optional smoothing
 *  - return dip frequency and value
 *
 * Returns true on success; false if inputs invalid or no groups.
 */
bool detect_dip(const uint16_t *samples,
                const uint32_t *freqs,
                size_t N,
                const DipParams *params,
                uint32_t *dip_freq_hz,
                float *dip_value,
                // Optional outputs to reuse later (can be NULL if you don't need them)
                uint32_t *scratch_freq,   // length >= N
                float *scratch_val,       // length >= N
                float *scratch_val_sm)    // length >= N (only used if smoothing enabled)
{
    if (!samples || !freqs || !params || !dip_freq_hz || !dip_value || N == 0) return false;

    // Allocate scratch if not provided
    uint32_t *out_f = scratch_freq  ? scratch_freq  : (uint32_t*)malloc(N * sizeof(uint32_t));
    float    *out_v = scratch_val   ? scratch_val   : (float*)   malloc(N * sizeof(float));
    float    *out_s = scratch_val_sm? scratch_val_sm: (params->smooth_window ? (float*)malloc(N*sizeof(float)) : NULL);

    if (!out_f || !out_v || (params->smooth_window && !out_s)) {
        if (!scratch_freq && out_f) free(out_f);
        if (!scratch_val  && out_v) free(out_v);
        if (!scratch_val_sm && out_s) free(out_s);
        return false;
    }

    size_t M = compress_by_frequency(samples, freqs, N, params, out_f, out_v);
    if (M == 0) {
        if (!scratch_freq) free(out_f);
        if (!scratch_val)  free(out_v);
        if (!scratch_val_sm && out_s) free(out_s);
        return false;
    }

    const float *series = out_v;

    if (params->smooth_window >= 3 && (params->smooth_window % 2) == 1) {
        smooth_moving_average(out_v, out_s, M, params->smooth_window);
        series = out_s;
    }

    float vmin;
    size_t idx = argminf(series, M, &vmin);

    *dip_freq_hz = out_f[idx];
    *dip_value   = vmin;

    if (!scratch_freq) free(out_f);
    if (!scratch_val)  free(out_v);
    if (!scratch_val_sm && out_s) free(out_s);

    return true;
}

// ---- Tiny demo harness ----------------------------------------------------------
// Remove main() when integrating into your firmware build.

int main(void)
{
    // Synthetic example: three frequency groups, each with noise; middle has a deeper dip.
    enum { N = 90 };
    uint16_t samples[N];
    uint32_t freqs[N];

    // 30 samples @ 1 MHz, 30 @ 1.1 MHz, 30 @ 1.2 MHz
    for (int i = 0; i < 30; i++) { freqs[i] = 1000000;     samples[i] = 600 + (rand()%21 - 10); }
    for (int i = 30; i < 60; i++){ freqs[i] = 1100000;     samples[i] = 500 + (rand()%21 - 10); } // <- dip
    for (int i = 60; i < 90; i++){ freqs[i] = 1200000;     samples[i] = 580 + (rand()%21 - 10); }

    DipParams p = {
        .k_sigma = 2.5f,
        .min_group_for_reject = 6,
        .smooth_window = 5,          // try 0 to disable smoothing
        .freq_tolerance_hz = 0
    };

    uint32_t dip_f;
    float dip_v;
    bool ok = detect_dip(samples, freqs, N, &p, &dip_f, &dip_v, NULL, NULL, NULL);
    if (ok) {
        printf("Detected dip: %u Hz, value=%.2f\n", dip_f, dip_v);
    } else {
        printf("Dip detection failed.\n");
    }
    return ok ? 0 : 1;
}

