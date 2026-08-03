#include "atlas/audio/sdl3_audio_backend.hpp"

#include "atlas/audio/wav_decoder.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>

namespace atlas::audio {

namespace {

// The device (and every per-voice stream) always speaks stereo 16-bit
// signed PCM at this library's canonical sample rate - see
// sdl3_audio_backend.hpp's class doc comment ("Device format decision") for
// why stereo, even though every decoded source clip is mono.
constexpr int device_channel_count = 2;

[[nodiscard]] SDL_AudioSpec stereo_canonical_spec() noexcept {
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_S16;
    spec.channels = device_channel_count;
    spec.freq = static_cast<int>(canonical_sample_rate);
    return spec;
}

struct StereoGains {
    float left;
    float right;
};

// Equal-power (constant-power) pan law - see sdl3_audio_backend.hpp's class
// doc comment ("Pan law decision") for the full reasoning and formula.
[[nodiscard]] StereoGains equal_power_pan_gains(float pan) noexcept {
    constexpr float quarter_pi = 0.785398163F; // pi / 4
    const float angle = (clamp_pan(pan) + 1.0F) * quarter_pi;
    return StereoGains{.left = std::cos(angle), .right = std::sin(angle)};
}

[[nodiscard]] std::int16_t saturate_to_int16(float value) noexcept {
    constexpr float max_value = 32767.0F;
    constexpr float min_value = -32768.0F;
    if (value >= max_value) {
        return static_cast<std::int16_t>(max_value);
    }
    if (value <= min_value) {
        return static_cast<std::int16_t>(min_value);
    }
    return static_cast<std::int16_t>(std::lround(value));
}

// Builds a stereo interleaved int16 buffer from `mono`, with the equal-power
// pan law baked directly into each sample (sdl3_audio_backend.hpp: "this gain
// matrix is baked directly into pre-computed stereo int16 samples"). Overall
// linear gain is NOT applied here - see the header's "Gain vs. pan update
// mechanism" section for why gain is instead always applied via the native,
// non-destructive SDL_SetAudioStreamGain.
[[nodiscard]] std::vector<std::int16_t> pan_to_stereo(std::span<const std::int16_t> mono, float pan) {
    const StereoGains gains = equal_power_pan_gains(pan);
    std::vector<std::int16_t> stereo;
    stereo.reserve(mono.size() * 2);
    for (const std::int16_t sample : mono) {
        stereo.push_back(saturate_to_int16(static_cast<float>(sample) * gains.left));
        stereo.push_back(saturate_to_int16(static_cast<float>(sample) * gains.right));
    }
    return stereo;
}

} // namespace

Sdl3AudioBackend::Sdl3AudioBackend(DecodeCache& decode_cache) : decode_cache_(&decode_cache) {
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        throw std::runtime_error(std::string("SDL_InitSubSystem(SDL_INIT_AUDIO) failed: ") + SDL_GetError());
    }
    owns_sdl_ = true;

    const SDL_AudioSpec device_spec = stereo_canonical_spec();
    device_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &device_spec);
    if (device_ == 0) {
        const std::string error = SDL_GetError();
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        owns_sdl_ = false;
        throw std::runtime_error("SDL_OpenAudioDevice failed: " + error);
    }
}

Sdl3AudioBackend::~Sdl3AudioBackend() {
    destroy();
}

Sdl3AudioBackend::Sdl3AudioBackend(Sdl3AudioBackend&& other) noexcept
    : decode_cache_(std::exchange(other.decode_cache_, nullptr)),
      device_(std::exchange(other.device_, SDL_AudioDeviceID{0})),
      voices_(std::exchange(other.voices_, {})),
      one_shots_(std::exchange(other.one_shots_, {})),
      owns_sdl_(std::exchange(other.owns_sdl_, false)) {}

Sdl3AudioBackend& Sdl3AudioBackend::operator=(Sdl3AudioBackend&& other) noexcept {
    if (this != &other) {
        destroy();
        decode_cache_ = std::exchange(other.decode_cache_, nullptr);
        device_ = std::exchange(other.device_, SDL_AudioDeviceID{0});
        voices_ = std::exchange(other.voices_, {});
        one_shots_ = std::exchange(other.one_shots_, {});
        owns_sdl_ = std::exchange(other.owns_sdl_, false);
    }
    return *this;
}

void Sdl3AudioBackend::destroy() noexcept {
    if (!owns_sdl_) {
        return;
    }

    for (const SustainedVoice& voice : voices_) {
        SDL_UnbindAudioStream(voice.stream);
        SDL_DestroyAudioStream(voice.stream);
    }
    voices_.clear();

    for (SDL_AudioStream* stream : one_shots_) {
        SDL_UnbindAudioStream(stream);
        SDL_DestroyAudioStream(stream);
    }
    one_shots_.clear();

    if (device_ != 0) {
        SDL_CloseAudioDevice(device_);
        device_ = 0;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    owns_sdl_ = false;
}

// See the declaration's own NOLINT comment (sdl3_audio_backend.hpp) for why gain/pan aren't
// restructured to silence bugprone-easily-swappable-parameters.
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
SDL_AudioStream* Sdl3AudioBackend::create_voice_stream(ResourceId cue, float gain, float pan) {
    const DecodeCacheResult& decoded = decode_cache_->get_or_decode(cue);
    if (decoded.status != DecodeCacheStatus::Ok) {
        // A cue that fails to decode simply produces no audible voice (class
        // doc comment / submit()'s and trigger()'s own "don't crash, don't
        // throw" contract) - never a placeholder, never a retry loop beyond
        // whatever the next call naturally attempts again.
        return nullptr;
    }

    const SDL_AudioSpec spec = stereo_canonical_spec();
    SDL_AudioStream* stream = SDL_CreateAudioStream(&spec, &spec);
    if (stream == nullptr) {
        return nullptr;
    }

    const std::vector<std::int16_t> stereo_samples = pan_to_stereo(decoded.clip.samples, pan);
    SDL_SetAudioStreamGain(stream, gain);
    const auto byte_length = static_cast<int>(stereo_samples.size() * sizeof(std::int16_t));
    SDL_PutAudioStreamData(stream, stereo_samples.data(), byte_length);
    SDL_FlushAudioStream(stream);

    if (!SDL_BindAudioStream(device_, stream)) {
        SDL_DestroyAudioStream(stream);
        return nullptr;
    }

    return stream;
}

void Sdl3AudioBackend::reap_finished_one_shots() noexcept {
    std::erase_if(one_shots_, [](SDL_AudioStream* stream) {
        // Src/dst formats are identical for every stream this backend
        // creates (stereo_canonical_spec() on both ends), so nothing is ever
        // buffered mid-conversion - once every byte put into the stream has
        // been consumed by the device, SDL_GetAudioStreamQueued reports 0
        // and this one-shot voice is done. A negative (error) return is
        // treated the same as "finished" - destroying a stream SDL itself
        // can no longer report on cleanly is safer than leaking it forever.
        const bool finished = SDL_GetAudioStreamQueued(stream) <= 0;
        if (finished) {
            SDL_UnbindAudioStream(stream);
            SDL_DestroyAudioStream(stream);
        }
        return finished;
    });
}

void Sdl3AudioBackend::submit(std::span<const ResolvedCue> cues) {
    reap_finished_one_shots();

    // Stop-on-absence half of the diff-by-source contract (issue #159): any
    // previously tracked voice whose source is not present in this tick's
    // list is unbound and destroyed.
    std::erase_if(voices_, [&](const SustainedVoice& voice) {
        const bool still_present = std::any_of(
            cues.begin(), cues.end(), [&](const ResolvedCue& cue) { return cue.source == voice.source; });
        if (!still_present) {
            SDL_UnbindAudioStream(voice.stream);
            SDL_DestroyAudioStream(voice.stream);
        }
        return !still_present;
    });

    // Start-or-update half of the diff-by-source contract.
    for (const ResolvedCue& cue : cues) {
        const auto existing = std::find_if(voices_.begin(), voices_.end(), [&](const SustainedVoice& voice) {
            return voice.source == cue.source;
        });

        if (existing == voices_.end()) {
            SDL_AudioStream* stream = create_voice_stream(cue.cue, cue.effective_gain, cue.effective_pan);
            if (stream != nullptr) {
                voices_.push_back(
                    SustainedVoice{.source = cue.source, .stream = stream, .baked_pan = cue.effective_pan});
            }
            continue;
        }

        // Gain: a native, thread-safe, non-destructive SDL3 primitive -
        // always cheap to re-apply, regardless of whether it changed (see
        // header's "Gain vs. pan update mechanism").
        SDL_SetAudioStreamGain(existing->stream, cue.effective_gain);

        // Pan: no native SDL3 primitive exists, so it is baked into queued
        // sample bytes - only re-bake (and pay the resulting restart-from-
        // beginning glitch, documented in the header) when it actually
        // changed from what is currently queued.
        if (existing->baked_pan != cue.effective_pan) {
            const DecodeCacheResult& decoded = decode_cache_->get_or_decode(cue.cue);
            if (decoded.status == DecodeCacheStatus::Ok) {
                const std::vector<std::int16_t> stereo_samples =
                    pan_to_stereo(decoded.clip.samples, cue.effective_pan);
                SDL_ClearAudioStream(existing->stream);
                const auto byte_length = static_cast<int>(stereo_samples.size() * sizeof(std::int16_t));
                SDL_PutAudioStreamData(existing->stream, stereo_samples.data(), byte_length);
                SDL_FlushAudioStream(existing->stream);
            }
            existing->baked_pan = cue.effective_pan;
        }
    }
}

void Sdl3AudioBackend::trigger(const TriggeredCue& trigger_cue) {
    reap_finished_one_shots();

    SDL_AudioStream* stream = create_voice_stream(trigger_cue.cue, trigger_cue.gain, trigger_cue.pan);
    if (stream != nullptr) {
        one_shots_.push_back(stream);
    }
}

SDL_AudioStream* Sdl3AudioBackend::debug_voice_stream(EntityRef source) const noexcept {
    const auto it = std::find_if(
        voices_.begin(), voices_.end(), [&](const SustainedVoice& voice) { return voice.source == source; });
    return it == voices_.end() ? nullptr : it->stream;
}

} // namespace atlas::audio
