# Estágio 1: Compilação (Dinâmica)
FROM alpine:3.19 AS builder

RUN apk add --no-cache g++ make fftw-dev

WORKDIR /app
COPY solution.cpp .

RUN g++ -O3 -std=c++20 solution.cpp -lfftw3f -lfftw3 -o solution

# Estágio 2: Execução leve
FROM alpine:3.19

# Correção: apenas 'libgcc' em vez de 'gnu-libgcc'
RUN apk add --no-cache fftw libstdc++ libgcc

COPY --from=builder /app/solution /app/solution

CMD ["/app/solution"]
