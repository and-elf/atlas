## 10. The Editor Is a Client

The Atlas Editor is an Atlas client. It is not a special execution mode. It is not a separate object model.

It uses the same:

- runtime
- generated contracts
- reflection metadata
- entity model
- resource identities
- configuration
- networking

as every other Atlas client. The editor differs from a gameplay client only by the additional capabilities it composes.

### Editor Capabilities

The editor may compose capabilities for:

- entity inspection
- property editing
- resource preview
- request execution
- server connection
- simulation observation
- debugging

These capabilities use the same public contracts available to every Atlas application.

### Editor as a Platform Consumer

The editor does not receive privileged access to application internals. Instead, it consumes the same systems exposed to all hosts:

- contracts
- reflection
- resources
- requests
- events
- replication

This ensures that editor behavior remains aligned with runtime behavior.

### Connecting to a Live Server

Because the editor is an ordinary client (above), connecting one to a live, publicly-reachable server raises the same question any other client connection does: what stops a request this client is capable of issuing — anything its composed capabilities expose — from being issued by an unauthorized party running the same open-source client against a production deployment?

Atlas answers this at the session layer, not by treating the editor as a special case (§6, Session Origin): a request an application wants restricted to trusted operators requires a session whose `SessionOrigin` is `Internal` — obtainable only through a listener the public cannot reach, never a fact a connecting client can claim for itself. This is what makes live editing of a publicly-observed server (a level designer reshaping terrain players are standing on, a game master spawning an event) possible *and* restricted at once: the server composes the capabilities the editor needs (so its mutations land on the exact world state connected players observe), while requiring an origin no internet-facing client can produce.
