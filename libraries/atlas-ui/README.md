# atlas-ui

**Status:** Not yet implemented.

**Provides:** UI node tree, property binding infrastructure, behavior primitives (Clickable, Focusable, etc.), compositing layer management, backend dispatch

**Spec:** [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities) (responsibility), [§5 Dependency Model](../../docs/specification/05-dependency-model.md) (dependency rules)

## Dependency position

Per §5, a library may depend only on lower-level libraries, generated contracts, and platform services — never upward on capabilities, applications, or editor/deployment-specific code. This library is optional (§13): a headless server host composes neither `atlas-input` nor `atlas-ui`. The concrete set of libraries `atlas-ui` links against isn't fixed yet; it will be established via `target_link_libraries` when implementation begins, following the responsibility above and the dependency direction in [§5](../../docs/specification/05-dependency-model.md).
