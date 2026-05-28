# Makefile — deblur pipeline (C)
#
# Build:     make
# Static:    make STATIC=1
# Clean:     make clean
#
# Overridable: CC, CFLAGS, LDFLAGS, OUT
#
# Backend FFT: FFTW3 float (rev 4). Substitui muFFT.
# ATENÇÃO: o binário C é GPL (FFTW é GPL). Rust e Zig NÃO são afetados.
# Ver ADR e README (nota de licenciamento misto).

CC      ?= gcc
OUT     ?= deblur_pipeline_c
SRCDIR  := src/c
TOOLDIR := tools

# Localizar FFTW via brew se disponível (fallback para caminhos do sistema)
FFTW_PREFIX := $(shell brew --prefix fftw 2>/dev/null || echo /usr)
FFTW_INC    := $(FFTW_PREFIX)/include
FFTW_LIB    := $(FFTW_PREFIX)/lib

SRCS    := $(SRCDIR)/main.c $(SRCDIR)/bmp_io.c $(SRCDIR)/fft.c \
           $(SRCDIR)/estimar_sigma.c $(SRCDIR)/estimar_sigma_n.c \
           $(SRCDIR)/kernel_gaussiano.c $(SRCDIR)/wiener.c \
           $(SRCDIR)/autocorr.c $(SRCDIR)/var_sp.c $(SRCDIR)/admm_tv.c \
           $(SRCDIR)/tv_denoise.c

all: $(OUT)

CFLAGS  ?= -O3 -std=c11 -Wall -Wextra -march=x86-64 -mtune=generic
CPPFLAGS := -I$(FFTW_INC)

# Variante de M6 (ver ADR 019 + ADR 022).
#   WIENER=b — wiener_b (rev 3, forma teórica S_n/S_x bin-a-bin). Padrão.
#   WIENER=0 — wiener_0 (rev 1, K=0.04 fixo). Rollback.
#   WIENER=a — wiener_a (rev 2, K dinâmico). Histórico, não submeter.
WIENER ?= b
ifeq ($(WIENER),0)
    CPPFLAGS += -DWIENER_0
endif
ifeq ($(WIENER),b)
    CPPFLAGS += -DWIENER_B
endif

# Variante de M3 (ver ADR 019).
#   M3=a — m3_rev3 (α adaptativo). Padrão.
#   M3=0 — m3_rev2 (α=2.0 fixo). Regressão.
M3 ?= a
ifeq ($(M3),0)
    CPPFLAGS += -DM3_REV2
endif

ifeq ($(STATIC),1)
    LDFLAGS  += -static
    LDLIBS   := -l:libfftw3f.a -lm
else
    LDLIBS   := -L$(FFTW_LIB) -lfftw3f -lm
    LDFLAGS  += -Wl,-rpath,$(FFTW_LIB)
endif

# ── Passo 1: gerar o wisdom header (build-time, uma vez por máquina) ────────
$(SRCDIR)/fftw_wisdom.h: $(TOOLDIR)/gen_wisdom.c
	$(CC) -O2 -I$(FFTW_INC) -o /tmp/gen_wisdom $< $(FFTW_LIB)/libfftw3f.a -lm
	/tmp/gen_wisdom > $@

# ── Passo 2: build principal depende do wisdom header ───────────────────────
$(OUT): $(SRCS) $(SRCDIR)/fftw_wisdom.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $(SRCS) $(LDLIBS)

# CLI shim do M3 para validação cross-language.
m3_cli: tests/c/m3_cli.c $(SRCDIR)/estimar_sigma.c
	$(CC) $(CFLAGS) -o $@ tests/c/m3_cli.c $(SRCDIR)/estimar_sigma.c -lm

# CLI shim do M4 para validação cross-language.
m4_cli: tests/c/m4_cli.c $(SRCDIR)/estimar_sigma_n.c
	$(CC) $(CFLAGS) -o $@ tests/c/m4_cli.c $(SRCDIR)/estimar_sigma_n.c -lm

clean:
	rm -f $(OUT) m3_cli m4_cli $(SRCDIR)/fftw_wisdom.h /tmp/gen_wisdom

.PHONY: all clean
