#pragma once

#include "atlas/entity/entity_ref.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/ui/bindable_property.hpp"

#include <optional>

namespace atlas::ui {

// The output of an invoked Clickable behavior (spec §19: "It produces
// Intent events - the same Intent events atlas-input produces from
// hardware input"). atlas-input's Intent type is being designed in a
// sibling, independent worktree as of this writing, so this is
// deliberately a minimal, local stand-in rather than a guess at that
// type's eventual shape - see this library's README for the explicit
// deferred-unification note. `source` names which entity's interaction
// produced the click; there is no standing NodeId concept yet (see
// node.hpp's own doc comment), so the caller supplies whichever entity it
// considers the click's owner.
struct ClickEvent {
    atlas::EntityRef source{};
};

// Behavior: Clickable (spec §19, Minimum UI Contract - "small, composable
// capabilities a node may carry (Clickable, Focusable, Draggable, Tooltip,
// CooldownOverlay)"). This pass implements only Clickable, the natural
// minimal choice - the smallest behavior that demonstrates the shape any
// future behavior (Focusable, Draggable, ...) would also take: a small
// value type invoked against a Context, producing (or withholding) an
// output event.
//
// `enabled` is itself a BindableProperty<bool> rather than a plain bool, so
// a capability can disable a whole class of clickable nodes through
// ordinary property composition (e.g. a composed "combat lockout" property)
// rather than only through each node's own literal flag - the same
// mechanism any other bindable node property uses.
//
// A basic aggregate (rule of zero): the only field is `enabled`, and
// `invoke()` is a pure query over it plus its caller-supplied arguments -
// no invariant to protect, no private state.
struct Clickable {
    BindableProperty<bool> enabled{.value = true};

    // Resolves `enabled` against ctx (spec §20, Continuous Re-resolution -
    // re-checked at invocation time, never cached) and produces a
    // ClickEvent unless the behavior is disabled. Returns nullopt when
    // disabled - explicit rejection, matching this platform's "never
    // silently coerce" stance (spec §6, applied here to behavior invocation
    // rather than request validation) rather than firing a click event a
    // disabled node shouldn't have produced.
    [[nodiscard]] std::optional<ClickEvent> invoke(atlas::Context& ctx, atlas::EntityRef source) const;
};

} // namespace atlas::ui
