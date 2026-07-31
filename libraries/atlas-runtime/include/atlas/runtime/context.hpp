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

    // Registers store the same way register_property_store<T>() does
    // (get<T>()/set<T>() work identically afterward), and additionally
    // remembers how to reset it - so this Context can clear every triggered
    // property it knows about at a tick boundary (end_tick(), below) without
    // whatever drives that boundary needing to re-enumerate every triggered
    // T itself. This is the registration a triggered property (spec §20,
    // Triggered composition) uses instead of register_property_store<T>().
    template <typename T> void register_triggered_property_store(runtime::PropertyStore<T>& store) {
        register_property_store(store);
        triggered_resets_.push_back([&store] { store.reset(); });
    }

    // Resets every store registered via register_triggered_property_store<T>()
    // (never one registered via plain register_property_store<T>()), in
    // registration order - the tick-boundary clear that makes a triggered
    // property's "absent this tick" outcome (§20) actually happen without a
    // caller remembering to call reset_property<T>() by hand for each type.
    // See advance_tick(), which calls this automatically once per tick.
    void end_tick() {
        for (const auto& reset : triggered_resets_) {
            reset();
        }
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
    std::vector<std::function<void()>> triggered_resets_;
};

// The minimal tick-boundary driver issue #38 adds: runs host's scheduled
// jobs exactly like Host::run_tick() always has, then resets every
// triggered property registered with ctx via
// register_triggered_property_store<T>() (Context::end_tick()). A free
// function rather than a Host method - Host and Context are deliberately
// decoupled siblings (see atlas-runtime's README), and Context already
// depends on Host (its constructor takes a Host&), so a function needing
// both types lives here rather than forcing Host to include context.hpp
// and create a circular dependency between the two headers.
//
// Deliberately not the parallel/job-stealing scheduler spec §4 describes in
// full - Scheduler::run_tick() stays single-threaded and strictly
// sequential, unchanged. This is the smallest thing that gives Host+Context
// a genuine, single per-tick entry point, specifically so a triggered
// property's tick-boundary reset happens automatically rather than being
// left to caller discipline.
inline void advance_tick(runtime::Host& host, Context& ctx) {
    host.run_tick();
    ctx.end_tick();
}

} // namespace atlas
