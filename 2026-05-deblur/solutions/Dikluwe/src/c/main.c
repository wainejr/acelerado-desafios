/*
 * Crystalline Lineage
 * @prompt prompts/prompt-m7-rev5.md (revisão 5)
 * @module M7
 * @language c
 * @updated 2026-05-27
 */

#ifdef PROFILE
#define _POSIX_C_SOURCE 199309L // Para habilitar clock_gettime
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <complex.h>
#include <math.h>

#ifdef PROFILE
#include <time.h>
#endif

// Declarações das funções dos módulos (M1 a M6, M9, M11)
int read_bmp(FILE* in_stream, uint8_t out[512 * 512]);
int write_bmp(const uint8_t buf[512 * 512], FILE* out_stream);
float estimar_sigma(const float power_spectrum[512 * 512]);
float estimar_sigma_n(const float power_spectrum[512 * 512]);
int fft2_forward(float complex buffer[512 * 512]);
int fft2_inverse(float complex buffer[512 * 512]);
void kernel_gaussiano_fft(float sigma, float complex out[512 * 512]);
void wiener(const float complex x[512 * 512], const float complex h[512 * 512], float sigma_n, float var_noisy, float complex output[512 * 512]);
void wiener_b_iter(const float complex x[512 * 512], const float complex h[512 * 512], float sigma_n, int n_iter, float complex output[512 * 512]);
void wiener_d(const float complex x[512 * 512], const float complex h[512 * 512], float sigma_n, float complex output[512 * 512]);
float var_sp(const float x[512 * 512]);
void tv_denoise(
    const float input[512 * 512],
    float lambda,
    float rho,
    int n_iter,
    float output[512 * 512]
);

// Layout de Buffers no escopo de arquivo (.bss ~12.5 MiB total)
static uint8_t input_u8[262144];
static uint8_t output_u8[262144];
static float complex X[262144];
static float power_spectrum[262144];
static float complex H[262144];
static float complex Y[262144];
static float complex spatial[262144];

static float input_f32[262144];
static float warm_start_f32[262144];
static float output_f32[262144];

#ifdef PROFILE
static double diff_ms(struct timespec start, struct timespec end) {
    return (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_nsec - start.tv_nsec) / 1000000.0;
}
#endif

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

#ifdef PROFILE
    struct timespec start, end;
    double t_m1_read, t_m2_fwd, t_m3, t_m5, t_m6, t_m2_inv, t_m8, t_m9, t_m10, t_m1_write;
#endif

    // 1. Leitura BMP de stdin → input_u8
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    if (read_bmp(stdin, input_u8) != 0) {
        return 1;
    }
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_m1_read = diff_ms(start, end);
#endif

    // Converter input_u8 para f32
    for (int i = 0; i < 262144; i++) {
        input_f32[i] = (float)input_u8[i];
    }

#ifndef WIENER_0
    // 2. Cálculo de Estatística Espacial (Usar f64 para acumuladores)
    double soma = 0.0;
    double soma_quadrados = 0.0;
    double N_val = 262144.0;

    for (int i = 0; i < 262144; i++) {
        double val = (double)input_u8[i];
        soma += val;
        soma_quadrados += val * val;
    }

    double media = soma / N_val;
    double var_noisy_f64 = (soma_quadrados / N_val) - (media * media);
    if (var_noisy_f64 < 0.0) var_noisy_f64 = 0.0;

    float var_noisy = (float)var_noisy_f64;
    if (var_noisy < 1e-4f) var_noisy = 1e-4f; // Previne underflow no M6
#else
    float var_noisy = 0.0f; // Valor dummy para a versão antiga
#endif

    // 3. Preparação para Frequência
    for (int i = 0; i < 262144; i++) {
        spatial[i] = input_f32[i] + 0.0f * I;
    }

    // 4. FFT forward (M2): spatial → X
    for (int i = 0; i < 262144; i++) {
        X[i] = spatial[i];
    }
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    if (fft2_forward(X) != 0) {
        return 1;
    }
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_m2_fwd = diff_ms(start, end);
#endif

    // 5. Power spectrum inline: |X|²
    for (int i = 0; i < 262144; i++) {
        float re = crealf(X[i]);
        float im = cimagf(X[i]);
        power_spectrum[i] = re * re + im * im;
    }

    // 6. Estimativas (M3 e M4)
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    float sigma = estimar_sigma(power_spectrum);
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_m3 = diff_ms(start, end);
#endif

    float sigma_n = estimar_sigma_n(power_spectrum);

    // 7. Construir kernel FFT (M5)
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    kernel_gaussiano_fft(sigma, H);
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_m5 = diff_ms(start, end);
#endif

    // 8. Aplicar Wiener Híbrido (M6 rev 5, wiener_d)
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    wiener_d(X, H, sigma_n, Y);
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_m6 = diff_ms(start, end);
#endif

    // 9. IFFT inversa (M2): Y → spatial
    for (int i = 0; i < 262144; i++) {
        spatial[i] = Y[i];
    }
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    if (fft2_inverse(spatial) != 0) {
        return 1;
    }
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_m2_inv = diff_ms(start, end);
#endif

    for (int i = 0; i < 262144; i++) {
        warm_start_f32[i] = crealf(spatial[i]);
    }

    // 10. Constantes da Cascata
    static const float VS_THRESHOLD = 0.045f;
    static const float TV_LAMBDA = 5.0f;
    static const float TV_RHO = 1.0f;
    static const int TV_N_ITER = 3;

    // 11. Orquestração: wiener_d → var_sp → TV-denoise opcional
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    float vs_base = var_sp(warm_start_f32);
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_m9 = diff_ms(start, end);
    t_m8 = 0.0;
#endif

    float *final_output_ptr;

    if (vs_base <= VS_THRESHOLD) {
        // Textura: mantém saída wiener_d diretamente
        final_output_ptr = warm_start_f32;
#ifdef PROFILE
        t_m10 = 0.0;
#endif
    } else {
        // Suave: aplica TV-denoise sobre a saída do wiener_d
#ifdef PROFILE
        clock_gettime(CLOCK_MONOTONIC, &start);
#endif
        tv_denoise(warm_start_f32, TV_LAMBDA, TV_RHO, TV_N_ITER, output_f32);
#ifdef PROFILE
        clock_gettime(CLOCK_MONOTONIC, &end);
        t_m10 = diff_ms(start, end);
#endif
        final_output_ptr = output_f32;
    }

    // 13. Extrair parte real, clip [0, 255], cast u8
    for (int i = 0; i < 262144; i++) {
        float v = roundf(final_output_ptr[i]); // Arredondamento evita viés de -0.5 LSB
        if (v < 0.0f) {
            v = 0.0f;
        } else if (v > 255.0f) {
            v = 255.0f;
        }
        output_u8[i] = (uint8_t)v;
    }

    // 14. Escrever BMP em stdout
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &start);
#endif
    if (write_bmp(output_u8, stdout) != 0) {
        return 1;
    }
#ifdef PROFILE
    clock_gettime(CLOCK_MONOTONIC, &end);
    t_m1_write = diff_ms(start, end);

    fprintf(stderr, "=== PROFILING C ===\n");
    fprintf(stderr, "M1 (Read BMP):  %.3f ms\n", t_m1_read);
    fprintf(stderr, "M2 (FFT Fwd):   %.3f ms\n", t_m2_fwd);
    fprintf(stderr, "M3 (Sigma):     %.3f ms\n", t_m3);
    fprintf(stderr, "M5 (Kernel):    %.3f ms\n", t_m5);
    fprintf(stderr, "M6 (Wiener):    %.3f ms\n", t_m6);
    fprintf(stderr, "M2 (FFT Inv):   %.3f ms\n", t_m2_inv);
    fprintf(stderr, "M9 (SpatialVar):%.3f ms\n", t_m9);
    fprintf(stderr, "M11 (TV-Denoise):%.3f ms\n", t_m10);
    fprintf(stderr, "M1 (Write BMP): %.3f ms\n", t_m1_write);
    fprintf(stderr, "Total Modules:  %.3f ms\n", t_m1_read + t_m2_fwd + t_m3 + t_m5 + t_m6 + t_m2_inv + t_m9 + t_m10 + t_m1_write);
    fprintf(stderr, "===================\n");
#endif

    return 0;
}
