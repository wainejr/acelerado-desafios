/*
 * Crystalline Lineage
 * @prompt prompts/01-bmp-io.md (revisão 3)
 * @module M1
 * @language c
 * @updated 2026-05-13
 */

#include <stdio.h>
#include <stdint.h>

static const uint8_t BMP_HEADER[54] = {
    /* Offset 0-13: BITMAPFILEHEADER (14 bytes) */
    0x42, 0x4D,             /* 'BM' magic */
    0x36, 0x00, 0x0C, 0x00, /* file size: 786486 = 0x000C0036 (LE) */
    0x00, 0x00, 0x00, 0x00, /* reserved */
    0x36, 0x00, 0x00, 0x00, /* offset to pixels: 54 = 0x36 (LE) */

    /* Offset 14-53: BITMAPINFOHEADER (40 bytes) */
    0x28, 0x00, 0x00, 0x00, /* header size: 40 = 0x28 (LE) */
    0x00, 0x02, 0x00, 0x00, /* width: 512 = 0x200 (LE) */
    0x00, 0x02, 0x00, 0x00, /* height: 512 = 0x200 (LE, positive = bottom-up) */
    0x01, 0x00,             /* planes: 1 */
    0x18, 0x00,             /* bits per pixel: 24 = 0x18 */
    0x00, 0x00, 0x00, 0x00, /* compression: 0 (BI_RGB) */
    0x00, 0x00, 0x0C, 0x00, /* image size: 786432 = 0x000C0000 (LE) */
    0x00, 0x00, 0x00, 0x00, /* x pixels per meter: irrelevante */
    0x00, 0x00, 0x00, 0x00, /* y pixels per meter: irrelevante */
    0x00, 0x00, 0x00, 0x00, /* colors in palette: 0 */
    0x00, 0x00, 0x00, 0x00, /* important colors: 0 */
};

/* Retorna 0 em sucesso, !=0 em erro. */
int read_bmp(FILE* in_stream, uint8_t out[512 * 512]) {
    uint8_t header_buf[54];
    
    /* Leitura e descarte do cabeçalho de 54 bytes */
    /* Valida o número de bytes retornados para tratar EOF prematuro (T6) */
    if (fread(header_buf, 1, 54, in_stream) != 54) {
        return 1;
    }

    /* Buffer temporário na stack para uma linha (1536 bytes) */
    uint8_t row_buf[1536];
    
    /* As linhas do arquivo BMP estão ordenadas de baixo para cima (bottom-up) */
    for (int i = 0; i < 512; i++) {
        if (fread(row_buf, 1, 1536, in_stream) != 1536) {
            return 1;
        }
        
        /* A linha lida 'i' vai para a posição '511 - i' no buffer de saída (top-down) */
        int out_row_idx = 511 - i;
        int out_offset = out_row_idx * 512;
        
        for (int c = 0; c < 512; c++) {
            /* Em grayscale, os canais R=G=B têm o mesmo valor. Pega o primeiro byte do pixel */
            out[out_offset + c] = row_buf[c * 3];
        }
    }

    return 0;
}

/* Retorna 0 em sucesso, !=0 em erro. */
int write_bmp(const uint8_t buf[512 * 512], FILE* out_stream) {
    /* Escreve o cabeçalho estático e valida a escrita */
    if (fwrite(BMP_HEADER, 1, 54, out_stream) != 54) {
        return 1;
    }

    /* Buffer temporário na stack */
    uint8_t row_buf[1536];

    for (int i = 0; i < 512; i++) {
        /* A linha 'i' que vai para o BMP (bottom-up) vem da posição '511 - i' do buffer interno */
        int in_row_idx = 511 - i;
        int in_offset = in_row_idx * 512;

        for (int c = 0; c < 512; c++) {
            uint8_t pixel_val = buf[in_offset + c];
            
            /* Expande para 3 bytes iguais (BGR) */
            row_buf[c * 3]     = pixel_val;
            row_buf[c * 3 + 1] = pixel_val;
            row_buf[c * 3 + 2] = pixel_val;
        }

        /* Verifica se todos os bytes foram escritos com sucesso (T7) */
        if (fwrite(row_buf, 1, 1536, out_stream) != 1536) {
            return 1;
        }
    }

    return 0;
}
