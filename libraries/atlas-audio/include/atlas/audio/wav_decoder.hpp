#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace atlas::audio {

// This library's canonical PCM format (issue #55/#163: "a canonical PCM
// format requirement... no resampling/format conversion" - a real, explicit
// algorithmic choice, not silently assumed). Every WAV asset atlas-audio
// decodes must already be authored in exactly this format; anything else is
// rejected outright rather than resampled or reinterpreted. Mono, not
// stereo: ResolvedCue/TriggeredCue already carry a `pan` field
// (sound_renderer.hpp) whose entire purpose is spreading a single source
// across stereo output - a pre-mixed stereo asset has no well-defined
// single answer for what pan should do to it, so this round only accepts
// the shape pan already assumes. 48 kHz is the de facto standard
// game-audio sample rate; 16-bit signed PCM is simple to hand-decode
// (matching this project's "hand-roll it when the format is trivial"
// precedent, e.g. FNV-1a) - this is the explicit decision #55 asks for,
// not a hypothetical wider set of supported rates/depths/channel counts.
inline constexpr std::uint32_t canonical_sample_rate = 48000;
inline constexpr std::uint16_t canonical_channels = 1;
inline constexpr std::uint16_t canonical_bits_per_sample = 16;

enum class WavDecodeStatus : std::uint8_t {
    Ok,
    // Not a well-formed WAV container at all - missing/truncated RIFF or
    // WAVE header, missing fmt or data chunk, or a chunk claiming more
    // bytes than are actually present. A malformed asset is an ordinary
    // runtime condition (a bad export, a corrupted download), not a
    // programmer error - reported as a status, matching
    // atlas::resource::Resolution's own convention, never thrown.
    Malformed,
    // A structurally valid WAV that simply isn't in this library's
    // canonical format (wrong sample rate, channel count, bit depth, or a
    // non-PCM audio_format code) - distinct from Malformed so a caller (or
    // a future diagnostic) can tell "this file is broken" apart from "this
    // file is fine, just not the format we require."
    UnsupportedFormat,
};

// One decoded clip's raw PCM samples, always in this library's canonical
// format (sample_rate/channels are reported rather than hard-coded purely
// so a caller can assert on them directly, even though today they can only
// ever hold the canonical constants above - decode_wav() rejects anything
// else via UnsupportedFormat before this type is ever populated with a
// mismatched value).
//
// A basic aggregate (rule of zero): no invariant beyond its plain fields.
struct DecodedClip {
    std::uint32_t sample_rate = 0;
    std::uint16_t channels = 0;
    std::vector<std::int16_t> samples;

    friend bool operator==(const DecodedClip&, const DecodedClip&) = default;
};

struct WavDecodeResult {
    WavDecodeStatus status = WavDecodeStatus::Malformed;
    DecodedClip clip;
};

// Decodes a WAV file's raw bytes (as already resolved by
// atlas::resource::ResourceRegistry - this function never touches a
// filesystem or ResourceId itself) into this library's canonical PCM
// format. Walks the RIFF chunk structure explicitly (skipping any chunk
// that isn't "fmt " or "data", honoring the format's own odd-chunk-size
// padding byte) rather than assuming "fmt " is always immediately followed
// by "data" - real encoders commonly emit "LIST"/"fact" metadata chunks in
// between.
[[nodiscard]] WavDecodeResult decode_wav(std::span<const std::byte> bytes);

} // namespace atlas::audio
