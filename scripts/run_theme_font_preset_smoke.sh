#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

echo "[smoke] Running shared theme/font adapter tests..."

make -C "${ROOT_DIR}/daw" test-shared-theme-font-adapter
make -C "${ROOT_DIR}/ide" test-shared-theme-font-adapter
make -C "${ROOT_DIR}/line_drawing" test-shared-theme-font-adapter
make -C "${ROOT_DIR}/line_drawing3d" test-shared-theme-font-adapter
make -C "${ROOT_DIR}/ray_tracing" test-shared-theme-font-adapter
make -C "${ROOT_DIR}/physics_sim" test-shared-theme-font-adapter
make -C "${ROOT_DIR}/map_forge" test-shared-theme-font-adapter

echo "[smoke] Completed shared theme/font adapter smoke successfully."
