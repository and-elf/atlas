#pragma once

#include "atlas/contracts/contract_concepts.hpp"
#include "atlas/request/request_result.hpp"
#include "atlas/runtime/context.hpp"

#include <functional>
#include <optional>
#include <stdexcept>

namespace atlas::request {

// Routes a request of type T to its registered handler - atlas-request's
// own named responsibility, "request routing" (spec §13), and the piece
// this library's own README previously named as not-yet-implemented ("actual
// request dispatch/routing from a network or capability boundary to a
// handler"). A handler receives the same (Context&, const T&) signature
// spec §21's worked example shows (`on_request(atlas::Context& ctx, const
// ApplyDamage& cmd)`).
//
// One Dispatcher<T> per request type - deliberately not a single registry
// spanning every request type a host might handle. That would need type
// erasure this simple, single-purpose mechanism doesn't need yet: a real
// host composing many request types can hold one Dispatcher<T> per type,
// the same way it holds one atlas::runtime::PropertyStore<T> per property
// type.
template <RequestContract T> class Dispatcher {
public:
    using Handler = std::function<RequestResult(Context&, const T&)>;

    // Only one handler per request type - dispatch is "route to THE
    // handler," not fan-out to many; a request has exactly one authoritative
    // outcome, so registering a second handler replaces the first rather
    // than accumulating a list.
    void register_handler(Handler handler) { handler_ = std::move(handler); }

    // Throws std::logic_error if no handler was ever registered - a setup
    // mistake (a request type composed with no capability to handle it),
    // not an ordinary request-validation outcome. A *registered* handler
    // reports validation outcomes through atlas::accept/atlas::reject, not
    // through this exception.
    [[nodiscard]] RequestResult dispatch(Context& ctx, const T& request) const {
        if (!handler_) {
            throw std::logic_error("atlas::request::Dispatcher: no handler registered for this request type");
        }
        return (*handler_)(ctx, request);
    }

private:
    std::optional<Handler> handler_;
};

} // namespace atlas::request
