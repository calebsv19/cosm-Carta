#!/bin/sh
set -eu

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
CORE_PACK_DIR="$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)"
REPO_DIR="$(CDPATH= cd -- "$CORE_PACK_DIR/../.." && pwd)"

PHYSICS_SRC_DEFAULT="$REPO_DIR/physics_sim/export/volume_frames/MAir/frame_001936.pack"
DAW_SRC_DEFAULT="$REPO_DIR/daw/assets/audio/bounce3.pack"

PHYSICS_SRC="${PHYSICS_PACK_SRC:-$PHYSICS_SRC_DEFAULT}"
DAW_SRC="${DAW_PACK_SRC:-$DAW_SRC_DEFAULT}"

if [ ! -f "$PHYSICS_SRC" ]; then
    echo "missing physics source pack: $PHYSICS_SRC" >&2
    exit 1
fi

if [ ! -f "$DAW_SRC" ]; then
    echo "missing daw source pack: $DAW_SRC" >&2
    exit 1
fi

if [ ! -x "$CORE_PACK_DIR/build/pack_cli" ]; then
    make -C "$CORE_PACK_DIR" tools
fi

cp "$PHYSICS_SRC" "$SCRIPT_DIR/physics_v1_sample.pack"
cp "$DAW_SRC" "$SCRIPT_DIR/daw_v1_sample.pack"

"$CORE_PACK_DIR/build/pack_cli" inspect "$SCRIPT_DIR/physics_v1_sample.pack" > "$SCRIPT_DIR/physics_v1_sample.inspect.txt"
"$CORE_PACK_DIR/build/pack_cli" inspect "$SCRIPT_DIR/daw_v1_sample.pack" > "$SCRIPT_DIR/daw_v1_sample.inspect.txt"

(
    cd "$SCRIPT_DIR"
    shasum -a 256 physics_v1_sample.pack daw_v1_sample.pack > fixtures.sha256
)

echo "fixtures regenerated"
