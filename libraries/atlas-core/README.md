# atlas-core

**Status:** Seeded. Implements `atlas::core::SemanticVersion` (see `include/atlas/core/semantic_version.hpp`), a comparable major/minor/patch value type used for contract version resolution and enforcement (§6, §13). Nothing else in this library's eventual scope is implemented yet.

**Provides:** foundational types, common utilities, platform primitives.

**Spec:** [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities) (responsibility), [§5 Dependency Model](../../docs/specification/05-dependency-model.md) (dependency rules)

## Dependency position

`atlas-core` is the foundation of the dependency graph (§5): it depends on nothing else in `libraries/`, only on the standard library and the shared `atlas_project_options`/`atlas_project_warnings` interface targets. Every other library is expected to sit at or above it.
