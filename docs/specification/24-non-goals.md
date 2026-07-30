## 24. Non-Goals

Atlas intentionally does not prescribe:

- gameplay architecture
- rendering *technology* — Atlas does not mandate a single rendering approach; it does, however, provide `atlas-render` (§13) as one optional, reference 3D rendering *implementation*, on the same mechanism-not-meaning boundary as every other library. A game remains free to substitute a different rendering backend entirely, or none — `atlas-render` is one possible backend for the renderer contract described in §19, never the mandatory one, and games that render through it gain no capability, request, or contract that a game using a different backend lacks
- physics implementation
- ECS storage model
- scripting language *(Atlas does not prescribe a scripting language; Lua appears only as part of the optional WotLK addon compatibility capability, §19)*
- content pipeline
- game rules
- application-specific workflows

Atlas provides mechanisms for building applications. Applications remain free to choose their own semantics and implementation strategies. What Atlas provides — including `atlas-render` — is mechanism (frame output from composed state), never meaning: shader authoring, art direction, and game-specific rendering content stay an application concern, not a platform one.

Atlas intentionally does not define a UI framework in the traditional sense. It provides a minimum UI contract — a bindable property tree, resource references, input events, and composition — that any backend may render. See §19 (UI System) for the full design.

**Cross-machine UI layout synchronization is explicitly out of scope.** UI layout state (panel positions, action bar assignments, color overrides) is client-local — it is a function of which capabilities and addons are installed on that specific client, and that composition is not known to or tracked by the server. A separate capability may persist and restore layout state locally; synchronizing it across machines would require the server to track per-client UI state at a cost inconsistent with the platform's scaling goals (§7, Host Composition).
