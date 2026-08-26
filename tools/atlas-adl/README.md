# atlas-adl

All Atlas platform tools are prefixed `atlas-` (library naming convention extended to tools), with the tool's C++ namespace matching the suffix (`atlas::adl`, mirroring `atlas::rcc`/`atlas::cgen`).

**Status:** Seeded (issue #265). Given an asset/animation request YAML — the reviewable, declarative artifact this issue introduces in place of freeform prose like "make an enemy wolf that uses quadruped animations" — parses and validates it (`atlas::adl::parse_asset_request`), rejecting with a specific `std::invalid_argument` message per violation, the same convention `atlas::rcc::parse_resource_manifest` established. No Blender integration, no agent loop, and no capability-authoring automation exist yet — this round is only the schema and its validator (see "Deliberately deferred" below).

**Provides:** asset/animation request manifest parsing and validation — the same "declare structure, validate it, never guess at intent" discipline capability manifests already get from `atlas-cgen` and resource manifests get from `atlas-rcc`, applied to content-authoring requests instead.

**Spec:** [§14 Generated Contracts](../../docs/specification/14-generated-contracts.md) (The Declarative Boundary: structure and composition parameters only, never conditionals/branching — the same discipline this schema follows), [§3 Architectural Definitions](../../docs/specification/03-architectural-definitions.md) (Resource: stable identity, never a hard-coded path — `request.name` is a resource name root in the same sense)

## Scope

Deliberately narrow — schema + validator only, matching this project's established discipline of shipping one minimal real slice (the same discipline `tools/atlas-rcc/README.md` documents for its own first round):

- **One YAML file per authored request**, conventionally living under `demo/design/requests/` (e.g. `demo/design/requests/enemy_wolf.request.yaml`) — content, not code, so it lives alongside `demo/`'s other gameplay-facing authoring artifacts rather than under `tools/`.
- **`request:` block** — `kind` (closed enum: `creature | prop | weapon | environment | vfx`) and `name` (the resource name root a downstream resource manifest would key off of, spec §3). An unrecognized `kind` value is rejected outright — this is a closed vocabulary, unlike e.g. `atlas-rcc`'s open-ended resource `type` string, because a content-authoring pipeline needs to know which of a fixed set of authoring flows a request belongs to.
- **`visual:` block** — `style_ref` (a reference into the not-yet-built `demo/design/style-guide.yaml`, carried through as an opaque string this round — see "Deliberately deferred") plus an optional `overrides:` list. Every override must be *justified*: `field`/`value`/`rationale` are all required — an override with no rationale is exactly the unreviewable freeform prose this ADL replaces.
- **`rig:` block** — `type` (open-ended, like `atlas-rcc`'s resource `type` — no closed vocabulary to validate an animation rig "type" against), `skeleton_template`, and the `animation_set:` list. Each entry has a `name` and a `mode` (closed enum: `procedural_auto | human_gated`), which gates which further fields are required:
  - `procedural_auto` requires `loop` (bool) and `duration_seconds` (number) — a clip the runtime can play/loop entirely proceduraly, no human timing sign-off needed.
  - `human_gated` requires `sync_to` (a capability/event reference string, e.g. `attack_resolution.hit_confirm` — resolved against real capability manifests is explicitly out of scope this round, see below) and `contact_frame_ratio` (a number in `[0, 1]`, rejected outside that range) — a clip whose timing must line up with a specific gameplay moment, needing a human animator's judgment call on where that moment falls in the clip.
- **`composition:` block** — `intended_role`, an optional `existing_capabilities` list, and `requires_new_mechanism` (bool, default `false`). `rationale` is required if and only if `requires_new_mechanism: true` — a request claiming it needs a new mechanism must justify why no existing composition covers it; a request that doesn't claim this may still carry an (optional) `rationale`, but omitting it isn't an error either way.
- **Validation** (`atlas::adl::parse_asset_request`, `include/atlas/adl/asset_request.hpp` / `src/asset_request.cpp`): rejects, with a specific `std::invalid_argument` message naming exactly what was wrong, mirroring `atlas::rcc::parse_resource_manifest`'s conventions —
  - malformed YAML, a non-mapping document root, a missing `request:`/`visual:`/`rig:`/`composition:` block
  - any block (including the document root itself) carrying a field beyond its recognized set
  - a missing or empty required string field anywhere in the schema
  - an unrecognized `request.kind` or `animation_set[].mode` value
  - a missing `rig.animation_set` sequence, or a non-mapping entry within it
  - a `procedural_auto` entry missing `loop` or `duration_seconds`; a `human_gated` entry missing `sync_to` or `contact_frame_ratio`
  - a `contact_frame_ratio` outside `[0, 1]`
  - `composition.requires_new_mechanism: true` with no `composition.rationale`
  - a `visual.overrides[]` entry missing `field`, `value`, or `rationale`

## Deliberately deferred (not corner-cutting — explicitly out of this round's scope, per the issue)

- **No Blender bridge / MCP tool surface** (`create_primitive`, `export_native_mesh`, `render_snapshot`, etc.).
- **No Implementor/Checker agentic loop.** This tool only validates a request file a human or agent already authored — it has no opinion on who authors one or how.
- **No `sync_to` reference resolution against real capability manifests.** `sync_to` is accepted and carried through as an opaque string this round; resolving it to a live field on a real capability and enforcing the timing check it implies is separate, later work.
- **No `demo/design/style-guide.yaml` file.** `visual.style_ref` is validated only as a non-empty string — this tool has no style guide to resolve it against yet.
- **No actual asset generation, procedural or human-authored.** This tool produces a validated, in-memory `AssetRequest` — nothing downstream consumes it yet.

## Real discoveries made building this

- **`request.kind` is a closed enum, everything else content-shaped (`rig.type`, `visual.style_ref`, `sync_to`) stays a free string.** Mirrors `atlas-cgen`'s small closed field-type table vs. `atlas-rcc`'s open-ended resource `type`: a vocabulary is closed only where this tool itself needs to branch on it (`kind` picks which authoring flow applies; `mode` picks which fields are required) — everything else is downstream data this tool has no reason to constrain.
- **`FetchContent_Declare(yaml-cpp SYSTEM ...)` is duplicated verbatim from `tools/atlas-rcc/CMakeLists.txt`** (same `GIT_TAG 0.8.0`), for the same reason documented there: keeping this tool free of a build-ordering dependency on any sibling `tools/` subdirectory's `CMakeLists.txt`.

## Dependency position

Depends on `yaml-cpp` (fetched, `SYSTEM`) for manifest parsing — no other `atlas-*` library dependency, since this tool validates content-authoring *data*, not a vocabulary type any runtime/capability library defines. Does not depend on, and is not depended on by, `atlas-cgen` or `atlas-rcc` — three genuinely separate compile-time tooling responsibilities (spec §12): contract shape, resource data, and content-authoring requests.

## Open questions for a human reviewer

- **Where does `demo/design/style-guide.yaml` eventually live, and what validates a `style_ref` against it?** Left fully open this round (see "Deliberately deferred").
- **Does `sync_to` ever become a resolved reference into a real capability manifest's events/requests, checked at validation time?** Left fully open this round — `contact_frame_ratio`'s `[0, 1]` range is the only numeric check this round performs on a `human_gated` entry.
- **Should `rig.type` eventually gain a closed vocabulary once real rig templates exist?** Left open-ended for now, matching `atlas-rcc`'s own stance on resource `type`.
