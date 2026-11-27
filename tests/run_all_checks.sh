#!/usr/bin/env bash
# Every test, including the ones that read the converted game (assets/) and the
# PC version (data/, img/). The PC version is found in $NARAKU_ORIGINAL,
# original/ inside the repository or original/ next to it.
#
# KNOWN_STALE lists tests that compare assets/ with what the current
# converter produces. They fail until the map scripts are rebuilt (see
# tests/README.md) and are reported separately instead of failing the run.
set -uo pipefail
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT_DIR"
KNOWN_STALE=" test_native_actors_139 test_stage_135 test_will_stage_170 test_factory_extension "

original=""
for dir in "${NARAKU_ORIGINAL:-}" "$ROOT_DIR/original" "$ROOT_DIR/../original"; do
    if [[ -n "$dir" && -f "$dir/data/System.json" ]]; then original="$(cd "$dir" && pwd)"; break; fi
done
[[ -n "$original" ]] || { echo "PC game data not found (set NARAKU_ORIGINAL)." >&2; exit 2; }
[[ -f assets/map001.bin ]] || { echo "assets/ with the converted game is missing." >&2; exit 2; }
export NARAKU_ORIGINAL="$original"
if [[ ! -f assets/picture_v050_manifest.json || ! -f assets/portraits_v132.json ]]; then
    python3 tools/prepare_test_data.py "$original" assets
fi

bash tests/run_host_checks.sh >/dev/null 2>&1 || { echo "FAIL: tests/run_host_checks.sh"; exit 1; }
pass=0; fail=0; stale=0; fixed=""
for t in tests/test_*.py; do
    name="$(basename "$t" .py)"
    if out="$(python3 "$t" 2>&1)"; then
        pass=$((pass + 1))
        [[ "$KNOWN_STALE" == *" $name "* ]] && fixed="$fixed $name"
    elif [[ "$KNOWN_STALE" == *" $name "* ]]; then
        stale=$((stale + 1))
    else
        fail=$((fail + 1))
        echo "FAIL: $name"
        echo "$out" | tail -5 | sed 's/^/    /'
    fi
done
echo "passed: $pass, failed: $fail, known stale: $stale"
[[ -z "$fixed" ]] || echo "Now passing, remove from KNOWN_STALE:$fixed"
[[ $fail -eq 0 ]]
