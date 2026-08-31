#!/usr/bin/env bash
# Launches the orb demo's three separate host processes (issues #277, #278):
# server-host (authoritative), client-host (observer), editor-host (issues
# move requests). As of #278 they genuinely talk over real Unix domain
# sockets (demo/orb_transport.hpp/.cpp) - server-host validates/applies each
# Move it receives and broadcasts the resulting Position back out.
#
# Requires the binaries to have been built with -DATLAS_REPLICATION_TRANSPORT=UNIX
# (UnixSocketTransport's own backend-option convention) - see demo/README.md.
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
