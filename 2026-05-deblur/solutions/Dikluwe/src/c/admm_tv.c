/*
 * Crystalline Lineage
 * @prompt prompts/prompt-m10-admm-tv.md (revisão 1)
 * @module M10
 * @language c
 * @updated 2026-05-27
 */

#include <complex.h>
#include <math.h>
#include <string.h>

#define N 512
#define NN (N * N)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern void kernel_gaussiano_fft(float sigma, float complex out[NN]);
extern int fft2_forward(float complex buffer[NN]);
extern int fft2_inverse(float complex buffer[NN]);

static float complex H_FFT[NN];
static float complex DX_F[NN];
static float complex DY_F[NN];
static float DENOM[NN];
static float complex HTY_FFT[NN];
static float X_VAL[NN];
static float Z_X[NN];
static float Z_Y[NN];
static float U_X[NN];
static float U_Y[NN];
static float DX_X[NN];
static float DY_X[NN];
static float complex SCRATCH[NN];
static float complex ZX_UX_FFT[NN];
static float complex ZY_UY_FFT[NN];

void admm_tv(
    const float input[NN],
    const float warm_start[NN],
    float sigma_blur,
    float lambda,
    float rho,
    int n_iter,
    float output[NN]
) {
    // 1. PSF Gaussiana
    kernel_gaussiano_fft(sigma_blur, H_FFT);
    
    // 2. Operadores de gradiente
    for (int i = 0; i < N; i++) {
        double angle_i = -2.0 * M_PI * i / N;
        float complex dy_val = (1.0 - cos(angle_i)) - sin(angle_i) * I;
        
        for (int j = 0; j < N; j++) {
            double angle_j = -2.0 * M_PI * j / N;
            float complex dx_val = (1.0 - cos(angle_j)) - sin(angle_j) * I;
            
            int idx = i * N + j;
            DX_F[idx] = dx_val;
            DY_F[idx] = dy_val;
        }
    }
    
    // 3. Denominador
    for (int i = 0; i < NN; i++) {
        float complex h_val = H_FFT[i];
        float h_abs_sq = crealf(h_val) * crealf(h_val) + cimagf(h_val) * cimagf(h_val);
        
        float complex dx_val = DX_F[i];
        float complex dy_val = DY_F[i];
        float dt_d = (crealf(dx_val) * crealf(dx_val) + cimagf(dx_val) * cimagf(dx_val)) + 
                     (crealf(dy_val) * crealf(dy_val) + cimagf(dy_val) * cimagf(dy_val));
        
        float d_val = h_abs_sq + rho * dt_d;
        if (d_val < 1e-9f) {
            d_val = 1e-9f;
        }
        DENOM[i] = d_val;
    }
    
    // 4. H^T y
    for (int i = 0; i < NN; i++) {
        SCRATCH[i] = input[i] + 0.0f * I;
    }
    (void)fft2_forward(SCRATCH);
    for (int i = 0; i < NN; i++) {
        float complex h_val = H_FFT[i];
        float complex h_conj = crealf(h_val) - cimagf(h_val) * I;
        HTY_FFT[i] = h_conj * SCRATCH[i];
    }
    
    // 5. Inicialização
    memcpy(X_VAL, warm_start, sizeof(float) * NN);
    memset(Z_X, 0, sizeof(float) * NN);
    memset(Z_Y, 0, sizeof(float) * NN);
    memset(U_X, 0, sizeof(float) * NN);
    memset(U_Y, 0, sizeof(float) * NN);
    
    float thresh = lambda / rho;
    float epsilon_shrink = 1e-9f;
    
    // 6. Loop Principal
    for (int k = 0; k < n_iter; k++) {
        // x-update
        for (int i = 0; i < NN; i++) {
            ZX_UX_FFT[i] = (Z_X[i] - U_X[i]) + 0.0f * I;
            ZY_UY_FFT[i] = (Z_Y[i] - U_Y[i]) + 0.0f * I;
        }
        (void)fft2_forward(ZX_UX_FFT);
        (void)fft2_forward(ZY_UY_FFT);
        
        for (int i = 0; i < NN; i++) {
            float complex dx_f = DX_F[i];
            float complex dy_f = DY_F[i];
            float complex dx_conj = crealf(dx_f) - cimagf(dx_f) * I;
            float complex dy_conj = crealf(dy_f) - cimagf(dy_f) * I;
            
            float complex term_x = dx_conj * ZX_UX_FFT[i];
            float complex term_y = dy_conj * ZY_UY_FFT[i];
            
            float complex rhs_val = HTY_FFT[i] + rho * (term_x + term_y);
            float denom_val = DENOM[i];
            SCRATCH[i] = rhs_val / denom_val;
        }
        (void)fft2_inverse(SCRATCH);
        for (int i = 0; i < NN; i++) {
            X_VAL[i] = crealf(SCRATCH[i]);
        }
        
        // Cache de grads
        for (int i = 0; i < NN; i++) {
            SCRATCH[i] = X_VAL[i] + 0.0f * I;
        }
        (void)fft2_forward(SCRATCH);
        
        for (int i = 0; i < NN; i++) {
            ZX_UX_FFT[i] = DX_F[i] * SCRATCH[i];
            ZY_UY_FFT[i] = DY_F[i] * SCRATCH[i];
        }
        (void)fft2_inverse(ZX_UX_FFT);
        (void)fft2_inverse(ZY_UY_FFT);
        
        for (int i = 0; i < NN; i++) {
            DX_X[i] = crealf(ZX_UX_FFT[i]);
            DY_X[i] = crealf(ZY_UY_FFT[i]);
        }
        
        // z-update
        for (int i = 0; i < NN; i++) {
            float t_x = DX_X[i] + U_X[i];
            float t_y = DY_X[i] + U_Y[i];
            float mag = sqrtf(t_x * t_x + t_y * t_y);
            float shrunk = (mag - thresh > 0.0f) ? (mag - thresh) : 0.0f;
            float scale = shrunk / (mag + epsilon_shrink);
            Z_X[i] = scale * t_x;
            Z_Y[i] = scale * t_y;
        }
        
        // u-update
        for (int i = 0; i < NN; i++) {
            U_X[i] = U_X[i] + DX_X[i] - Z_X[i];
            U_Y[i] = U_Y[i] + DY_X[i] - Z_Y[i];
        }
    }
    
    // Pós-processamento (clip no final)
    for (int i = 0; i < NN; i++) {
        float v = X_VAL[i];
        float clamped = v;
        if (v < 0.0f) clamped = 0.0f;
        else if (v > 255.0f) clamped = 255.0f;
        output[i] = clamped;
    }
}
