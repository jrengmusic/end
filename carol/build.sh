#!/usr/bin/env bash
# CAROL build script — SSOT version renderer.
#
# Usage:
#   ./build.sh              Re-render using current VERSION file
#   ./build.sh v0.0.8       Bump VERSION to 0.0.8 and re-render
#   ./build.sh 0.0.8        Same (leading 'v' is optional)
#
# Renders every *.tmpl file by substituting {{VERSION}} with the contents
# of the VERSION file. The VERSION file is the single source of truth.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

if [ $# -gt 0 ]; then
    new_version="${1#v}"
    if ! [[ "$new_version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
        echo "error: version must be X.Y.Z (got '$1')" >&2
        exit 1
    fi
    echo "$new_version" > VERSION
    echo "VERSION → $new_version"
fi

if [ ! -f VERSION ]; then
    echo "error: VERSION file missing" >&2
    exit 1
fi

version="$(tr -d '[:space:]' < VERSION)"
if [ -z "$version" ]; then
    echo "error: VERSION file is empty" >&2
    exit 1
fi

rendered=0
while IFS= read -r tmpl; do
    out="${tmpl%.tmpl}"
    sed "s/{{VERSION}}/$version/g" "$tmpl" > "$out"
    echo "  rendered $out"
    rendered=$((rendered + 1))
done < <(find . -name '*.tmpl' -not -path './.git/*' -not -path './carol/*')

echo "built $rendered files at version $version"
