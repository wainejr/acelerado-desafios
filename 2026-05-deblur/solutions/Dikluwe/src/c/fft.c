/*
 * Crystalline Lineage
 * @prompt prompts/prompt-m2-rev4-fftw.md (revisão 4)
 * @module M2
 * @language c
 * @updated 2026-05-28
 * @note Backend FFTW3 float (GPL). O binário C é GPL. Ver ADR e README.
 *
 * Interface pública inalterada:
 *   int fft2_forward(float complex buffer[512*512]);  // retorna 0 em sucesso
 *   int fft2_inverse(float complex buffer[512*512]);  // normaliza ÷N²
 *
 * Estratégia de planejamento (MEASURE + wisdom embutido + fallback ESTIMATE):
 *   1. Importa wisdom gerado em build-time (FFTW_WISDOM_EMBED).
 *   2. Tenta criar plano com FFTW_WISDOM_ONLY|FFTW_MEASURE (~1ms se wisdom bate).
 *   3. Se NULL (wisdom incompatível com CPU de execução), cria com FFTW_ESTIMATE
 *      (~0ms planejamento, ~75ms execução — dentro do limite de 200ms).
 *   O caso catastrófico (MEASURE sem wisdom em runtime, ~280ms) nunca ocorre.
 */

#include <complex.h>
#include <fftw3.h>
#include "fftw_wisdom.h"  /* define: static const char FFTW_WISDOM_EMBED[]; */

#define N   512
#define NN  (N * N)

static fftwf_plan plan_forward = NULL;
static fftwf_plan plan_inverse = NULL;
static int wisdom_loaded = 0;

/* Importa o wisdom embutido (uma vez). */
static void ensure_wisdom(void) {
    if (!wisdom_loaded) {
        fftwf_import_wisdom_from_string(FFTW_WISDOM_EMBED);
        wisdom_loaded = 1;
    }
}

/*
 * Cria plano para `direction` (FFTW_FORWARD ou FFTW_BACKWARD).
 * Tenta WISDOM_ONLY primeiro; cai para ESTIMATE se o wisdom
 * não for aplicável nesta CPU.
 *
 * NOTA: usamos fftwf_execute_dft (new-array execute) para reutilizar o
 * mesmo plano em buffers distintos. O buffer passado aqui só serve para
 * o planejamento; na execução real, qualquer buffer do mesmo tamanho
 * e alinhamento é aceito.
 */
static fftwf_plan make_plan(fftwf_complex *buf, int direction) {
    ensure_wisdom();

    /* FFTW_WISDOM_ONLY: retorna NULL se não há wisdom adequado. */
    fftwf_plan p = fftwf_plan_dft_2d(N, N, buf, buf, direction,
                                     FFTW_WISDOM_ONLY | FFTW_MEASURE);
    if (!p) {
        /* Fallback: ESTIMATE (planejamento instantâneo, execução ~75ms). */
        p = fftwf_plan_dft_2d(N, N, buf, buf, direction, FFTW_ESTIMATE);
    }
    return p;
}

/* Retorna 0 em sucesso. Buffer sobrescrito in-place com FFT 2D forward. */
int fft2_forward(float complex buffer[NN]) {
    if (!plan_forward) {
        plan_forward = make_plan((fftwf_complex *)buffer, FFTW_FORWARD);
        if (!plan_forward) return 1;
    }
    fftwf_execute_dft(plan_forward,
                      (fftwf_complex *)buffer,
                      (fftwf_complex *)buffer);
    return 0;
}

/* Retorna 0 em sucesso. Buffer sobrescrito in-place com IFFT 2D + norm 1/N². */
int fft2_inverse(float complex buffer[NN]) {
    if (!plan_inverse) {
        plan_inverse = make_plan((fftwf_complex *)buffer, FFTW_BACKWARD);
        if (!plan_inverse) return 1;
    }
    fftwf_execute_dft(plan_inverse,
                      (fftwf_complex *)buffer,
                      (fftwf_complex *)buffer);
    /* FFTW não normaliza. Aplicamos 1/N² igual à convenção muFFT anterior. */
    const float scale = 1.0f / (float)NN;
    for (int i = 0; i < NN; i++) {
        buffer[i] *= scale;
    }
    return 0;
}
