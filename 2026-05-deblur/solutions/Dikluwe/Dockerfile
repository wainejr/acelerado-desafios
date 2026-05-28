# Build stage: compila com FFTW3 float estatico.
FROM gcc:13-bookworm AS build

# FFTW3 float, versao estatica para link em FROM scratch.
RUN apt-get update && apt-get install -y --no-install-recommends \
        libfftw3-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY Makefile ./
COPY src/c ./src/c
COPY tools ./tools

# Gera wisdom (MEASURE) na CPU do builder e compila.
# ATENÇÃO: o wisdom é específico para a CPU do builder. Em CPU de execução
# diferente, o fft.c cai para ESTIMATE via fallback (ver ADR).
RUN make STATIC=1 WIENER=b M3=a \
 && strip deblur_pipeline_c

# Runtime: scratch. Binario estatico, sem deps dinamicas.
FROM scratch
COPY --from=build /src/deblur_pipeline_c /deblur
ENTRYPOINT ["/deblur"]
