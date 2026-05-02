# Maio 2026 — Astrofoto da Lua: desfocando o seeing

## O cenário

Numa quinta à noite em São Paulo, um astrofotógrafo amador monta no quintal um
**telescópio Newtoniano de 130 mm** com uma **ZWO ASI120MM-S** acoplada — uma
câmera CMOS monocromática de 1.2 MP, USB 2.0, sem sistema de resfriamento.
Aponta pra Lua, configura ganho médio, e em 90 segundos captura **3 mil frames
da cratera Tycho** com exposição de 5 ms cada.

No software de processamento ele descobre o que todo mundo no hobby já sabe:
**a maior parte dos frames está borrada**. Cinco a dez por cento das fotos
saem "tremidas" de um jeito específico — não é a câmera mexendo, é o **ar**.

Entre o telescópio e a Lua existem 100 km de atmosfera turbulenta. Cada
camada de ar quente subindo, ar frio descendo, vento cisalhando, age como
uma **lente fraca e mal alinhada** sobre o caminho óptico. O efeito acumulado
distorce e borra a imagem do mesmo jeito que olhar pra fundo de piscina mexida.
A galera chama esse fenômeno de **seeing**.

Pra um intervalo de 5 ms, o seeing é bem aproximado por uma **convolução
2D com uma PSF gaussiana**, com um σ que depende do estado da atmosfera
**naquele exato instante**. Em uma noite excelente em SP, σ ≈ 1.5 pixels.
Em uma noite ruim ou perto do horizonte, σ ≈ 3.5 pixels. Frame a frame, o
σ varia.

Em paralelo, a ASI120MM **mete ruído**. Sem TEC pra esfriar o sensor, a
leitura de cada pixel adiciona um ruído **branco gaussiano** com σ_n ≈ 2 ADUs
no domínio 0–255. É o "chiado" que aparece como granulação fina nas áreas
escuras do disco lunar.

A foto que sai do sensor satisfaz:

```
foto_borrada = nítida ⊛ gaussiana(σ_seeing) + ruído_branco
```

Onde:
- `⊛` é convolução 2D **periódica** (borda da imagem se enrola — o frame
  é tratado como um toro 512×512)
- `σ_seeing` é desconhecido pra cada frame, mas **garantido em [1.5, 3.5]**
- `ruído_branco ~ N(0, σ_n²)` aditivo, com σ_n = 2.0 fixo

## A tarefa

O astrofotógrafo te paga um café se você devolver o melhor que dá pra fazer
do frame ruim. Concretamente: **dado um BMP borrado, retornar a estimativa
da imagem nítida, o mais rápido possível**. Você não conhece o σ_seeing
daquele frame específico — tem que descobrir ou contornar.

O desafio é por **tempo**: quem entrega rápido sem perder qualidade de
recuperação ganha. Detalhes do que conta como "qualidade aceitável" estão em
[Spec](#spec) abaixo.

## Spec

### Formato de I/O

- **Entrada (stdin)**: BMP 24-bit, **512×512**, sem compressão, grayscale
  armazenado como R = G = B em cada pixel.
  - 14 bytes `BITMAPFILEHEADER` + 40 bytes `BITMAPINFOHEADER` = **54 bytes
    de cabeçalho**, pixels começam no byte 54
  - Linhas armazenadas **de baixo pra cima**, BGR por pixel
  - Width 512 → row stride 1536 bytes, **sem padding**
- **Saída (stdout)**: BMP no exato mesmo formato e dimensões, com a
  imagem deblur'd. Pixels fora de `[0, 255]` devem ser clipados antes de
  escrever em `uint8`.

### Sem I/O em arquivo

A solução roda em container Docker com:

```
--read-only        # filesystem todo somente-leitura
                   # (sem --tmpfs, sem volume de escrita)
--network=none     # sem rede
```

Sua solução **não pode escrever em nenhum arquivo**. Apenas stdin de leitura,
stdout/stderr de escrita, memória RAM, e o filesystem somente-leitura do
próprio container. Tentar escrever em `/tmp`, `~/.cache`, qualquer coisa,
falha com `EROFS`.

### Modelo direto (forward model)

Cada frame de teste é gerado por:

1. Imagem nítida `nitida` (uint8, 512×512 grayscale)
2. PSF gaussiana periódica via FFT, com σ_seeing sorteado uniformemente em
   `[1.5, 3.5]` (semente determinística por imagem; participantes não sabem
   quais σ saíram).
3. Convolução circular: `borrada = IFFT( FFT(nítida) · FFT(psf) )`
4. Ruído branco gaussiano aditivo: `borrada += N(0, 2.0²)`
5. Clipping em `[0, 255]` e quantização para `uint8` → arquivo BMP

Sua solução assume **exatamente esse modelo**. Convolução periódica (não
zero-padded), ruído branco com σ_n = 2.0 conhecido.

### Validação

- **PSNR ≥ 21 dB** contra `expected/<caso>.bmp` em **todos** os casos
  (públicos + ocultos). PSNR é calculado como
  `10 · log₁₀(255² / MSE)`, onde MSE é o erro quadrático médio sobre os
  262 144 pixels.
- Solução com PSNR abaixo do threshold em qualquer caso é **desclassificada**
  — não recebe score de tempo.

### Caps

- `time_ms ≤ 30 000` (30 segundos por caso)
- `peak_rss_mb ≤ 1024` (1 GB de RSS)

Estourar qualquer cap também desclassifica.

### Ranking

`time_ms` mediano (5 runs medidos + 1 warmup, via `hyperfine`), agregado
sobre todos os casos pela **mediana das medianas**. Empates desempatados
pelo timestamp de submissão (mais cedo vence).

## Construir a PSF (referência)

A PSF gaussiana com um σ específico é construída no domínio da frequência
como segue (a referência usa numpy; em outras linguagens, reproduzir o
mesmo layout):

```python
import numpy as np

def gaussian_psf_freq(shape, sigma):
    h, w = shape
    yy = np.fft.fftfreq(h) * h
    xx = np.fft.fftfreq(w) * w
    yy, xx = np.meshgrid(yy, xx, indexing="ij")
    psf = np.exp(-(xx**2 + yy**2) / (2 * sigma**2))
    psf /= psf.sum()
    return np.fft.fft2(psf)
```

Equivalente: kernel gaussiano centrado em `(0, 0)` com periodicidade
implícita 512, normalizado pra somar 1 no domínio espacial. Implementações
em outras linguagens devem reproduzir esse layout — sob pena de a
deconvolução errar a fase.

## A referência

Em `reference/wiener.py` mora um solver **intencionalmente burro**: aplica
o filtro de Wiener com **σ fixo = 2.5** (o ponto médio da faixa). Quando o
σ verdadeiro é próximo de 2.5 ele sai bem; quando é 1.5 ou 3.5, sofre.

A referência **não estima** σ. Sua solução deve fazer melhor:

- **Estimar σ a partir do espectro de potência** (técnica clássica:
  `log P(f) ≈ -2π²σ²f² - α log(f) + c`, ajusta linear)
- **Cepstrum analysis** (aplicável a kernels com zeros — gaussiana não tem,
  mas variações suavizadas dão pistas)
- **Sweep paralelo + escolha por métrica de foco** (Laplacian variance,
  total variation, gradient sparsity)
- **Métodos iterativos** (Richardson-Lucy, TV-regularizado)
- **Rede neural** (out of scope esse mês — sem GPU no bench, sem rede)

A regularização `λ` que a referência usa é `0.005`, calibrada pro nível de
ruído σ_n = 2.0. Você pode (e deve) escolher diferente.

## Dataset público

9 imagens clássicas de processamento, todas normalizadas para 512×512 em
escala de cinza. Originais em `reference/originals/` (gerado on-demand pelo
script de geração, não commitado).

| Slug | Imagem | Origem |
|---|---|---|
| `cameraman` | The Cameraman | MIT (via skimage.data) |
| `mandrill` | Mandrill (Baboon) | USC-SIPI 4.2.03 |
| `peppers` | Peppers | USC-SIPI 4.2.07 |
| `airplane` | F-16 Airplane | USC-SIPI 4.2.05 |
| `lake` | Sailboat on Lake | USC-SIPI 4.2.06 |
| `boat` | Boat | USC-SIPI boat.512 |
| `house` | House | USC-SIPI 4.1.05 (upscaled) |
| `couple` | Couple | USC-SIPI 5.2.08 |
| `stream` | Stream and bridge | USC-SIPI 5.2.10 |

`inputs/<slug>.bmp` é o frame "saído da câmera" (borrado + ruidoso).
`expected/<slug>.bmp` é o ground truth (a imagem nítida — você não tem
isso na prática, é só pra você validar localmente).

> **Mandrill** é o caso mais difícil: a textura fina dos pelos do focinho
> gera ruído de alta frequência onde o seeing comeu o sinal mais útil. A
> referência passa raspando aqui (PSNR ~21.8 dB). Se você não passar do
> threshold no mandrill, é onde começar a debugar.

## Dataset oculto

Existem casos ocultos adicionais que só são revelados quando o desafio
encerra. São fotos no mesmo formato, mesmo modelo direto, com σ_seeing
sorteado da mesma faixa. Sua solução roda contra eles no benchmark final.

## Como testar localmente

Pré-requisitos: `uv` (Python 3.11+), `docker`, `hyperfine`, `jq`.

```bash
# 1. Gerar o dataset (cache em reference/originals/, faz a parte de rede uma vez)
uv run python 2026-05-deblur/reference/generate.py

# 2. Rodar a referência num caso e conferir o PSNR
uv run python 2026-05-deblur/reference/wiener.py \
    < 2026-05-deblur/inputs/cameraman.bmp \
    > /tmp/cameraman_out.bmp

uv run python 2026-05-deblur/reference/score.py \
    /tmp/cameraman_out.bmp \
    2026-05-deblur/expected/cameraman.bmp
# → algo tipo "26.2221"
```

Pra rodar o bench harness contra a referência (precisa de `docker`):

```bash
docker build -t acelerado-ref:bench 2026-05-deblur/reference/
2026-05-deblur/bench/run.sh 2026-05-deblur/reference/
```

## Como submeter

Veja [`../SUBMISSION.md`](../SUBMISSION.md). TL;DR:

1. `solutions/<seu-usuario>/Dockerfile` constrói sua solução
2. Container lê BMP de stdin, escreve BMP em stdout — **sem escrever
   nenhum arquivo**
3. PR contra a branch `submissions/<seu-usuario>` ou `main` (público)

## Atribuição

Imagens de teste do **USC-SIPI** (University of Southern California, Signal
and Image Processing Institute, "for research and educational use") e do
**MIT** (Cameraman, via `skimage.data`).
