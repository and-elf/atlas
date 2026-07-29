## 15. Runtime Libraries

The runtime consists of reusable libraries, shared by every Atlas host. Examples include:

- entity management
- scheduling
- serialization
- replication
- resource management
- rendering integration

A gameplay client and an editor link the same runtime libraries.

### Runtime Responsibilities

| The runtime provides | The runtime does not provide |
|---|---|
| deterministic execution | gameplay rules |
| lifecycle management | application semantics |
| system coordination | domain-specific behavior |
| resource access | |
| request execution | |
| event delivery | |
| scheduling | |

### Runtime Stability

The runtime remains intentionally stable. New functionality is introduced through:

- capabilities
- libraries
- generated contracts

rather than expanding the runtime itself. This keeps the execution foundation predictable.
