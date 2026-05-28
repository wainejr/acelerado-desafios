/*
 * Crystalline Lineage
 * @prompt prompts/prompt-m9-var-sp.md (revisão 1)
 * @module M9
 * @language c
 * @updated 2026-05-27
 */

float var_sp(const float x[512 * 512]) {
    const double N_TOTAL = 512.0 * 512.0;
    
    double sum = 0.0;
    double sum_sq = 0.0;
    
    for (int i = 0; i < 512 * 512; i++) {
        double val = (double)x[i];
        sum += val;
        sum_sq += val * val;
    }
    
    double mean = sum / N_TOTAL;
    double var = sum_sq / N_TOTAL - mean * mean;
    if (var < 0.0) var = 0.0;
    
    return (float)(var / 65025.0);
}
