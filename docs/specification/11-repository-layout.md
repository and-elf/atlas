## 11. Repository Layout

Atlas is intended to be included directly into a project. A typical project structure is:

```
MyGame/
├── CMakeLists.txt
├── atlas.project
├── external/
│   └── atlas/
├── modules/
├── src/
├── resources/
├── config/
├── tests/
└── generated/
```

Atlas is commonly added as:

- a git submodule
- a source dependency
- an equivalent package-managed dependency

Atlas requires no global installation. The application owns the final build environment.

### Curated Capability Repositories

Atlas maintains a curated set of capabilities, distributed as separate repositories, that developers may optionally depend on — for example, a targeting/area-effect capability built on Property and Resource Composition (§20), or common gameplay, UI, and utility capabilities.

A curated capability repository is architecturally an ordinary capability library (§13, Library Architecture). It follows the same rules as any other capability:

- it is added to a project the same way (git submodule, source dependency, or equivalent — see above)
- its dependencies follow the same layering rules (§5, Dependency Model)
- it is composed at compile time, like any other capability (§4)
- it is versioned and compatibility-checked the same way any generated contract is (§6, Contract Version Enforcement)

"Curated" describes provenance and maintenance — Atlas-authored, reviewed, and kept compatible across Atlas versions — not a distinct architectural category. A curated capability carries no special runtime privilege and is not treated differently by the runtime or tooling than a capability a developer writes themselves.

**Forking and replacement.** Because a curated capability is an ordinary dependency, a developer may fork, vendor, or fully replace it with their own implementation. A game that wants its own targeting logic can depend on its own capability instead of a curated one, or compose both side by side under different names. Nothing in the runtime or build model distinguishes a curated dependency from a project's own — replacing one is exactly as supported as swapping any other capability dependency.
