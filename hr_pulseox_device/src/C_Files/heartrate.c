#include <stdio.h>
#define N 1024                   // Total number of data entries
#define W 10                     // Window size for the moving average
#define TIME_INTERVAL 0.01       // Time interval (in seconds) between data points

int main(void) {
    int arr[N], rawPeaks[N], filteredPeaks[N];
    int cnt = 0, rawPCount = 0, fcount = 0, i, j, k, total = 0;
    double s[N], sum;

    // Read 1024 integers from raw_data.csv (no header)
    FILE *fp = fopen("raw_data.csv", "r");
    if (!fp)
        return 1;
    while (cnt < N && fscanf(fp, "%d,", &arr[cnt]) == 1)
        cnt++;
    fclose(fp);

    // Moving average smoothing: average values from i-W to i+W
    for (i = 0; i < cnt; i++) {
        int n = 0;
        sum = 0;
        for (j = i - W; j <= i + W; j++) {
            if (j >= 0 && j < cnt) {
                sum += arr[j];
                n++;
            }
        }
        s[i] = sum / n;
    }

    // Detect peaks: a point is a peak if it is ≥ each of its 3 preceding and 3 following neighbors
    for (i = 3; i < cnt - 3; i++) {
        int flag = 1;
        for (j = i - 3; j < i; j++) {
            if (s[i] < s[j]) { flag = 0; break; }
        }
        if (flag) {
            for (j = i + 1; j <= i + 3; j++) {
                if (s[i] < s[j]) { flag = 0; break; }
            }
        }
        if (flag)
            rawPeaks[rawPCount++] = i;
    }

    // Filter peaks: group peaks closer than 10 data points and keep the one with the highest value
    i = 0;
    while (i < rawPCount) {
        int best = rawPeaks[i];
        k = i + 1;
        while (k < rawPCount && rawPeaks[k] - rawPeaks[i] < 10) {
            if (s[rawPeaks[k]] > s[best])
                best = rawPeaks[k];
            k++;
        }
        filteredPeaks[fcount++] = best;
        i = k;
    }

    if (fcount < 2)
        return 1;

    // Sum spacing between consecutive filtered peaks and compute average time between peaks
    for (i = 1; i < fcount; i++)
        total += (filteredPeaks[i] - filteredPeaks[i - 1]);
    double avg_spacing = (double)total / (fcount - 1);
    double avg_time_between_peaks = avg_spacing * TIME_INTERVAL;
    printf("Average time between peaks: %.2f seconds\n", avg_time_between_peaks);

    return 0;
}
