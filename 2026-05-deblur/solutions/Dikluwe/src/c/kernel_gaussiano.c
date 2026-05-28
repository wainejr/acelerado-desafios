/*
 * Crystalline Lineage
 * @prompt prompts/05-kernel-gaussiano.md (revisão 1)
 * @module M5
 * @language c
 * @updated 2026-05-14
 */

#include <complex.h>
#include <math.h>

/* Declaração da função de FFT do M2 */
extern int fft2_forward(float complex buffer[512 * 512]);

/// Gera a FFT 2D do kernel Gaussiano com largura `sigma`.
/// 
/// O resultado é preenchido in-place no buffer `out`.
/// Se `sigma` for muito pequeno (< 1e-6), retorna a FFT do delta de Dirac (todos os bins = 1.0).
void kernel_gaussiano_fft(float sigma, float complex out[512 * 512]) {
    // Passo 1: Tratamento do caso degenerado σ → 0
    // Também trata casos de σ negativo.
    if (sigma < 1e-6f) {
        for (int i = 0; i < 262144; i++) {
            out[i] = 1.0f + 0.0f * I;
        }
        return;
    }

    // Clamp para sigma grande (>10) para evitar underflow total espúrio
    if (sigma > 10.0f) {
        sigma = 10.0f;
    }

    // Passo 2: Construir kernel 1D (separável)
    float inv_2sigma2 = 1.0f / (2.0f * sigma * sigma);
    float kernel_1d[512];

    for (int i = 0; i < 512; i++) {
        // Replicar fftfreq * N: [0, 1, ..., 255, -256, -255, ..., -1]
        float freq = (i <= 255) ? (float)i : (float)(i - 512);
        float f_squared = freq * freq;
        kernel_1d[i] = expf(-f_squared * inv_2sigma2);
    }

    // Passo 3: Outer product (separabilidade)
    // Aproveitamos o buffer de saída para construir a PSF espacial e evitar alocação na stack.
    for (int v = 0; v < 512; v++) {
        for (int u = 0; u < 512; u++) {
            float val = kernel_1d[v] * kernel_1d[u];
            out[v * 512 + u] = val + 0.0f * I;
        }
    }

    // Passo 4: Normalização
    float sum = 0.0f;
    for (int i = 0; i < 262144; i++) {
        sum += crealf(out[i]);
    }

    for (int i = 0; i < 262144; i++) {
        out[i] /= sum;
    }

    // Passo 5: FFT 2D via M2
    (void)fft2_forward(out);
}
