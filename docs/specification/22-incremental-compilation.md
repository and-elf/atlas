## 22. Incremental Compilation

Atlas encourages small, reusable libraries. When application code changes:

- affected capability libraries are rebuilt
- generated contracts are regenerated when required
- affected hosts are relinked

The build system can determine the smallest affected dependency set because capabilities and contracts have explicit boundaries.

### Editor Incremental Builds

The editor is not a privileged application. The editor is another Atlas host composed from reusable libraries.

When gameplay code changes:

- gameplay libraries are rebuilt
- affected contracts are regenerated
- the gameplay client is relinked
- the editor is relinked only when affected dependencies change

Atlas runtime libraries remain unchanged. This minimizes incremental build times.

### Independent Evolution

Runtime libraries, gameplay libraries, editor libraries, and tooling evolve independently while remaining connected through stable public contracts. A change in one layer should not require rebuilding unrelated layers.
