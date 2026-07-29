## 24. Non-Goals

Atlas intentionally does not prescribe:

- gameplay architecture
- rendering technology
- physics implementation
- ECS storage model
- scripting language *(Atlas does not prescribe a scripting language; Lua appears only as part of the optional WotLK addon compatibility capability, §19)*
- content pipeline
- game rules
- application-specific workflows

Atlas provides mechanisms for building applications. Applications remain free to choose their own semantics and implementation strategies.

Atlas intentionally does not define a UI framework in the traditional sense. It provides a minimum UI contract — a bindable property tree, resource references, input events, and composition — that any backend may render. See §19 (UI System) for the full design.

**Cross-machine UI layout synchronization is explicitly out of scope.** UI layout state (panel positions, action bar assignments, color overrides) is client-local — it is a function of which capabilities and addons are installed on that specific client, and that composition is not known to or tracked by the server. A separate capability may persist and restore layout state locally; synchronizing it across machines would require the server to track per-client UI state at a cost inconsistent with the platform's scaling goals (§7, Host Composition).
