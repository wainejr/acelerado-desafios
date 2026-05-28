/* Crystalline Lineage
 * @prompt prompts/prompt-m2-rev4-fftw.md (rev 4)
 * @module M2/tools
 * @language c
 * @updated 2026-05-28
 *
 * Gera o wisdom FFTW3 para 512x512 c2c forward+inverse usando FFTW_MEASURE
 * e o emite como header C com a string embutida.
 *
 * Uso:
 *   gcc -O2 -o /tmp/gen_wisdom tools/gen_wisdom.c -lfftw3f -lm
 *   /tmp/gen_wisdom > src/c/fftw_wisdom.h
 *
 * Rodado UMA VEZ no docker build (ou localmente antes de make).
 * ATENÇÃO: o wisdom é específico da CPU onde foi gerado. O fft.c inclui
 * fallback para ESTIMATE se o wisdom for rejeitado em CPU diferente.
 */
#include <fftw3.h>
#include <stdio.h>

#define N 512

int main(void) {
    fftwf_complex *buf = fftwf_malloc(sizeof(fftwf_complex) * N * N);
    if (!buf) { fprintf(stderr, "fftwf_malloc falhou\n"); return 1; }

    /* Planejar ambas as direções com MEASURE para popular o wisdom. */
    fftwf_plan pf = fftwf_plan_dft_2d(N, N, buf, buf, FFTW_FORWARD,  FFTW_MEASURE);
    fftwf_plan pi = fftwf_plan_dft_2d(N, N, buf, buf, FFTW_BACKWARD, FFTW_MEASURE);

    if (!pf || !pi) { fprintf(stderr, "planejamento FFTW falhou\n"); return 1; }

    char *w = fftwf_export_wisdom_to_string();
    if (!w)  { fprintf(stderr, "export_wisdom_to_string retornou NULL\n"); return 1; }

    /* Emitir como header C, escapando aspas/barras/newlines. */
    printf("/* AUTO-GERADO em build-time por tools/gen_wisdom.c. Nao editar. */\n");
    printf("static const char FFTW_WISDOM_EMBED[] =\n\"");
    for (const char *p = w; *p; p++) {
        if      (*p == '\n') printf("\\n");
        else if (*p == '"')  printf("\\\"");
        else if (*p == '\\') printf("\\\\");
        else                 putchar(*p);
    }
    printf("\";\n");

    fftwf_free(w);
    fftwf_destroy_plan(pf);
    fftwf_destroy_plan(pi);
    fftwf_free(buf);
    return 0;
}
