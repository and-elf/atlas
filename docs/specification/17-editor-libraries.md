## 17. Editor Libraries

Editor functionality is provided through ordinary Atlas libraries. Examples include:

- property inspector
- hierarchy view
- asset browser
- viewport
- gizmos
- debugging tools
- profiling tools

The editor is built by linking these libraries into an Atlas host.

### Editor Isolation

The gameplay client remains free of editor dependencies. Editor functionality does not exist as hidden runtime behavior. Instead:

- gameplay code provides capabilities
- editor code provides optional tooling
- contracts connect the two

This keeps runtime builds minimal.
