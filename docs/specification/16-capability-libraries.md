## 16. Capability Libraries

A capability consists of one or more libraries. Example:

```
combat/
    combat

combat-editor/
    combat-editor
```

- The gameplay application links: `combat`
- The editor links: `combat`, `combat-editor`

### Capability Independence

Capabilities remain usable without editor-specific libraries. A capability does not require:

- editor support
- visualization tools
- debugging tools
- authoring interfaces

Those are optional extensions.

### Capability Composition

Capabilities are combined during compilation. Composition determines:

- available behavior
- available contracts
- generated metadata
- host functionality

The runtime executes the resulting validated composition.
