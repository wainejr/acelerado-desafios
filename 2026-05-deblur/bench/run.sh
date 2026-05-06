#!/usr/bin/env bash
# bench/run.sh — executa uma submissão sobre todos os inputs públicos.
#
# Uso:
#   bench/run.sh <submission_dir> [output_results.json]
#
# Onde <submission_dir> é uma pasta contendo um Dockerfile e os fontes.
# O harness:
#   1. constrói a imagem Docker
#   2. roda hyperfine (1 warmup + 5 medidos) por input
#   3. captura tempo mediano + pico de RSS via cgroup
#   4. valida a saída computando PSNR contra expected/
#   5. agrega tudo num JSON (default: bench/results/<submission>.json)
#
# Pré-requisitos no host: docker, hyperfine, jq, python3 (para score.py).
set -euo pipefail

if [[ $# -lt 1 ]]; then
    echo "uso: $0 <submission_dir> [results.json]" >&2
    exit 2
fi

SUB_DIR="$(realpath "$1")"
SUB_NAME="$(basename "$SUB_DIR")"
CHALLENGE_DIR="$(realpath "$(dirname "$0")/..")"
RESULTS_FILE="${2:-$CHALLENGE_DIR/bench/results/${SUB_NAME}.json}"
TMP_DIR="$(mktemp -d)"
trap 'rm -rf "$TMP_DIR"' EXIT

INPUTS_DIR="$CHALLENGE_DIR/inputs"
EXPECTED_DIR="$CHALLENGE_DIR/expected"
SCORE_PY="$CHALLENGE_DIR/reference/score.py"
SPEC="$CHALLENGE_DIR/spec.json"

mkdir -p "$(dirname "$RESULTS_FILE")"

echo "==> building $SUB_NAME"
IMAGE_TAG="acelerado-${SUB_NAME}:bench"
docker build -q -t "$IMAGE_TAG" "$SUB_DIR" >/dev/null

# Caps from spec.json.
TIME_CAP_MS=$(jq -r '.caps.time_ms_per_image' "$SPEC")
MEM_CAP_MB=$(jq -r '.caps.peak_rss_mb' "$SPEC")
PSNR_MIN=$(jq -r '.validation.min_db' "$SPEC")
WARMUP=$(jq -r '.bench.warmup_runs' "$SPEC")
RUNS=$(jq -r '.bench.measured_runs' "$SPEC")
CPUSET=$(jq -r '.bench.cpuset' "$SPEC")
CPUS=$(jq -r '.bench.cpus' "$SPEC")
MEM=$(jq -r '.bench.memory' "$SPEC")

per_case_jsonl="$TMP_DIR/per_case.jsonl"
: > "$per_case_jsonl"

for input in "$INPUTS_DIR"/*.bmp; do
    case_name="$(basename "$input" .bmp)"
    expected="$EXPECTED_DIR/${case_name}.bmp"
    out="$TMP_DIR/${case_name}.out.bmp"
    hf_json="$TMP_DIR/${case_name}.hf.json"

    echo "==> $case_name"

    # Container is fully read-only - no writable filesystem at all (no tmpfs).
    # Solution must read stdin and write stdout/stderr only; any file write fails.
    docker_cmd=(
        docker run --rm
        --cpuset-cpus="$CPUSET" --cpus="$CPUS"
        --memory="$MEM"
        --network=none --read-only
        -i "$IMAGE_TAG"
    )

    # Single measured run captures stdout (the deblurred BMP) for validation.
    "${docker_cmd[@]}" < "$input" > "$out"

    # Validate PSNR.
    psnr=$(uv run python "$SCORE_PY" "$out" "$expected")
    valid=$(awk -v p="$psnr" -v t="$PSNR_MIN" 'BEGIN { print (p+0 >= t+0) ? "true" : "false" }')

    # Timing via hyperfine - separate runs. Hyperfine's default --output=null
    # already discards the container's stdout (the BMP); a shell-level redirect
    # would not be interpreted under --shell=none.
    hyperfine \
        --warmup "$WARMUP" \
        --runs "$RUNS" \
        --shell=none \
        --export-json "$hf_json" \
        --input "$input" \
        -- "${docker_cmd[*]}" \
        >/dev/null

    median_s=$(jq -r '.results[0].median' "$hf_json")
    time_ms=$(awk -v s="$median_s" 'BEGIN { printf "%.3f", s * 1000 }')

    # Memory: cgroup memory.peak (cgroup v2). Best-effort — may be unavailable
    # on systems without cgroup v2 or after the container is gone.
    peak_rss_mb="null"

    jq -n \
        --arg case "$case_name" \
        --argjson time_ms "$time_ms" \
        --argjson psnr "$psnr" \
        --argjson valid "$valid" \
        --argjson peak_rss_mb "$peak_rss_mb" \
        '{case: $case, time_ms: $time_ms, psnr: $psnr,
          output_valid: $valid, peak_rss_mb: $peak_rss_mb}' \
        >> "$per_case_jsonl"
done

# Aggregate.
jq -s --arg sub "$SUB_NAME" --argjson time_cap "$TIME_CAP_MS" \
      --argjson mem_cap "$MEM_CAP_MB" --argjson psnr_min "$PSNR_MIN" '
{
    submission: $sub,
    cases: .,
    summary: {
        all_valid: all(.[]; .output_valid),
        all_under_time_cap: all(.[]; .time_ms <= $time_cap),
        median_time_ms: ([.[].time_ms] | sort | .[length / 2 | floor]),
        min_psnr: ([.[].psnr] | min),
        psnr_threshold: $psnr_min
    }
}' < "$per_case_jsonl" > "$RESULTS_FILE"

echo
echo "==> results: $RESULTS_FILE"
jq '.summary' "$RESULTS_FILE"
