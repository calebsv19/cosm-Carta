#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SHADER_DIR="${SCRIPT_DIR}/../shaders"
GLSLC="${GLSLC:-glslc}"

shaders=(
  line.vert
  line.frag
  fill.vert
  fill.frag
  textured.vert
  textured.frag
)

for shader in "${shaders[@]}"; do
  input="${SHADER_DIR}/${shader}"
  output="${input}.spv"
  echo "[compile] ${shader} -> ${output}"
  "${GLSLC}" "${input}" -o "${output}"
done
