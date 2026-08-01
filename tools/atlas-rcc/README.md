# atlas-rcc

All Atlas platform tools are prefixed `atlas-` (library naming convention extended to tools), with the tool's C++ namespace matching the suffix (`atlas::rcc`, mirroring `atlas::cgen`/`atlas::core`).

**Status:** Seeded (issue #33), plus a real packing step (issue #66). Given a resource manifest YAML (a `resources:` list of `name`/`type`/`path` entries), parses and validates it, then compiles it into an in-memory table of `atlas::ResourceId` (spec §3, Resource: "resources are resolved through generated metadata rather than hard-coded paths") plus the type/path resolution data a downstream host needs. Additionally now packs a compiled entry list's real asset files into a self-describing binary blob (`pack_resource_blob`) that `atlas::resource::ResourceRegistry` loads at runtime — see "What's implemented" below. Does not yet emit a generated C++ header or write blob files to disk as part of a build step — see "Deliberately deferred" below.

**Provides:** resource manifest parsing/validation, in-memory `ResourceId`-keyed resource compilation, and packing compiled entries' real asset bytes into a loadable blob (spec §12's "resource compilation" tooling responsibility).

**Spec:** [§12 Build Model](../../docs/specification/12-build-model.md) ("resource compilation" as a distinct Atlas Tooling responsibility from contract generation), [§3 Architectural Definitions](../../docs/specification/03-architectural-definitions.md) (Resource: stable identity, never a hard-coded path, scoped to what the composing host actually references), CLAUDE.md's "Naming convention" paragraph (`atlas-rcc`, turning authored asset lists into compiled `ResourceId` tables)

## Scope

Deliberately narrow, matching this project's established discipline of shipping one minimal real slice rather than the eventual full scope (the same discipline `tools/atlas-cgen/README.md` documents for its own first round):

- **Manifest shape.** A resource manifest is a single top-level `resources:` sequence, each entry a mapping of exactly three required fields:

  ```yaml
  resources:
    - name: characters/hero/mesh
      type: Mesh
      path: characters/hero/mesh.fbx
    - name: characters/hero/texture
      type: Texture
      path: characters/hero/diffuse.png
  ```

  `name` is the resource's stable identity — what `atlas::ResourceId::from_name` hashes — never derived from `path`. `type` is an open-ended asset-kind tag (`Mesh`, `Texture`, `Sound`, ...); unlike `atlas-cgen`'s closed field-type table (which maps a fixed set of C++ vocabulary types), a resource `type` is downstream, capability-authored vocabulary this tool has no reason to constrain to a fixed list. `path` is carried through exactly as authored — this tool never resolves, normalizes, or checks it against the filesystem (see "Deliberately deferred").

- **Validation** (`atlas::rcc::parse_resource_manifest`, `include/atlas/rcc/resource_manifest.hpp` / `src/resource_manifest.cpp`): rejects, with a specific `std::invalid_argument` message naming exactly what was wrong, rather than guessing at intent —
  - malformed YAML (a `YAML::Exception` wrapped with context, same convention as `atlas-cgen`'s `parse_manifest`)
  - a non-mapping document root
  - a missing or non-sequence `resources:` key
  - a non-mapping entry
  - an entry missing `name`/`type`/`path`, or carrying an empty value for one
  - an entry carrying any field beyond the three recognized ones
  - two entries declaring the same `name` (resource identity must be unique across a manifest — a duplicate would either silently overwrite or silently alias in the compiled table, either of which is a real correctness bug, not a warning)

  An empty `resources: []` list parses successfully as zero entries — a manifest that legitimately declares no resources yet is not itself an error.

- **Compilation** (`atlas::rcc::compile_resource_table`, `include/atlas/rcc/resource_table.hpp` / `src/resource_table.cpp`): turns a validated `std::vector<ResourceEntry>` into a `ResourceTable` (`std::unordered_map<atlas::ResourceId, CompiledResource>`), keyed by `atlas::ResourceId::from_name(entry.name)` — the same id a running host would compute to look a resource up. `CompiledResource` is a plain aggregate (Rule of Zero) carrying the id back out alongside `name`/`type`/`path`, since the id itself doesn't reproduce the name it was derived from.

- **`atlas::rcc::pack_resource_blob`** (`include/atlas/rcc/resource_blob.hpp` / `src/resource_blob.cpp`, issue #66) — the first place this tool touches real asset bytes rather than just carrying `path` through as an opaque string. Given a list of already-compiled `CompiledResource` entries and an asset root directory, reads each entry's real file from `asset_root / entry.path` and packs them into one self-describing binary blob: `u64 entry_count`, then that many `{u64 id; u64 offset; u64 size}` triples, then the concatenated data section. Throws `std::invalid_argument` if an entry's file can't be read — a build-time packaging step over trusted local input, matching the existing throw-on-malformed-input convention (distinct from `atlas::resource::ResourceRegistry`'s runtime return-value convention for the same class of failure at load time, since a missing asset *during a build* is an authoring error, not an ordinary runtime condition). Callers decide how entries are partitioned (typically one blob per asset type) — the function itself is agnostic to that and just packs whatever list it's handed. Integers are written host-native (`u64`); see `atlas-resource`'s README for why that's a stated, not silent, little-endian assumption.

- **`main.cpp` (the CLI)** is a thin wrapper: read the manifest file, call `parse_resource_manifest` then `compile_resource_table`, and print a summary (resource count, then one `id / type / name -> path` line per entry, `id` in hex) to stdout on success, or a `std::invalid_argument`'s message to stderr on failure. Matching `atlas-cgen`'s convention, `main.cpp` is excluded from the coverage gate (`cmake/CodeCoverage.cmake`'s existing `tools/[^/]+/src/main.cpp` glob already covers it without modification) — its argv/file-I/O error paths need subprocess-spawning test infrastructure this project doesn't build yet. The library logic it wires together (`parse_resource_manifest`, `compile_resource_table`, `pack_resource_blob`) is fully unit-tested in `tests/atlas-rcc/`. The CLI does not yet call `pack_resource_blob` or write `.blob` files to disk — see "Deliberately deferred" below.

## Deliberately deferred (not corner-cutting — explicitly out of this round's scope)

- **No generated C++ header is emitted.** The issue's acceptance criteria treat the in-memory, fully-unit-tested `ResourceTable` as this round's real deliverable — proving the compile-time-identity mechanism (parse → validate → hash → table) — rather than also committing to a generated-file shape (constexpr array? `#include`-able table? one header per manifest, like `atlas-cgen`, or one aggregated table across a whole host's referenced resources?) before a real consumer exists to drive that design. `atlas-cgen`'s own template-embedding machinery (`cmake/scripts/EmbedTextFile.cmake`, `templates/*.tmpl`, `template_engine.hpp`) is the natural pattern to reuse once this is picked up — deliberately not duplicated speculatively here.
- **No CLI wiring for blob packing either.** `pack_resource_blob` is real, tested library logic, but nothing yet writes its output to a `.blob` file on disk as part of a build step, or wires it into `main.cpp`. Same reasoning as the point above: proving the mechanism first, committing to a CLI surface once a real consumer exists to drive its shape.
- **No integration into the build-time tooling pipeline.** Spec §12 describes resource compilation running as part of the same pre-compilation tooling step contract generation does (`Src → Tooling → Validate → Gen → Compile`); this round is a standalone CLI + library, not yet wired into a CMake custom-command step the way `atlas-cgen`'s `cmake/GenerateCapabilityContract.cmake` / `atlas_generate_capability_contract` helper wires that tool into `tests/atlas-cgen`'s and `tests/atlas-contracts`'s builds. That wiring is a natural next step once a real consumer (a capability manifest's own resource references, or a host manifest's composed resource set) exists to drive it.
- **No project-wide/host-scoped resource registry.** Spec §3 describes a resource as "scoped to what the composing host actually references," implying resource compilation is eventually driven by a host's actual manifest-declared references, the same way `atlas-cgen`'s host composition mode resolves a `composes:` list against real capability manifests. This round compiles one manifest at a time, standalone — there is no equivalent of `atlas-cgen --host` yet.
- **`compile_resource_table`/`parse_resource_manifest` still do no path resolution or existence checking.** `path` stays an opaque authored string as far as those two functions are concerned; `pack_resource_blob` is what actually reads real files now, and only once a caller explicitly asks it to pack a given entry list — identity compilation itself remains untouched by that.
- **No hash-collision detection between distinct names.** `compile_resource_table` does not check whether two different (already-validated-as-non-duplicate) resource names happen to produce the same `atlas::ResourceId` (FNV-1a 64-bit) — astronomically unlikely at any realistic manifest size, and not exercisable by a real test case without deliberately searching for an FNV-1a collision pair, which felt like manufacturing a test rather than proving real behavior. Flagged here rather than silently assumed impossible.

## Real discoveries made building this

- **`FetchContent_Declare(yaml-cpp SYSTEM ...)` is duplicated from `tools/atlas-cgen/CMakeLists.txt` verbatim (same `GIT_TAG 0.8.0`), not shared via a common CMake include.** `FetchContent_Declare`/`_MakeAvailable` are idempotent per dependency name — declaring `yaml-cpp` a second time with identical arguments across two sibling `tools/` subdirectories does not re-download or conflict, confirmed by a clean `cmake --preset debug` configure with both tools present. This keeps `atlas-rcc` free of a build-ordering dependency on `atlas-cgen`'s own `CMakeLists.txt` existing or running first, matching this project's stated preference for tools/libraries with clear, independent dependency positions (§5).
- **A `type` field is open-ended vocabulary, unlike `atlas-cgen`'s closed field-type table.** `atlas-cgen` deliberately maps only a small, fixed set of manifest type tokens to C++ types (`map_field_type`) because it must know how to *emit* each one. `atlas-rcc`'s `type` is just downstream data carried through to whatever consumes the compiled table — there is no equivalent closed set to validate against here, and inventing one for this round would be exactly the "guessing at intent" this project's error-handling convention warns against.
- **Per-file branch coverage on `resource_table.cpp` reads low (`gcovr`'s per-file HTML report) even though the project-wide gate passes comfortably.** The `table.emplace(...)` call and the function's closing brace show partial/uncovered branch coverage — this is the same class of compiler-generated exception-unwind ("what if `std::string`/`unordered_map` allocation throws") edge `tools/atlas-cgen/README.md` already documents finding and excluding project-wide via `gcovr --exclude-throw-branches` (`cmake/CodeCoverage.cmake`), not a genuinely-unexercised code path — every line this file actually executes is covered, confirmed by its 100% line coverage.

## Dependency position

Depends on `yaml-cpp` (fetched, `SYSTEM`) for manifest parsing, and `atlas::resource` (for `atlas::ResourceId`/`ResourceId::from_name`) — the only `atlas-*` library dependency this tool needs, since it compiles *data* for that vocabulary type rather than generating C++ representations of vocabulary types the way `atlas-cgen` does. Does not depend on `atlas-cgen`, and `atlas-cgen` does not depend on this: per `tools/atlas-cgen/README.md`'s own "Contract generation vs. resource compilation" section, one emits contract *shape*, the other emits resource *data* — genuinely separate compile-time responsibilities (spec §12), not two modes of one tool.

## Open questions for a human reviewer

- **Generated-header shape.** Whichever future round adds file emission needs to settle: one header per manifest (mirroring `atlas-cgen`'s single-capability mode) vs. one aggregated table per host (mirroring `atlas-cgen`'s host composition mode) vs. both. This round's `ResourceTable` type is agnostic to that choice — it's an in-memory value, not tied to either shape yet.
- **`type` as a free-form string vs. a validated enum.** Left fully open-ended in this round (see "Real discoveries" above). If gameplay-facing tooling later wants compile-time or load-time confidence that `type` names a resource kind some consumer actually knows how to load, that validation would need its own closed vocabulary — deliberately not invented speculatively here.
- **Host-scoped compilation.** Spec §3's "scoped to what the composing host actually references" is not yet addressed at all — this round only ever compiles one manifest, standalone. Whether host-scoping becomes a second CLI mode (`atlas-rcc --host`, mirroring `atlas-cgen --host`) or a different mechanism entirely is left for whoever picks up the build-pipeline-integration follow-up.
- **CLI/build-step wiring for `pack_resource_blob`.** Which entries go into which blob (one per `type`? something finer?), where blob files land on disk, and how a build step invokes packing are all still open — this round proves the packing mechanism itself, not its build-pipeline surface.
