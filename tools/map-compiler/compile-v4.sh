#!/usr/bin/env bash
# PurrGO: OSM -> optimizer -> map package V4 with search index.
#
# Usage: compile-v4.sh <input.osm|input.osm.pbf> [output_dir]
#
# Output folder default: <this script's folder>/<input name>-v4
# The map name (map.name) is the input file name without extension.
# A .pbf input needs the `osmium` command line tool (osmium cat).
set -euo pipefail

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "Usage: $0 <input.osm|input.osm.pbf> [output_dir]" >&2
    exit 2
fi

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INPUT="$1"
[ -f "$INPUT" ] || { echo "Error: $INPUT not found" >&2; exit 1; }
INPUT="$(cd "$(dirname "$INPUT")" && pwd)/$(basename "$INPUT")"

PY="${PYTHON:-}"
if [ -z "$PY" ]; then
    if command -v python3 >/dev/null 2>&1; then PY=python3; else PY=python; fi
fi

BASE="$(basename "$INPUT")"
NAME="${BASE%.pbf}"
NAME="${NAME%.osm}"
OUT="${2:-$DIR/$NAME-v4}"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

case "$BASE" in
    *.pbf)
        command -v osmium >/dev/null 2>&1 || { echo "Error: osmium not found (needed for .pbf)" >&2; exit 1; }
        echo "[1/3] osmium cat: $BASE"
        osmium cat "$INPUT" -o "$WORK/source.osm"
        SRC="$WORK/source.osm"
        ;;
    *)
        SRC="$INPUT"
        ;;
esac

# The compiler takes the map name from the file name.
echo "[2/3] Optimizing OSM"
"$PY" "$DIR/purrgo_osm_optimizer.py" "$SRC" "$WORK/$NAME.osm"

echo "[3/3] Compiling map V4 with search index -> $OUT"
"$PY" "$DIR/purrgo_map_compiler.py" "$WORK/$NAME.osm" \
    --format 4 --search -o "$OUT"

echo "Done: $OUT"
