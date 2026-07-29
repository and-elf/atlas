## 26. Design Principle

Every Atlas host is built from reusable libraries. The editor is not a privileged application — it is another Atlas client.

Because every host shares generated contracts and runtime libraries, Atlas minimizes duplicated infrastructure while allowing each host to compose capabilities appropriate to its purpose.

Gameplay libraries, editor libraries, runtime libraries, and tooling evolve independently while remaining connected through stable public contracts.

The most significant consequence of this design is that "engine," "editor," "server," "previewer," and "test harness" stop being different applications with separate behavior and failure modes. They become different configurations of the same runtime. A bug that exists in the game exists in the preview tool; a result confirmed in the preview tool is a real result. An AI tool that edits capability data — adding a sound layer, adjusting a composition condition, modifying a property — immediately produces a result that can be previewed against the actual runtime, without a separate engine build or editor reimplementation (§9, Capability Isolation and Previewing).

### Composition Over Deployment

Atlas separates architecture from deployment.

- Composition defines behavior.
- Deployment defines location.

A host may execute:

- alone in a process
- alongside other hosts in the same process
- on another machine
- inside a testing environment

without changing its architectural identity.

A standalone multiplayer game containing both a server host and a client host is the same architectural model as a distributed server-client deployment. The difference is only where execution occurs.

### Final Principle

Atlas is a platform whose architecture is defined by:

- compile-time composition
- deterministic execution
- stable contracts
- generated reflection
- reusable capabilities
- deployment-independent hosts

Every Atlas application — whether a gameplay client, dedicated server, editor, automation tool, or embedded simulation — is built from the same foundational model.

> Atlas defines execution. Capabilities define behavior. Applications define meaning.
