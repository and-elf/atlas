#!/usr/bin/env bash
# Launches the orb demo's three separate host processes (issue #277):
# server-host (authoritative), client-host (observer), editor-host (issues
# move requests). As of this issue none of the three actually talk to each
# other yet - that is issue #278's own scope (real transport wiring) - so
# this script's own value today is proving three genuinely separate OS
# processes exist and run concurrently, not proving replication works.
#
# Usage: demo/scripts/run_orb_demo.sh [build-dir] [--ticks N]
#   build-dir defaults to build/debug (relative to the repo root).
#   --ticks N, if given, is forwarded to all three processes (bounded
#   smoke-test mode instead of running indefinitely until Ctrl+C).
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${1:-build/debug}"
shift || true

server_bin="${repo_root}/${build_dir}/demo/server-host"
client_bin="${repo_root}/${build_dir}/demo/client-host"
editor_bin="${repo_root}/${build_dir}/demo/editor-host"

for bin in "${server_bin}" "${client_bin}" "${editor_bin}"; do
    if [ ! -x "${bin}" ]; then
        echo "run_orb_demo.sh: '${bin}' not found or not executable - build it first" \
            "(cmake --build ${build_dir} --target server-host client-host editor-host)" >&2
        exit 1
    fi
done

pids=()
cleanup() {
    for pid in "${pids[@]}"; do
        kill "${pid}" 2>/dev/null || true
    done
}
trap cleanup EXIT INT TERM

"${server_bin}" "$@" &
pids+=("$!")
"${client_bin}" "$@" &
pids+=("$!")
"${editor_bin}" "$@" &
pids+=("$!")

wait "${pids[@]}"
