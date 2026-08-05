#pragma once

#include "atlas/entity/entity_ref.hpp"
#include "atlas/input/intent.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/ui/bindable_property.hpp"

#include <optional>

namespace atlas::ui {

// Behavior: Clickable (spec §19, Minimum UI Contract - "small, composable
// capabilities a node may carry (Clickable, Focusable, Draggable, Tooltip,
// CooldownOverlay)"). This pass implements only Clickable, the natural
// minimal choice - the smallest behavior that demonstrates the shape any
// future behavior (Focusable, Draggable, ...) would also take: a small
// value type invoked against a Context, producing (or withholding) an
// output event.
//
// `intent` names which semantic intent a click on this node produces (spec
// §19's own AbilitySlot example: `on_click: intent: CastAbility`) - a
// structural, authored choice, the same way an InputBinding names which
// intent a raw signal produces.
//
// `enabled` is itself a BindableProperty<bool> rather than a plain bool, so
// a capability can disable a whole class of clickable nodes through
// ordinary property composition (e.g. a composed "combat lockout" property)
// rather than only through each node's own literal flag - the same
// mechanism any other bindable node property uses.
//
// A basic aggregate (rule of zero): `invoke()` is a pure query over its own
// fields plus its caller-supplied arguments - no invariant to protect, no
// private state.
struct Clickable {
    // `{}` is redundant from IntentId's own default constructor's point of
    // view (readability-redundant-member-init), but removing it makes GCC's
    // -Wmissing-field-initializers fire at every call site that designated-
    // initializes only `enabled` (e.g. `Clickable{.enabled = {...}}`) -
    // matching Node's own identical precedent for `children`/`clickable`.
    atlas::input::IntentId intent{}; // NOLINT(readability-redundant-member-init)
    BindableProperty<bool> enabled{.value = true};

    // Resolves `enabled` against ctx (spec §20, Continuous Re-resolution -
    // re-checked at invocation time, never cached) and produces the
    // configured `intent` as an atlas::input::Intent carrying `source` as
    // its entity, unless the behavior is disabled. Returns nullopt when
    // disabled - explicit rejection, matching this platform's "never
    // silently coerce" stance (spec §6, applied here to behavior invocation
    // rather than request validation) rather than firing an event a
    // disabled node shouldn't have produced. Producing atlas::input::Intent
    // directly - the same type atlas-input's IntentRouter produces from
    // hardware input - is what makes §19's "a button click and a keypress
    // are indistinguishable to the capabilities below them" literally true:
    // there is no separate atlas-ui-only click event type.
    [[nodiscard]] std::optional<atlas::input::Intent> invoke(atlas::Context& ctx,
                                                             atlas::EntityRef source) const;
};

} // namespace atlas::ui
