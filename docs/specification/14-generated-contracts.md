## 14. Generated Contracts

Capability definitions generate strongly typed C++ contracts. Generated artifacts include:

- property types
- identifiers
- reflection metadata
- serialization metadata
- resource identifiers
- documentation metadata

Generated code defines structure. Generated code never contains application logic.

Contracts are built as `constexpr` metadata rather than data merely produced by the build and inspected at runtime. This is what makes tiny-interface contract satisfaction (§5, Tiny Interface Composability) a compile-time fact with no runtime lookup cost — the same generation step that produces a capability's contract also makes that contract's shape available to the compiler for structural checking.

### Contract Purpose

Contracts provide the stable communication boundary between:

- capabilities
- hosts
- tools
- runtime systems
- external integrations

A contract describes **what exists**. It does not define **how** behavior is implemented.

### Manual Implementation

Developers implement algorithms manually. Examples include:

- gameplay rules
- simulation algorithms
- AI behavior
- application workflows
- domain-specific systems

Atlas generates infrastructure. Applications provide meaning.

### Declarative Source Format

Capability structure — properties, requests, events, dependencies, composition strategy — is authored as data (YAML, or an equivalent structured format) rather than hand-written C++. Atlas tooling consumes this declaration and generates the corresponding `constexpr` contracts, concept constraints, and boilerplate described above.

Host composition follows the same declarative model. A host's capability list is data, the same as a capability's structure:

```yaml
host: GameplayClient
composes:
  - entity
  - health
  - health_ui_bridge
  - lua_ui_framework
  - wotlk_addon_compat
  - replication
  - networking_client
```

Atlas tooling generates the corresponding `atlas::Host<...>` composition (§21, Worked Example) from this declaration, validating it against the same dependency and cycle rules that apply to any composition (§5).

There is one tool and one generation step, used uniformly for capability structure and host composition — not a separate mechanism for each. This is a direct extension of §12 (Compile-Time Validation): the tooling that already validates dependency graphs, contract compatibility, and stage ordering consumes the declarative source as its input, rather than parsing hand-written C++ for the same information.

### The Declarative Boundary

The declarative format expresses **structure and composition** — what exists, what a capability depends on, what a host composes, how a property's contributions combine (§20, Composition Strategies). It never expresses **behavior**.

Concretely, the declarative format may express:

- properties, requests, events, and their types
- a capability's dependencies
- a host's composed capabilities
- a property's composition strategy (Additive, Priority Override, and so on — §20)
- a filter or strategy's *parameters* (e.g. a range value, a faction name, a priority ordering)

It may never express:

- conditionals, branching, or control flow
- what a request handler does when validation fails
- the algorithm behind a filter or composition strategy
- any per-tick or per-event logic

The moment a declaration needs an `if`, it has stopped describing structure and started describing behavior — and behavior is manual implementation (§14, Manual Implementation), written in C++ against the generated contract. A filter such as `MaxTargets` is declared in YAML only insofar as *which* filter and *what parameter* (`MaxTargets: 10`); the filter's own evaluation logic is a capability, written in C++, the same as `AuraCapability`'s engine would be.

This mirrors the platform's central boundary (§2, Mechanism Over Meaning) at the level of the tooling itself: Atlas — and the declarative format Atlas consumes — defines what can be composed and how contributions interact. It does not, and structurally cannot, drive logic. Logic remains entirely the developer's, expressed in C++ against the contracts the declaration generates.
