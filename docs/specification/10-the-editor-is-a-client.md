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
