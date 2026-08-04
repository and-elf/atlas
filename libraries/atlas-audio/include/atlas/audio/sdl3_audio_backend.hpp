#pragma once

#include "atlas/audio/audio_backend.hpp"
#include "atlas/audio/decode_cache.hpp"
#include "atlas/audio/sound_renderer.hpp"
#include "atlas/entity/entity_ref.hpp"

#include <SDL3/SDL.h>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace atlas::audio {

// Issue #205: which channel layout every device/per-voice stream in
// Sdl3AudioBackend speaks. Not "channel count 1 of N" - the equal-power pan
// law (Sdl3AudioBackend's own "Pan law decision" doc comment) is
// fundamentally a 2-channel (left/right) gain matrix that doesn't
// generalize to other channel counts without a different spatialization
// scheme entirely (real 5.1/7.1 surround needs per-speaker positioning, not
// a bigger version of the same gain matrix) - Mono is the one specific
// alternative this backend supports: no panning at all. A voice's own `pan`
// field is still accepted but silently ignored in Mono mode (this backend
// never reads it into any real stereo placement), the same "policy over
// crashing" precedent sound_renderer.hpp's clamp_gain/clamp_pan set for an
// out-of-range value, applied here to "not applicable" rather than "out of
// range."
enum class ChannelMode : std::uint8_t { Mono, Stereo };

// The full startup-configurable surface Sdl3AudioBackend's constructor
// accepts (issue #205), mirroring JoltPhysicsBackendConfig's own
// constructor-parameter precedent (issue #193) exactly: every field
// defaults to this backend's own pre-#205 hardcoded behavior, so
// `Sdl3AudioBackend backend{decode_cache}` (every existing call site, none
// of which needed to change for this issue) keeps compiling and behaving
// identically.
struct Sdl3AudioBackendConfig {
    // See ChannelMode's own doc comment above.
    ChannelMode channels = ChannelMode::Stereo;
};

// The first real (non-null) atlas::audio::AudioBackend (issue #55): one
// shared SDL3 playback device, with one SDL_AudioStream bound to it per live
// voice. Satisfies the same AudioBackend concept NullAudioBackend does - a
// caller composing a host never branches on which concrete backend it was
// handed (spec §5: "checked like a C++ concept, never a runtime interface
// table or virtual dispatch lookup").
//
// == Device format decision ==
//
// The device is opened at 48 kHz, 16-bit signed PCM, with a channel count
// resolved from Sdl3AudioBackendConfig::channels (issue #205) - Stereo (the
// default, and this backend's entire pre-#205 behavior) even though every
// decoded source clip is mono (wav_decoder.hpp's canonical_channels == 1):
// ResolvedCue/TriggeredCue's whole reason for carrying a `pan` field is
// spreading one mono source across a stereo image, which has no meaning at
// all unless the device's own output actually has two independent channels
// to place that source between - a mono device would make pan silently
// unobservable. Mono is the one other configured choice: pan is then
// silently ignored (see ChannelMode's own doc comment) and every clip's
// samples pass through unchanged.
//
// == Pan law decision ==
//
// SDL3's audio API has no pan primitive whatsoever (grepped the real fetched
// SDL_audio.h directly before this decision was made: zero matches for
// "pan"). In Stereo mode (the default), this backend hand-rolls panning as
// an **equal-power (constant-power) stereo gain matrix**, not a linear one:
//
//     angle = (pan + 1) * (pi / 4)          // maps pan in [-1, 1] to [0, pi/2]
//     left_gain  = cos(angle)
//     right_gain = sin(angle)
//
// At pan == -1 (full left): left_gain == 1, right_gain == 0. At pan == +1
// (full right): left_gain == 0, right_gain == 1. At pan == 0 (center):
// left_gain == right_gain == 1/sqrt(2) (~0.707, i.e. each channel is -3 dB) -
// left_gain^2 + right_gain^2 == 1 at every pan value, so the perceived
// *power* (not the raw linear amplitude sum) stays constant as a sound pans
// across the stereo field. A linear law (left = (1-pan)/2, right =
// (1+pan)/2) would instead dip in perceived loudness at center, since
// 0.5 + 0.5 in linear amplitude terms is not power-equivalent to 1.0 hard
// left/right - equal-power is the standard choice for this exact reason and
// is what this backend implements (sdl3_audio_backend.cpp's
// equal_power_pan_gains()).
//
// This gain matrix is baked directly into pre-computed output int16 samples
// (see to_output_samples() in the .cpp; Mono mode's own branch there simply
// copies the clip's samples through unchanged, pan ignored) - each per-voice
// SDL_AudioStream is created with an identical src_spec/dst_spec (both = the
// device's own format), so SDL performs no channel up/downmixing of its own
// that this backend would otherwise need to fight or reason about; every
// sample this backend ever queues is already in its final, fully-processed
// form by the time SDL sees it.
//
// == Gain vs. pan update mechanism (submit()'s already-present-voice path) ==
//
// Gain and pan are NOT symmetric here, and that asymmetry is deliberate:
//
// - Gain has a native, thread-safe, non-destructive SDL3 primitive
//   (SDL_SetAudioStreamGain) that scales already-queued/future audio without
//   touching queued sample data or playback position at all - so every
//   submit() call unconditionally re-applies it, cheaply, regardless of
//   whether it actually changed.
// - Pan has no such primitive (see above) - it is baked into the sample
//   bytes themselves. Changing it on an already-playing sustained voice
//   therefore requires clearing and re-queuing that voice's entire clip,
//   re-panned - SDL_ClearAudioStream() followed by a fresh
//   SDL_PutAudioStreamData() call. This is a real, documented limitation:
//   changing pan on a sustained voice restarts that voice's clip from the
//   beginning (audible as a small glitch) rather than continuing seamlessly
//   from wherever playback currently was - an acceptable trade-off for this
//   round given the explicit "short one-shot sounds only, no streaming/long-
//   form audio" scope (this library's README), where a full clip is short
//   enough that this is rarely audible in practice. To avoid paying this cost
//   on every tick a voice's *gain* alone changes, this re-queue only runs
//   when pan's value actually differs from what was last baked in
//   (tracked per-voice, see SustainedVoice::baked_pan below) - see
//   sdl3_audio_backend.cpp's submit() for the exact comparison.
//
// == Sustained-voice looping (ambiance) ==
//
// A sustained voice's clip is expected to keep sounding for as long as its
// source keeps being submitted - the real motivating cases are ambiance (a
// looping wind/room-tone bed covering a whole zone) and a channeled effect's
// hum (ResolvedCue's own doc comment, sound_renderer.hpp: "a channeled
// attack's wind-up hum"), neither of which should fall silent partway
// through just because the underlying clip is shorter than however long the
// source keeps contributing that cue. Every submit() call therefore checks
// each already-tracked voice's own SDL_GetAudioStreamQueued() and, once it
// reaches 0 (the device has fully consumed what was queued), re-queues that
// same voice's already-baked stereo samples (SustainedVoice::
// baked_output_samples below) from the start - a real, seamless-enough loop
// for this round's "full clip, no streaming" scope (true gapless/crossfaded
// looping, or looping a clip too long to fully decode into memory, is a
// separate, harder problem this backend does not attempt). trigger()'s own
// one-shot voices are deliberately excluded from this - they play once and
// are reaped on completion (see "one-shot voice pool" below), never looped.
//
// == diff-by-source contract (issue #159) ==
//
// submit() diffs this tick's ResolvedCue list against the previous one by
// `source` (ResolvedCue's own doc comment): an entity present last tick and
// absent this tick has its stream unbound and destroyed; an entity newly
// present gets a freshly created and bound stream; an entity present in both
// ticks has its existing stream updated in place (see above) - never
// recreated. voices_ below is a std::vector<SustainedVoice> (insertion-
// ordered, linearly searched by `source`), not a std::unordered_map, matching
// this project's "avoid unordered iteration...out of general code quality"
// guidance (CLAUDE.md) even though this is a presentation-only backend
// (spec §4/§20) that does not need to satisfy the strict simulation-
// determinism bar atlas-physics/capability code does.
//
// == one-shot voice pool (trigger()) ==
//
// SDL3 has no ma_engine_play_sound-equivalent automatic lifetime management
// for a fire-and-forget sound - trigger() creates one independent
// SDL_AudioStream per call (never diffed, never reused as a "same source"
// slot the way sustained voices are - two triggers from the same source are
// two independent voices), and this backend detects when a triggered
// stream's queued data has been fully drained by the device
// (SDL_GetAudioStreamQueued() reaching 0, since the src/dst formats are
// identical so nothing is ever buffered mid-conversion) via a lazy sweep at
// the start of every submit()/trigger() call - never a background thread.
//
// == Lifetime / ownership ==
//
// decode_cache must outlive this backend (mirrors DecodeCache's own "registry
// must outlive this cache" contract, threaded one level further). This
// backend never owns the DecodeCache, exactly as it never owns any
// ResourceRegistry underneath it.
//
// An encapsulated class, not a basic aggregate: it owns real OS audio
// resources (an SDL_AudioDeviceID, and one SDL_AudioStream per live voice)
// with a genuine invariant to protect - every stream must be unbound and
// destroyed, and the device closed, in the right order exactly once
// (mirrors atlas::render::Sdl3FrameBackend's own class doc comment
// reasoning).
//
// Construction can fail (no audio subsystem, no audio device available -
// SDL3's own real "dummy" driver, used in this library's own headless-CI
// tests, always succeeds) - reported by throwing std::runtime_error with
// SDL_GetError()'s message included, per CLAUDE.md's documented
// libstdc++/Clang <expected> incompatibility, matching
// Sdl3FrameBackend/Sdl3RawSignalSource's own established convention exactly.
class Sdl3AudioBackend {
public:
    // decode_cache must outlive this backend (see class doc comment above).
    // config defaults to this backend's pre-#205 hardcoded behavior
    // (Stereo) - see Sdl3AudioBackendConfig's own doc comment.
    //
    // Throws std::runtime_error, with SDL_GetError()'s message included, if
    // SDL audio subsystem initialization or opening the default playback
    // device fails.
    explicit Sdl3AudioBackend(DecodeCache& decode_cache, Sdl3AudioBackendConfig config = {});

    ~Sdl3AudioBackend();

    // Copying would require duplicating device/stream ownership, which SDL3
    // has no defined semantics for - deleted, not implemented (mirrors
    // Sdl3FrameBackend/Sdl3RawSignalSource).
    Sdl3AudioBackend(const Sdl3AudioBackend&) = delete;
    Sdl3AudioBackend& operator=(const Sdl3AudioBackend&) = delete;

    Sdl3AudioBackend(Sdl3AudioBackend&& other) noexcept;
    Sdl3AudioBackend& operator=(Sdl3AudioBackend&& other) noexcept;

    // This tick's full sustained/standing voice snapshot - diffs against the
    // previous tick's list by `source` (see class doc comment: "diff-by-
    // source contract"). A cue that fails to decode simply produces no
    // audible voice for that entity this tick - never throws, never crashes.
    void submit(std::span<const ResolvedCue> cues);

    // Fires one independent, fire-and-forget one-shot voice - see class doc
    // comment's "one-shot voice pool" section. A cue that fails to decode
    // simply produces no audible voice - never throws, never crashes.
    void trigger(const TriggeredCue& trigger_cue);

    // Test/diagnostic observability only - not part of the AudioBackend
    // concept itself. The number of currently bound sustained voices this
    // backend is tracking, so a test can verify the diff-by-source
    // start/stop contract without reaching into SDL3's own device state
    // directly.
    [[nodiscard]] std::size_t active_voice_count() const noexcept { return voices_.size(); }

    // Same reasoning, for the one-shot pool trigger() manages - reflects the
    // state as of the last submit()/trigger() call's reap sweep, not a live
    // recount (this backend never polls SDL off of those two entry points).
    [[nodiscard]] std::size_t active_one_shot_count() const noexcept { return one_shots_.size(); }

    // Test/diagnostic observability only. The live SDL_AudioStream* bound
    // for `source`'s sustained voice, or nullptr if no such voice exists -
    // exposed so a test can assert pointer identity is preserved across an
    // update (proving "updated in place," not "recreated") without any
    // other way to observe this backend's internal bookkeeping.
    [[nodiscard]] SDL_AudioStream* debug_voice_stream(EntityRef source) const noexcept;

private:
    struct SustainedVoice {
        EntityRef source;
        SDL_AudioStream* stream = nullptr;
        // The pan value actually baked into this voice's currently-queued
        // output samples - compared against each submit()'s incoming
        // ResolvedCue::effective_pan to decide whether a re-pan (and its
        // accompanying restart-from-beginning glitch, see class doc comment)
        // is actually necessary this tick. Meaningless in Mono mode (pan is
        // always ignored there), but tracked unconditionally anyway - the
        // comparison is just always false in that mode, no special-casing
        // needed.
        float baked_pan = 0.0F;
        // This voice's currently-queued output samples (already panned in
        // Stereo mode, a straight copy of the clip in Mono mode) - kept
        // around (not just queued and forgotten) so
        // refill_drained_sustained_voices() can re-queue the exact same
        // audio once the device fully drains it, without re-decoding or
        // re-panning from decode_cache_ on every loop iteration (see class
        // doc comment, "Sustained-voice looping").
        std::vector<std::int16_t> baked_output_samples;
    };

    // The stream/samples pair create_voice_stream() below produces - a voice
    // needs both (the stream to bind/queue/gain, the samples to keep for
    // looping), and trigger()'s own one-shot callers simply discard the
    // samples half since a one-shot never loops (see class doc comment).
    struct VoiceStreamResult {
        SDL_AudioStream* stream = nullptr;
        std::vector<std::int16_t> output_samples;
    };

    void destroy() noexcept;
    void reap_finished_one_shots() noexcept;
    // Re-queues any already-tracked sustained voice whose queued audio has
    // fully drained (SDL_GetAudioStreamQueued() reaching 0) from its own
    // baked_output_samples - see class doc comment, "Sustained-voice
    // looping". Never touches one_shots_.
    void refill_drained_sustained_voices() noexcept;
    // Decodes `cue` via decode_cache_ and bakes it into this backend's own
    // output format (see to_output_samples(), the .cpp - panned stereo in
    // Stereo mode, a straight copy in Mono mode where pan is ignored).
    // Returns std::nullopt - never throws - if decoding fails (see
    // submit()/trigger()'s own "don't crash" contract).
    [[nodiscard]] std::optional<std::vector<std::int16_t>> bake_output_samples(ResourceId cue, float pan);
    // Builds a fresh SDL_AudioStream for `cue` at the given gain/pan (see
    // bake_output_samples() above) and binds it to device_. Returns
    // std::nullopt - never throws - if decoding, stream creation, or binding
    // fails.
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters) - gain and pan are
    // both plain floats in ResolvedCue/TriggeredCue's own field order
    // already; renaming or reordering them here would just relocate the
    // same risk, not remove it (same reasoning
    // null_physics_backend.hpp's own NOLINT for origin/direction documents).
    [[nodiscard]] std::optional<VoiceStreamResult> create_voice_stream(ResourceId cue, float gain, float pan);

    DecodeCache* decode_cache_;
    ChannelMode channels_;
    SDL_AudioDeviceID device_ = 0;
    std::vector<SustainedVoice> voices_;
    std::vector<SDL_AudioStream*> one_shots_;
    // True only for an instance whose constructor actually completed (SDL
    // successfully initialized the audio subsystem and opened a device) -
    // false for a default-moved-from instance, so destroy() never calls
    // SDL_QuitSubSystem(SDL_INIT_AUDIO) on behalf of an instance that never
    // called SDL_InitSubSystem(SDL_INIT_AUDIO) itself (mirrors
    // Sdl3RawSignalSource's own owns_sdl_ reasoning).
    bool owns_sdl_ = false;
};

static_assert(AudioBackend<Sdl3AudioBackend>);

} // namespace atlas::audio
