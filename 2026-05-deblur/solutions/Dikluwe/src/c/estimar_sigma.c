/*
 * Crystalline Lineage
 * @prompt prompts/03-estimar-sigma.md (rev 2: M3_REV2)
 *         prompts/03-estimar-sigma-rev3.md (rev 3: M3_REV3, default)
 * @module M3
 * @language c
 * @updated 2026-05-15
 *
 * Coexistência de revisões via ADR 019. Selecionar com:
 *   make M3=0  → -DM3_REV2 (α=2.0 fixo, ADR 011)
 *   make       → default M3_REV3 (α adaptativo; calibrado em
 *                tests/results/m3_rev3_calibration_report.md)
 */

#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define N_TOTAL (512 * 512)
#define N_FREQ 256

/* --- Compartilhado entre rev 2 e rev 3 --- */
static void binning_radial(const float power_spectrum[N_TOTAL],
                           float sum_power[N_FREQ], uint32_t count[N_FREQ]) {
    for (int i = 0; i < N_FREQ; i++) { sum_power[i] = 0.0f; count[i] = 0; }
    for (int v = 0; v < 512; v++) {
        int dv = v <= 256 ? v : 512 - v;
        float dv2 = (float)(dv * dv);
        int row_offset = v * 512;
        for (int u = 0; u < 512; u++) {
            int du = u <= 256 ? u : 512 - u;
            float du2 = (float)(du * du);
            float r = sqrtf(du2 + dv2);
            int r_bin = (int)roundf(r);
            if (r_bin >= 0 && r_bin < N_FREQ) {
                sum_power[r_bin] += power_spectrum[row_offset + u];
                count[r_bin]++;
            }
        }
    }
}

static float noise_floor_calc(const float sum_power[N_FREQ], const uint32_t count[N_FREQ]) {
    float s = 0.0f;
    uint32_t n = 0;
    for (int i = 200; i < N_FREQ; i++) {
        if (count[i] > 0) { s += sum_power[i]; n += count[i]; }
    }
    return n > 0 ? s / (float)n : 0.0f;
}

static int janela_adaptativa(const float sum_power[N_FREQ], const uint32_t count[N_FREQ], float nf) {
    int r_max = 77;
    for (int r = 5; r < 78; r++) {
        if (count[r] > 0) {
            float p_media = sum_power[r] / (float)count[r];
            if ((p_media - nf) < 3.0f * nf) { r_max = r; break; }
        }
    }
    return r_max < 15 ? 15 : r_max;
}

static float regressao_blur(const float sum_power[N_FREQ], const uint32_t count[N_FREQ],
                            float nf, int r_min, int r_max, float alpha) {
    float sum_x = 0.0f, sum_y = 0.0f, sum_xy = 0.0f, sum_x2 = 0.0f;
    uint32_t n = 0;
    for (int r = r_min; r <= r_max; r++) {
        if (count[r] > 0) {
            float p_media = sum_power[r] / (float)count[r];
            float val_sub = p_media - nf;
            float val_clamp = val_sub > 1e-9f ? val_sub : 1e-9f;
            float f_r = (float)r / 512.0f;
            float y = logf(val_clamp) + alpha * logf(f_r);
            float x = f_r * f_r;
            sum_x += x; sum_y += y; sum_xy += x * y; sum_x2 += x * x;
            n++;
        }
    }
    if (n < 3) return 0.0f;
    float n_f = (float)n;
    float denom = n_f * sum_x2 - sum_x * sum_x;
    if (fabsf(denom) < 1e-9f) return 0.0f;
    float a = (n_f * sum_xy - sum_x * sum_y) / denom;
    float a_pos = -a > 0.0f ? -a : 0.0f;
    return sqrtf(a_pos / (4.0f * (float)(M_PI * M_PI)));
}

static float finalizar_sigma(float sigma) {
    if (sigma < 0.45f) return 0.0f;
    if (sigma < 0.0f) sigma = 0.0f;
    if (sigma > 3.5f) sigma = 3.5f;
    return sigma;
}

#ifdef M3_REV2
/* === Rev 2: α = 2.0 fixo (ADR 011) === */
float estimar_sigma(const float power_spectrum[N_TOTAL]) {
    float sum_power[N_FREQ];
    uint32_t count[N_FREQ];
    binning_radial(power_spectrum, sum_power, count);
    float nf = noise_floor_calc(sum_power, count);
    int r_max = janela_adaptativa(sum_power, count, nf);
    float sigma = regressao_blur(sum_power, count, nf, 3, r_max, 2.0f);
    return finalizar_sigma(sigma);
}
#else
/* === Rev 3 (default): α adaptativo via fitting conjunto, com fallback ===
 * Origem: prompts/03-estimar-sigma-rev3.md.
 * Calibração: tests/results/m3_rev3_calibration_report.md.
 */

/* Acumuladores em double (sistema 3×3 sensível a condicionamento). */
static int ajuste_conjunto(const float sum_power[N_FREQ], const uint32_t count[N_FREQ],
                           float nf, int r_max, float *out_beta1, float *out_beta2) {
    double n = 0.0, sx1 = 0.0, sx2 = 0.0, sy = 0.0;
    double sx1x1 = 0.0, sx2x2 = 0.0, sx1x2 = 0.0;
    double sx1y = 0.0, sx2y = 0.0;
    for (int r = 3; r <= r_max; r++) {
        if (count[r] > 0) {
            float p_media = sum_power[r] / (float)count[r];
            float val_sub = p_media - nf;
            float val_clamp = val_sub > 1e-9f ? val_sub : 1e-9f;
            float f_r = (float)r / 512.0f;
            double x1 = (double)(f_r * f_r);
            double x2 = (double)logf(f_r);
            double y = (double)logf(val_clamp);
            n += 1.0;
            sx1 += x1; sx2 += x2; sy += y;
            sx1x1 += x1 * x1; sx2x2 += x2 * x2; sx1x2 += x1 * x2;
            sx1y += x1 * y; sx2y += x2 * y;
        }
    }
    if (n < 4.0) return 0;
    /* Cramer 3×3 sobre [n,sx1,sx2; sx1,sx1x1,sx1x2; sx2,sx1x2,sx2x2] · [c,β1,β2]ᵀ = [sy,sx1y,sx2y]ᵀ */
    double m11=n,    m12=sx1,   m13=sx2;
    double m21=sx1,  m22=sx1x1, m23=sx1x2;
    double m31=sx2,  m32=sx1x2, m33=sx2x2;
    double det_a =
          m11 * (m22 * m33 - m23 * m32)
        - m12 * (m21 * m33 - m23 * m31)
        + m13 * (m21 * m32 - m22 * m31);
    if (fabs(det_a) < 1e-9) return 0;
    double det_b1 =
          m11 * (sx1y * m33 - m23 * sx2y)
        - sy  * (m21 * m33 - m23 * m31)
        + m13 * (m21 * sx2y - sx1y * m31);
    double det_b2 =
          m11 * (m22 * sx2y - sx1y * m32)
        - m12 * (m21 * sx2y - sx1y * m31)
        + sy  * (m21 * m32 - m22 * m31);
    *out_beta1 = (float)(det_b1 / det_a);
    *out_beta2 = (float)(det_b2 / det_a);
    return 1;
}

float estimar_sigma(const float power_spectrum[N_TOTAL]) {
    float sum_power[N_FREQ];
    uint32_t count[N_FREQ];
    binning_radial(power_spectrum, sum_power, count);
    float nf = noise_floor_calc(sum_power, count);
    int r_max = janela_adaptativa(sum_power, count, nf);
    float beta1, beta2;
    float sigma;
    if (ajuste_conjunto(sum_power, count, nf, r_max, &beta1, &beta2)) {
        float alpha = -beta2;
        if (alpha >= 2.05f && alpha <= 3.0f) {
            float a_pos = -beta1 > 0.0f ? -beta1 : 0.0f;
            sigma = sqrtf(a_pos / (4.0f * (float)(M_PI * M_PI)));
        } else {
            sigma = regressao_blur(sum_power, count, nf, 3, r_max, 2.0f);
        }
    } else {
        sigma = regressao_blur(sum_power, count, nf, 3, r_max, 2.0f);
    }
    return finalizar_sigma(sigma);
}
#endif
