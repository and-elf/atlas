#pragma once

#include "atlas/entity/entity_ref.hpp"
#include "atlas/resource/resource_id.hpp"
#include "atlas/runtime/context.hpp"
#include "atlas/ui/bindable_property.hpp"
#include "atlas/ui/clickable.hpp"
#include "atlas/ui/transform.hpp"

#include <optional>
#include <vector>

namespace atlas::ui {

// The UI tree's element type (spec §19, Minimum UI Contract: "Node - a
// positioned, sized element in the UI tree, with a transform and optional
// children"). A Button is not a built-in Node subtype; it is
// `Node + Text + Background + Clickable + Focusable` (§19). This library
// does not attempt that composition vocabulary yet (Text, Background,
// Focusable, ...) - only the primitives §19 calls the *minimum* contract:
// a transform, one bindable visibility flag, one bindable resource
// reference, child nodes, and (at most) the one behavior this pass
// implements. Higher-level widget vocabularies are explicitly deferred to
// future "UI Capability Package" work (§19) - see this library's README.
//
// `visible` and `resource` demonstrate the two non-tree primitives §19
// requires directly on Node itself: `visible` is the one node-level concept
// universal enough to belong on every node regardless of widget vocabulary
// (whether an element participates in presentation at all - and per §20,
// Below Presentation-Only, note that *which* nodes exist and what they
// display is exactly the kind of state Atlas is willing to model this way,
// whereas transient interaction feedback, e.g. a live drag position, is
// deliberately not represented here at all); `resource` is a single
// generic resource-reference slot standing in for icon/background/font
// (§19 lists all three as examples of the same mechanism) - a real widget
// package would likely want several independently-named resource fields
// (icon, background, font) rather than sharing one, which is exactly the
// kind of widget-specific vocabulary this pass intentionally leaves to a
// future UI Capability Package (§19) rather than guessing at now.
//
// Node identity: there is no NodeId concept yet - children are owned by
// value in `children`, and behaviors like Clickable receive whichever
// atlas::EntityRef the caller considers the interaction's owner (see
// clickable.hpp). A stable, addressable node identity (for a bridge
// capability to reference "this specific node" from outside the tree) is
// left to a future increment once a real caller needs it.
//
// A basic aggregate (rule of zero): every field is independently valid on
// its own (an empty `children` is an ordinary leaf node, an unset
// `clickable` is an ordinary non-interactive node) - there is no
// cross-field invariant a constructor would need to protect. `try_click`
// is a small helper query over the node's own fields, the same shape as
// atlas::EntityRef::is_null()/atlas::ResourceId::is_null(), not a reason to
// hide state behind accessors.
struct Node {
    Transform2D transform{};
    BindableProperty<bool> visible{.value = true};
    BindableProperty<atlas::ResourceId> resource{};
    // Both `{}`s are redundant from std::vector/std::optional's own default
    // constructors' point of view (readability-redundant-member-init), but
    // removing them makes GCC's -Wmissing-field-initializers fire at every
    // call site that designated-initializes only a subset of Node's fields
    // (e.g. `{.clickable = Clickable{}}`) - keeping them is what lets
    // partial designated initialization stay warning-free.
    std::vector<Node> children{};         // NOLINT(readability-redundant-member-init)
    std::optional<Clickable> clickable{}; // NOLINT(readability-redundant-member-init)

    // Invokes this node's Clickable behavior, if any, gating on `visible`
    // first: a hidden node cannot be clicked even if its Clickable behavior
    // is itself enabled - visibility gates interaction at the node level,
    // Clickable::enabled gates it at the behavior level, and both are
    // re-resolved against ctx every call (spec §20, Continuous
    // Re-resolution). Returns nullopt when the node has no Clickable
    // behavior at all, matching Clickable::invoke's own "disabled produces
    // no event" contract rather than treating "no behavior" as an error.
    [[nodiscard]] std::optional<ClickEvent> try_click(atlas::Context& ctx, atlas::EntityRef source) const;
};

} // namespace atlas::ui
