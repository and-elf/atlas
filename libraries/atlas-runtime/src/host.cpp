#include "atlas/runtime/host.hpp"

namespace atlas::runtime {

Host::Host(stage::StageSequence sequence) : scheduler_(std::move(sequence)) {}

EntityRef Host::create_entity() {
    return entities_.create();
}

bool Host::destroy_entity(EntityRef ref) noexcept {
    return entities_.destroy(ref);
}

bool Host::is_entity_alive(EntityRef ref) const noexcept {
    return entities_.is_alive(ref);
}

bool Host::schedule(const stage::StageId& stage_id, scheduler::Job job) {
    return scheduler_.schedule(stage_id, std::move(job));
}

void Host::run_tick() const {
    scheduler_.run_tick();
}

const stage::StageSequence& Host::sequence() const noexcept {
    return scheduler_.sequence();
}

} // namespace atlas::runtime
