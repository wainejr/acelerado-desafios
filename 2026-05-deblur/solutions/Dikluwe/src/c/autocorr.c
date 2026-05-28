/*
 * Crystalline Lineage
 * @prompt prompts/prompt-m8-autocorr.md (revisão 1)
 * @module M8
 * @language c
 * @updated 2026-05-27
 */

#include <math.h>

float autocorr_5(const float x[512 * 512]) {
    const int N = 512;
    const int SHIFT = 5;
    const double TOTAL = (double)(N * N);
    
    double sum_x = 0.0, sum_xs = 0.0;
    double sum_xx = 0.0, sum_ss = 0.0;
    double sum_xs_prod = 0.0;
    
    for (int i = 0; i < N; i++) {
        int i_shifted = (i + SHIFT) % N;
        for (int j = 0; j < N; j++) {
            double val = (double)x[i * N + j];
            double val_s = (double)x[i_shifted * N + j];
            sum_x += val;
            sum_xs += val_s;
            sum_xx += val * val;
            sum_ss += val_s * val_s;
            sum_xs_prod += val * val_s;
        }
    }
    
    double mean_x = sum_x / TOTAL;
    double mean_xs = sum_xs / TOTAL;
    double var_x = sum_xx / TOTAL - mean_x * mean_x;
    double var_xs = sum_ss / TOTAL - mean_xs * mean_xs;
    double cov = sum_xs_prod / TOTAL - mean_x * mean_xs;
    
    double denom = sqrt(var_x * var_xs);
    if (denom < 1e-9) {
        return 0.0f;
    }
    return (float)(cov / denom);
}
