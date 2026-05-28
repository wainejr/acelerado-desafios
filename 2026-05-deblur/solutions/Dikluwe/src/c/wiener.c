/*
 * Crystalline Lineage
 * @prompt solutions/prompts/06-wiener.md (rev 1: WIENER_0)
 *         solutions/prompts/06a-wiener.md (rev 2: WIENER_A, default)
 *         solutions/prompts/06b-wiener-teorico.md (rev 3 hotfix: WIENER_B)
 *         solutions/prompts/prompt-m6-rev5-wiener-d.md (rev 5: WIENER_D)
 * @module M6
 * @language c
 * @variants WIENER_0, WIENER_A, WIENER_B, WIENER_B_ITER, WIENER_D
 * @updated 2026-05-28
 */

#include <complex.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>

#define N_M6 512
#define NN_M6 (N_M6 * N_M6)

#ifdef WIENER_0
void wiener(const float complex x[NN_M6],
            const float complex h[NN_M6],
            float sigma_n,
            float var_noisy,
            float complex output[NN_M6]) {
    (void)sigma_n;
    (void)var_noisy;
    const float k = 0.04f;
    const float K_MIN = 1e-4f;
    for (size_t i = 0; i < NN_M6; ++i) {
        float complex h_val = h[i];
        float complex h_conj = conjf(h_val);
        float h_abs_sq = crealf(h_val) * crealf(h_val) + cimagf(h_val) * cimagf(h_val);
        float denom = h_abs_sq + k;
        if (denom < K_MIN) denom = K_MIN;
        float complex g = h_conj / denom;
        output[i] = g * x[i];
    }
}
#elif defined(WIENER_B)
/* ---- wiener_b: forma teórica S_n/S_x bin-a-bin (prompt 06b hotfix) ---- */

static float g_fftfreq_tbl[N_M6];
static int   g_fftfreq_init = 0;
#define SCRATCH_CAP_C 32768
static float g_scratch[SCRATCH_CAP_C];

/* Sentinela de saúde do buffer (hotfix Bug 1, Camada 3).
 * Zerada no início de cada chamada de wiener(). Permite ao chamador
 * detectar NaN/Inf/zero entrando no buffer de mediana. Em fixture
 * natural, qualquer non_finite_count > 0 indica bug upstream. */
unsigned int wiener_b_non_finite_count = 0;
unsigned int wiener_b_zero_count = 0;
unsigned int wiener_b_total_examined = 0;

static void init_fftfreq(void) {
    if (g_fftfreq_init) return;
    float n_f = (float)N_M6;
    for (int k = 0; k < N_M6; k++) {
        int kk = (k < N_M6 / 2) ? k : (k - N_M6);
        g_fftfreq_tbl[k] = (float)kk / n_f;
    }
    g_fftfreq_init = 1;
}

/* Comparator com ordem total para float (hotfix Bug 1, Camada 2). */
static int cmp_f32_total(const void *a, const void *b) {
    float fa = *(const float *)a;
    float fb = *(const float *)b;
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    int a_nan = (fa != fa);
    int b_nan = (fb != fb);
    if (a_nan && !b_nan) return 1;
    if (!a_nan && b_nan) return -1;
    return 0;
}

static float median_f32(float *buf, size_t n) {
    if (n == 0) return 0.0f;
    qsort(buf, n, sizeof(float), cmp_f32_total);
    if (n % 2 == 1) return buf[n / 2];
    return 0.5f * (buf[n / 2 - 1] + buf[n / 2]);
}

static float estimar_c_faixa(const float complex *x, int r_min, int r_max) {
    float r_min_sq = (float)(r_min * r_min);
    float r_max_sq = (float)(r_max * r_max);
    size_t n = 0;
    for (int u = 0; u < N_M6; u++) {
        float u_freq = g_fftfreq_tbl[u];
        float u_bin = (u < N_M6 / 2) ? (float)u : (float)(N_M6 - u);
        for (int v = 0; v < N_M6; v++) {
            float v_bin = (v < N_M6 / 2) ? (float)v : (float)(N_M6 - v);
            float r_sq = u_bin * u_bin + v_bin * v_bin;
            if (r_sq >= r_min_sq && r_sq <= r_max_sq) {
                float v_freq = g_fftfreq_tbl[v];
                float f_sq = u_freq * u_freq + v_freq * v_freq;
                float complex xi = x[u * N_M6 + v];
                float p = crealf(xi) * crealf(xi) + cimagf(xi) * cimagf(xi);
                float val = p * f_sq;
                if (wiener_b_total_examined != 0xFFFFFFFFu)
                    wiener_b_total_examined++;
                if (!isfinite(val)) {
                    if (wiener_b_non_finite_count != 0xFFFFFFFFu)
                        wiener_b_non_finite_count++;
                    continue;
                }
                if (val <= 0.0f) {
                    if (wiener_b_zero_count != 0xFFFFFFFFu)
                        wiener_b_zero_count++;
                    continue;
                }
                if (n < SCRATCH_CAP_C) {
                    g_scratch[n++] = val;
                }
            }
        }
    }
    return median_f32(g_scratch, n);
}

void wiener(const float complex x[NN_M6],
            const float complex h[NN_M6],
            float sigma_n,
            float var_noisy,
            float complex output[NN_M6]) {
    (void)var_noisy;
    init_fftfreq();

    wiener_b_non_finite_count = 0;
    wiener_b_zero_count = 0;
    wiener_b_total_examined = 0;

    (void)estimar_c_faixa(x, 3, 15);
    float c_f2 = estimar_c_faixa(x, 10, 50);
    (void)estimar_c_faixa(x, 30, 100);

    if (c_f2 <= 0.0f) {
        for (size_t i = 0; i < NN_M6; ++i) output[i] = 0.0f + 0.0f * I;
        return;
    }

    float nn = (float)NN_M6;
    float s_n = sigma_n * sigma_n * nn;
    float eps = 1.0f / nn;
    const float K_MIN = 1e-4f;

    for (int u = 0; u < N_M6; u++) {
        float u_freq = g_fftfreq_tbl[u];
        for (int v = 0; v < N_M6; v++) {
            float v_freq = g_fftfreq_tbl[v];
            float f_sq = u_freq * u_freq + v_freq * v_freq;
            float s_x = c_f2 / (f_sq + eps);
            float ratio = s_n / s_x;
            int idx = u * N_M6 + v;
            float complex h_val = h[idx];
            float h_re = crealf(h_val);
            float h_im = cimagf(h_val);
            float h_abs_sq = h_re * h_re + h_im * h_im;
            float denom = h_abs_sq + ratio;
            if (denom < K_MIN) denom = K_MIN;
            float g_re = h_re / denom;
            float g_im = -h_im / denom;
            float complex xi = x[idx];
            float xi_re = crealf(xi);
            float xi_im = cimagf(xi);
            output[idx] = (g_re * xi_re - g_im * xi_im)
                        + (g_re * xi_im + g_im * xi_re) * I;
        }
    }
}

void wiener_b_iter(
    const float complex x[NN_M6],
    const float complex h[NN_M6],
    float sigma_n,
    int n_iter,
    float complex output[NN_M6]
) {
    init_fftfreq();

    wiener_b_non_finite_count = 0;
    wiener_b_zero_count = 0;
    wiener_b_total_examined = 0;

    float c_f2 = estimar_c_faixa(x, 10, 50);

    if (c_f2 <= 0.0f || !isfinite(c_f2)) {
        for (size_t i = 0; i < NN_M6; ++i) output[i] = 0.0f + 0.0f * I;
        return;
    }

    float nn = (float)NN_M6;
    float s_n = sigma_n * sigma_n * nn;
    float eps = 1.0f / nn;
    const float K_MIN = 1e-4f;

    for (int u = 0; u < N_M6; u++) {
        float u_freq = g_fftfreq_tbl[u];
        for (int v = 0; v < N_M6; v++) {
            float v_freq = g_fftfreq_tbl[v];
            float f_sq = u_freq * u_freq + v_freq * v_freq;
            float s_x = c_f2 / (f_sq + eps);
            float ratio = s_n / s_x;
            int idx = u * N_M6 + v;
            float complex h_val = h[idx];
            float h_re = crealf(h_val);
            float h_im = cimagf(h_val);
            float h_abs_sq = h_re * h_re + h_im * h_im;
            float denom = h_abs_sq + ratio;
            if (denom < K_MIN) denom = K_MIN;
            float g_re = h_re / denom;
            float g_im = -h_im / denom;
            float complex xi = x[idx];
            float xi_re = crealf(xi);
            float xi_im = cimagf(xi);
            output[idx] = (g_re * xi_re - g_im * xi_im)
                        + (g_re * xi_im + g_im * xi_re) * I;
        }
    }

    for (int k = 1; k < n_iter; k++) {
        for (size_t idx = 0; idx < NN_M6; idx++) {
            float complex out_val = output[idx];
            float out_re = crealf(out_val);
            float out_im = cimagf(out_val);
            float out_abs_sq = out_re * out_re + out_im * out_im;
            float s_x = (out_abs_sq > 1.0f) ? out_abs_sq : 1.0f;
            float ratio = s_n / s_x;
            float complex h_val = h[idx];
            float h_re = crealf(h_val);
            float h_im = cimagf(h_val);
            float h_abs_sq = h_re * h_re + h_im * h_im;
            float denom = h_abs_sq + ratio;
            if (denom < K_MIN) denom = K_MIN;
            float g_re = h_re / denom;
            float g_im = -h_im / denom;
            float complex xi = x[idx];
            float xi_re = crealf(xi);
            float xi_im = cimagf(xi);
            output[idx] = (g_re * xi_re - g_im * xi_im)
                        + (g_re * xi_im + g_im * xi_re) * I;
        }
    }
}

/* ---- wiener_d: híbrido suave via média radial (prompt-m6-rev5-wiener-d.md) ----
 *
 * S_x = max(media_radial(|X|²) − S_n , C/f²+ε)  com floor 1e-3.
 * Preserva harmônicos espectrais sem regressão em imagens lisas.
 * Buffers estáticos adicionais: 2 × 1 MiB (power_d, radial_d).
 */
#define RADIAL_D_MAX_C 363

static float power_d_c[NN_M6];
static float radial_d_c[NN_M6];

void wiener_d(
    const float complex x[NN_M6],
    const float complex h[NN_M6],
    float sigma_n,
    float complex output[NN_M6]
) {
    init_fftfreq();

    wiener_b_non_finite_count = 0;
    wiener_b_zero_count = 0;
    wiener_b_total_examined = 0;

    float c_f2 = estimar_c_faixa(x, 10, 50);

    if (c_f2 <= 0.0f || !isfinite(c_f2)) {
        for (size_t i = 0; i < NN_M6; ++i) output[i] = 0.0f + 0.0f * I;
        return;
    }

    float nn = (float)NN_M6;
    float s_n = sigma_n * sigma_n * nn;
    float eps = 1.0f / nn;
    const float K_MIN = 1e-4f;
    const float SX_FLOOR = 1e-3f;

    /* Passo 1: |X|² para todos os bins */
    for (int i = 0; i < NN_M6; i++) {
        float re = crealf(x[i]);
        float im = cimagf(x[i]);
        power_d_c[i] = re * re + im * im;
    }

    /* Passo 2: acumular soma e contagem por anel radial (stack, ~3 KiB) */
    double soma[RADIAL_D_MAX_C];
    unsigned int contagem[RADIAL_D_MAX_C];
    for (int r = 0; r < RADIAL_D_MAX_C; r++) { soma[r] = 0.0; contagem[r] = 0; }

    for (int u = 0; u < N_M6; u++) {
        float u_bin = (u < N_M6 / 2) ? (float)u : (float)(N_M6 - u);
        for (int v = 0; v < N_M6; v++) {
            float v_bin = (v < N_M6 / 2) ? (float)v : (float)(N_M6 - v);
            int r = (int)(sqrtf(u_bin * u_bin + v_bin * v_bin) + 0.5f);
            if (r >= RADIAL_D_MAX_C) r = RADIAL_D_MAX_C - 1;
            soma[r] += (double)power_d_c[u * N_M6 + v];
            contagem[r]++;
        }
    }

    /* Passo 3: preencher radial_d_c com media_radial */
    for (int u = 0; u < N_M6; u++) {
        float u_bin = (u < N_M6 / 2) ? (float)u : (float)(N_M6 - u);
        for (int v = 0; v < N_M6; v++) {
            float v_bin = (v < N_M6 / 2) ? (float)v : (float)(N_M6 - v);
            int r = (int)(sqrtf(u_bin * u_bin + v_bin * v_bin) + 0.5f);
            if (r >= RADIAL_D_MAX_C) r = RADIAL_D_MAX_C - 1;
            unsigned int cnt = contagem[r] > 0 ? contagem[r] : 1;
            radial_d_c[u * N_M6 + v] = (float)(soma[r] / (double)cnt);
        }
    }

    /* Passo 4: aplicar filtro wiener_d bin-a-bin */
    for (int u = 0; u < N_M6; u++) {
        float u_freq = g_fftfreq_tbl[u];
        for (int v = 0; v < N_M6; v++) {
            float v_freq = g_fftfreq_tbl[v];
            float f_sq = u_freq * u_freq + v_freq * v_freq;
            int idx = u * N_M6 + v;

            float s_x_model = c_f2 / (f_sq + eps);
            float s_x_obs   = radial_d_c[idx] - s_n;
            if (s_x_obs < 0.0f) s_x_obs = 0.0f;

            float s_x = s_x_obs > s_x_model ? s_x_obs : s_x_model;
            if (s_x < SX_FLOOR) s_x = SX_FLOOR;

            float ratio = s_n / s_x;
            float complex h_val = h[idx];
            float h_re = crealf(h_val);
            float h_im = cimagf(h_val);
            float h_abs_sq = h_re * h_re + h_im * h_im;
            float denom = h_abs_sq + ratio;
            if (denom < K_MIN) denom = K_MIN;
            float g_re = h_re / denom;
            float g_im = -h_im / denom;
            float complex xi = x[idx];
            float xi_re = crealf(xi);
            float xi_im = cimagf(xi);
            output[idx] = (g_re * xi_re - g_im * xi_im)
                        + (g_re * xi_im + g_im * xi_re) * I;
        }
    }
}

#else  /* Default: WIENER_A (Dinâmico) */
void wiener(const float complex x[NN_M6],
            const float complex h[NN_M6],
            float sigma_n,
            float var_noisy,
            float complex output[NN_M6]) {
    const float alpha = 2.0f;
    float sigma_n_sq = sigma_n * sigma_n;
    float diff = var_noisy - sigma_n_sq;
    float var_signal = (diff > 1.0f) ? diff : 1.0f;
    float k = alpha * (sigma_n_sq / var_signal);
    const float K_MIN = 1e-4f;
    if (k < K_MIN) k = K_MIN;
    for (size_t i = 0; i < NN_M6; ++i) {
        float complex h_val = h[i];
        float complex h_conj = conjf(h_val);
        float h_abs_sq = crealf(h_val) * crealf(h_val) + cimagf(h_val) * cimagf(h_val);
        float denom = h_abs_sq + k;
        if (denom < K_MIN) denom = K_MIN;
        float complex g = h_conj / denom;
        output[i] = g * x[i];
    }
}
#endif
