## 3. Architectural Definitions

### Platform

Atlas is the complete platform. The platform consists of:

- compile-time tooling
- runtime libraries
- generated contracts
- reflection metadata
- capability libraries
- hosts

Atlas Platform defines the architecture used by all Atlas applications.

### Runtime

The Atlas Runtime is the execution environment shared by every host. It provides deterministic execution, scheduling, serialization, replication, resource management, and other reusable infrastructure.

- The runtime contains no application-specific behavior.
- The runtime is intentionally stable.
- New application behavior is introduced through capabilities rather than runtime modification.

### Tooling

Atlas Tooling performs compile-time analysis and generation, executing during the build process. Atlas tooling:

- validates capability definitions
- validates dependency graphs
- validates contracts
- validates stage ordering
- validates request signatures
- validates serialization schemas
- generates contracts
- generates reflection metadata
- generates documentation
- processes resources

Tooling produces artifacts consumed by runtime systems.

### Host

A host is a logical execution context produced by composing:

- runtime libraries
- generated contracts
- capability libraries
- application code

A host defines an execution environment with its own:

- scheduling
- authority model
- composed capabilities
- runtime state

A host is an architectural concept rather than an operating system process. Examples include:

- dedicated server
- gameplay client
- editor
- automated test runner
- command-line utility

All hosts share the same execution model.

### Capability

A capability is a compile-time composable unit of behavior. A capability may define:

- entities
- properties
- requests
- events
- resources
- systems
- stages
- jobs
- editor extensions

- Capabilities expose public contracts.
- Capabilities may depend only on lower-level capabilities.
- Capabilities do not modify the runtime.

### Contract

A contract is the generated public interface through which capabilities and hosts communicate. Contracts describe public structure rather than implementation. Contracts may include:

- identifiers
- property definitions
- request definitions
- event definitions
- serialization metadata
- reflection metadata
- documentation metadata

Contracts contain no application algorithms. Generated code implements structure, not behavior.

### Resource

A resource is an externally authored asset identified by a stable resource identity.

- Resources are resolved through generated metadata rather than hard-coded paths.
- Resources remain independent from application logic.
- Resource resolution is always scoped to the requesting host's composition. A host only ever resolves resource IDs referenced by the capabilities it composes — a server host composing no UI capabilities never encounters UI resource IDs (icons, textures, fonts), because the capabilities that reference them are never part of its composition. `atlas-resource` is the same library on every host; it simply gets asked for different things by different capability compositions.

Examples include:

- models
- textures
- materials
- configurations
- serialized data
- authored assets
