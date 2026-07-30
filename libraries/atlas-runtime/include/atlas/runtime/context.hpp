#pragma once

#include "atlas/entity/entity_ref.hpp"
#include "atlas/runtime/host.hpp"
#include "atlas/runtime/property_store.hpp"

#include <any>
#include <functional>
#include <optional>
#include <stdexcept>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace atlas {

// The typed context a capability's request handler receives (spec §21:
// `atlas::RequestResult on_request(atlas::Context& ctx, const ApplyDamage& cmd)`).
// Lives in the top-level `atlas` namespace, not `atlas::runtime` - the same
// convention EntityRef/RequestResult already follow (see their libraries'
// README "Namespace note" sections): this is cross-library vocabulary a
// capability's hand-written implementation names directly, even though the
// header is physically owned by atlas-runtime.
//
// Context itself never knows what property or event type it is coordinating
// access to (spec §2, Mechanism Over Meaning) - get<T>()/publish<T>() are
// templates operating on whatever T a capability's own contract declares.
// The type-erased maps below (std::any keyed by std::type_index) are what
// make that genuinely generic rather than hard-coded to a fixed list of
// known property/event types.
class Context {
public:
    explicit Context(runtime::Host& host) noexcept : host_(&host) {}

    // Registers the PropertyStore<T> get<T>() reads/writes through for the
    // rest of this Context's lifetime. A capability composition (today:
    // hand-wired by whoever assembles a test/host; eventually: manifest-
    // driven) owns the actual PropertyStore instances and registers each one
    // it wants reachable through this context - Context borrows a reference,
    // never owns one, so store lifetime/ownership stays exactly where the
    // composing code already decided it should be.
    template <typename T> void register_property_store(runtime::PropertyStore<T>& store) {
        property_stores_[std::type_index(typeid(T))] = &store;
    }

    // ctx.get<Health>(cmd.target) in §21's worked example. Returns nullopt
    // if the entity has no stored value for T (an ordinary, expected
    // outcome a handler is meant to branch on - see §21's
    // `.or_else([&] { return atlas::reject(...); })`). Throws
    // std::logic_error if no PropertyStore<T> was ever registered - that is
    // a setup mistake (a capability composed without the property store its
    // own contract depends on), not a per-entity absence, and deserves a
    // different, louder failure mode than a quiet nullopt would give it.
    template <typename T> [[nodiscard]] std::optional<std::reference_wrapper<T>> get(EntityRef entity) {
        const auto it = property_stores_.find(std::type_index(typeid(T)));
        if (it == property_stores_.end()) {
            throw std::logic_error("atlas::Context::get: no PropertyStore registered for this type");
        }
        return std::any_cast<runtime::PropertyStore<T>*>(it->second)->get(entity);
    }

    // Creates or overwrites entity's stored value for T. Unlike get<T>(),
    // which only ever hands back a reference into an entry that already
    // exists, set<T>() is how a value is written the first time - the shape
    // a triggered property's producer needs (spec §20, Triggered
    // composition): there is no pre-existing entry for an occurrence to
    // mutate through, only a same-tick write. Throws std::logic_error if no
    // PropertyStore<T> was registered, the same setup-mistake case get<T>()
    // already guards against.
    template <typename T> void set(EntityRef entity, T value) {
        const auto it = property_stores_.find(std::type_index(typeid(T)));
        if (it == property_stores_.end()) {
            throw std::logic_error("atlas::Context::set: no PropertyStore registered for this type");
        }
        std::any_cast<runtime::PropertyStore<T>*>(it->second)->set(entity, std::move(value));
    }

    // Clears every entity's stored value for T - the tick-boundary-clear a
    // triggered property needs (spec §20, Triggered composition) so an
    // occurrence written this tick is absent again once the next tick
    // begins. Throws std::logic_error under the same setup-mistake
    // condition as get<T>()/set<T>().
    template <typename T> void reset_property() {
        const auto it = property_stores_.find(std::type_index(typeid(T)));
        if (it == property_stores_.end()) {
            throw std::logic_error(
                "atlas::Context::reset_property: no PropertyStore registered for this type");
        }
        std::any_cast<runtime::PropertyStore<T>*>(it->second)->reset();
    }

    // ctx.publish<HealthChanged>({...}) in §21's worked example. Invokes
    // every handler subscribed to T, synchronously, in registration order
    // (never unordered iteration over subscribers - spec §4, Deterministic
    // Execution). Publishing an event nobody subscribed to is an ordinary,
    // harmless case, not an error - unlike get<T>() with an unregistered
    // store, there is no setup mistake implied by "nothing is listening yet".
    template <typename T> void publish(T event) {
        const auto it = subscribers_.find(std::type_index(typeid(T)));
        if (it == subscribers_.end()) {
            return;
        }
        for (const auto& handler :
             std::any_cast<const std::vector<std::function<void(const T&)>>&>(it->second)) {
            handler(event);
        }
    }

    // Registers a handler invoked on every subsequent publish<T>(). Lazily
    // creates this Context's subscriber list for T on first use - a
    // capability never needs to pre-declare which event types it might
    // subscribe to.
    template <typename T> void subscribe(std::function<void(const T&)> handler) {
        auto& slot = subscribers_[std::type_index(typeid(T))];
        if (!slot.has_value()) {
            slot = std::vector<std::function<void(const T&)>>{};
        }
        std::any_cast<std::vector<std::function<void(const T&)>>&>(slot).push_back(std::move(handler));
    }

    [[nodiscard]] runtime::Host& host() noexcept { return *host_; }
    [[nodiscard]] const runtime::Host& host() const noexcept { return *host_; }

private:
    // A pointer, not a reference member (cppcoreguidelines-avoid-const-or-ref-data-members) -
    // functionally a non-owning, never-rebound back-reference to the Host
    // this Context coordinates access for, same reasoning as this repo's
    // other const-or-ref-data-members fixes (e.g. atlas-cgen's Block).
    runtime::Host* host_;
    std::unordered_map<std::type_index, std::any> property_stores_;
    std::unordered_map<std::type_index, std::any> subscribers_;
};

} // namespace atlas
