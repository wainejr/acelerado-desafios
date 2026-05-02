# Como submeter uma solução

Cada submissão é um diretório dentro de `<desafio>/solutions/<seu-usuario>/` contendo:

- `Dockerfile` — constrói a imagem que roda sua solução
- Arquivos-fonte que o `Dockerfile` referencia
- `meta.json` (opcional) — metadados extras

## Contrato de execução

O harness vai construir sua imagem e rodar:

```bash
docker run --rm \
  --cpuset-cpus=2,3 --cpus=2 \
  --memory=1g \
  --network=none --read-only \
  -i <sua-imagem> < inputs/<caso>.bin > out.bin
```

Sua solução:

- **Lê de stdin**, **escreve em stdout**.
- **Não recebe argumentos.**
- **Não tem rede** (`--network=none`).
- **Não tem disco gravável** (`--read-only` sem `--tmpfs`). Filesystem inteiro do
  container é somente-leitura. Tentar escrever em qualquer arquivo (incluindo
  `/tmp`, `~/.cache`, `/var/log`, etc.) falha com `EROFS`. Use **memória RAM**
  pra trabalhar.

Dicas práticas:

- **Python**: rode com `python -B` (ou `PYTHONDONTWRITEBYTECODE=1`) pra não
  tentar escrever `.pyc`. Bibliotecas que cacheam em `~/.cache` (matplotlib,
  numba JIT, etc.) podem falhar — pré-construa caches no `Dockerfile`, não em
  runtime, ou use `numpy` puro.
- **C/C++ com FFTW**: `fftw_wisdom` por default escreve em arquivo. Use
  apenas o wisdom embutido no binário ou desligue persistência.
- **CUDA / GPU**: indisponível no bench (não tem GPU no host).

O formato exato de `inputs/*` e da saída esperada está descrito no `README.md` de cada desafio.

## `meta.json` (opcional)

```json
{
  "language": "C",
  "display_name": "Fulano",
  "public_during_month": false
}
```

- `language`: aparece no post de resultados.
- `display_name`: como você quer ser citado (default: GitHub username).
- `public_during_month`: se `true`, sua submissão é mesclada em `main` imediatamente — fica visível pra todo mundo durante o mês. Default `false` (privada até o fechamento).

## Exemplo mínimo (C)

`solutions/fulano/Dockerfile`:
```dockerfile
FROM gcc:13-slim AS build
COPY solution.c /src/
RUN gcc -O3 -march=native -o /app/solution /src/solution.c -lm

FROM debian:bookworm-slim
COPY --from=build /app/solution /app/solution
CMD ["/app/solution"]
```

`solutions/fulano/solution.c`:
```c
#include <stdio.h>
int main(void) {
    /* ler stdin, processar inteiramente em memória, escrever stdout.
       Não fopen() pra disco — o container é read-only. */
    return 0;
}
```

## Como abrir o PR

1. Fork do repo
2. Branch `submissions/<seu-usuario>` (privacidade padrão) **ou** `solutions/<seu-usuario>` no `main` (público)
3. PR contra o branch correspondente no upstream
4. Aguarde a confirmação do bot

## Validação local

Cada desafio tem inputs/expected públicos para você testar sua solução localmente antes de submeter. Veja o `README.md` do desafio para o comando exato.
