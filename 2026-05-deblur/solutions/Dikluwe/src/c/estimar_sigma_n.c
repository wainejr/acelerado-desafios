/* 
 * Crystalline Lineage
 * @prompt prompts/04-estimar-sigma-n.md (revisão 1)
 * @module M4
 * @language c
 * @updated 2026-05-14
 */

#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * Estima o desvio‑padrão do ruído branco gaussiano (σ_n) a partir do
 * espectro de potência fornecido.  O algoritmo segue exatamente o esquema
 * descrito em prompts/04-estimar-sigma-n.md (rev 1) e não realiza nenhuma
 * alocação dinâmica, garantindo uso exclusivo de memória na pilha.
 * A função é puramente determinística e não contém estado global,
 * atendendo ao requisito de thread‑safety.
 */
float estimar_sigma_n(const float power_spectrum[512 * 512])
{
    /* -------------------------------------------------------------
     * Passo 1: Binning radial (cópia independente do módulo M3)
     * ------------------------------------------------------------- */
    float sum_power[256] = {0.0f};
    uint32_t count[256]   = {0};

    for (int v = 0; v < 512; ++v) {
        int dv  = (v <= 256) ? v : 512 - v;
        float dv2 = (float)(dv * dv);
        int row_offset = v * 512;

        for (int u = 0; u < 512; ++u) {
            int du  = (u <= 256) ? u : 512 - u;
            float du2 = (float)(du * du);

            float r = sqrtf(du2 + dv2);
            int r_bin = (int)roundf(r);

            if (r_bin >= 0 && r_bin < 256) {
                sum_power[r_bin] += power_spectrum[row_offset + u];
                ++count[r_bin];
            }
        }
    }

    /* -------------------------------------------------------------
     * Passo 2: Estimar noise floor em r ∈ [200, 256)
     * ------------------------------------------------------------- */
    float nfl_sum   = 0.0f;
    uint32_t nfl_count = 0;

    for (int r = 200; r < 256; ++r) {
        if (count[r] > 0) {
            nfl_sum   += sum_power[r];
            nfl_count += count[r];
        }
    }

    /* Falha degenerada – sem dados suficientes para estimar ruído */
    if (nfl_count == 0) {
        return 10.0f;
    }

    /* -------------------------------------------------------------
     * Passo 3: Aplicar a fórmula de sigma_n
     * ------------------------------------------------------------- */
    float noise_floor = nfl_sum / (float)nfl_count;
    const float K = 262144.0f;               /* N_total = 512*512 */
    float sigma_n = sqrtf(fmaxf(noise_floor, 0.0f) / K);

    /* -------------------------------------------------------------
     * Passo 4: Validação de faixa [3, 20]; fallback para 10.0
     * ------------------------------------------------------------- */
    if (sigma_n < 3.0f || sigma_n > 20.0f) {
        return 10.0f;
    }

    return sigma_n;
}
