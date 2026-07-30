# atlas-audio

**Status:** Not yet implemented.

**Provides:** audio rendering — consumes composed properties and resources (game state), produces sound output. Same mechanism-not-meaning boundary as `atlas-render`: sound design and content authoring remain an application concern, not a platform one.

**Spec:** [§13 Library Architecture](../../docs/specification/13-library-architecture.md#library-responsibilities) (responsibility), [§19 UI System](../../docs/specification/19-ui-system.md) (renderer pattern), [§24 Non-Goals](../../docs/specification/24-non-goals.md) (mechanism/content boundary), [§5 Dependency Model](../../docs/specification/05-dependency-model.md) (dependency rules)

## Dependency position

Per §5, a library may depend only on lower-level libraries, generated contracts, and platform services — never upward on capabilities, applications, or editor/deployment-specific code. This library is optional (§13): a headless server host composes neither `atlas-render` nor `atlas-audio`. The concrete set of libraries `atlas-audio` links against isn't fixed yet; it will be established via `target_link_libraries` when implementation begins, following the responsibility above and the dependency direction in [§5](../../docs/specification/05-dependency-model.md).
