#pragma once

#include <cstdint>

namespace atlas::ui {

// Spec §19, Compositing Layers: the UI renderer composites in three fixed
// macro-layers, "always in this order": World -> HUD -> Menu. This is
// deliberately just the ordering concept - the enum and the natural integer
// ordering scoped enums already get for free (no operator<=> needed) -
// nothing about the compositing/rendering engine that would actually walk a
// node tree per layer and issue draw calls. That engine is explicitly out
// of scope for this pass (see library README: it's atlas-render's and
// later host-composition's job, §19 Backend Implementations), but even a
// minimal ordering concept is a nice-to-have this round can afford, since
// nothing here depends on the engine existing.
enum class Layer : std::uint8_t { World, Hud, Menu };

} // namespace atlas::ui
