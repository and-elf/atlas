## 2. Core Principles

### Stable Runtime

The runtime remains small and stable.

New functionality is added through capabilities rather than runtime expansion. The runtime provides foundational execution mechanisms while remaining independent from application-specific behavior.

### Compile-Time Composition

Capabilities are composed during compilation.

- The runtime executes a validated application graph.
- The runtime never discovers capabilities dynamically.
- Composition decisions are made before execution begins.

This allows Atlas to provide:

- predictable dependencies
- deterministic behavior
- generated contracts
- optimized builds
- fast iteration

### Reflection First

Public contracts are reflected. Reflection powers:

- networking
- serialization
- editors
- automation
- documentation
- AI tooling
- debugging

Reflection metadata is generated during the build. Reflection is treated as a foundational system rather than an optional feature.

### Mechanism Over Meaning

Atlas understands execution. Applications define semantics.

| The runtime understands | The runtime never understands |
|---|---|
| entities | players |
| properties | health |
| requests | weapons |
| events | inventories |
| stages | quests |
| jobs | game rules |
| resources | |
| scheduling | |

Those right-hand concepts belong to capabilities. Atlas provides mechanisms through which applications express meaning.
