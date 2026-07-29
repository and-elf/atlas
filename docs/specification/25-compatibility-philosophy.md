## 25. Compatibility Philosophy

Atlas exposes stable public interfaces. The supported integration boundary is defined by:

- contracts
- generated metadata
- public library interfaces

Implementation details remain free to evolve.

### Contract Evolution

Generated artifacts may change between Atlas versions. However, contracts should evolve in a controlled manner. Where practical:

- existing integrations remain valid
- incompatible changes are explicit
- migration paths are provided

### Implementation Freedom

Internal implementations may change without affecting applications. For example:

- scheduler internals may change
- storage mechanisms may change
- serialization implementations may change
- editor implementations may change

as long as public contracts remain compatible.
