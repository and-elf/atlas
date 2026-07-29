#!/usr/bin/env bash
# Runs clang-tidy over only the .cpp files that changed between two refs.
# Checking every tracked source file on every push/CI run doesn't scale as
# the codebase grows — this is the single scoping rule shared by
# .githooks/pre-push (local) and .github/workflows/ci.yml's static-analysis
# job (CI), so both enforce exactly the same thing.
#
# Usage: clang-tidy-changed-files.sh <compile-commands-dir> <base-ref> <head-ref>
set -euo pipefail

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <compile-commands-dir> <base-ref> <head-ref>" >&2
    exit 2
fi

compile_db="$1"
base_ref="$2"
head_ref="$3"

repo_root="$(git rev-parse --show-toplevel)"
cd "${repo_root}"

mapfile -t changed_files < <(git diff --name-only --diff-filter=ACMR "${base_ref}" "${head_ref}" -- \
    'libraries/**/*.cpp' 'tests/**/*.cpp' 'tools/**/*.cpp')

if [ "${#changed_files[@]}" -eq 0 ]; then
    echo "clang-tidy: no changed .cpp files between ${base_ref} and ${head_ref} — nothing to check."
    exit 0
fi

echo "clang-tidy: checking ${#changed_files[@]} changed file(s) against ${compile_db}/compile_commands.json..."

status=0
for file in "${changed_files[@]}"; do
    # A file changed in the range but deleted by ${head_ref} has nothing to check.
    [ -f "${file}" ] || continue
    if ! clang-tidy -p "${compile_db}" --warnings-as-errors='*' "${file}"; then
        status=1
    fi
done

exit "${status}"
