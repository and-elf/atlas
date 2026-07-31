#include "atlas/input/scripted_raw_signal_source.hpp"

namespace atlas::input {

ScriptedRawSignalSource::ScriptedRawSignalSource(std::vector<std::vector<RawSignalEvent>> scripted_frames)
    : frames_(std::move(scripted_frames)) {}

std::vector<RawSignalEvent> ScriptedRawSignalSource::poll() {
    if (next_frame_ >= frames_.size()) {
        return {};
    }
    return frames_[next_frame_++];
}

} // namespace atlas::input
