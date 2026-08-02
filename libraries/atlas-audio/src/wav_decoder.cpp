#include "atlas/audio/wav_decoder.hpp"

#include <array>
#include <cstring>
#include <optional>
#include <string_view>

namespace atlas::audio {

namespace {

std::uint16_t read_u16(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint16_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::uint32_t read_u32(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint32_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

std::int16_t read_i16(std::span<const std::byte> bytes, std::size_t offset) {
    std::int16_t value = 0;
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

bool matches_ascii(std::span<const std::byte> bytes, std::size_t offset, std::string_view tag) {
    for (std::size_t i = 0; i < tag.size(); ++i) {
        if (static_cast<char>(bytes[offset + i]) != tag[i]) {
            return false;
        }
    }
    return true;
}

struct FmtFields {
    std::uint16_t audio_format = 0;
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint16_t bits_per_sample = 0;
};

struct DataChunk {
    std::size_t offset = 0;
    std::size_t size = 0;
};

struct ParsedChunks {
    std::optional<FmtFields> fmt;
    std::optional<DataChunk> data;
};

constexpr std::size_t riff_header_size = 12; // "RIFF" + u32 size + "WAVE"
constexpr std::size_t chunk_header_size = 8; // 4-byte id + u32 size
constexpr std::size_t fmt_payload_min_size = 16;

// Walks every chunk after the RIFF/WAVE header, collecting "fmt " and "data"
// - anything else ("LIST", "fact", ...) is skipped over entirely, honoring
// the format's own odd-chunk-size padding byte (only consumed when a byte
// actually follows - a final odd-sized chunk with nothing after it is not,
// in practice, always given a trailing pad byte by every real encoder).
// std::nullopt: a chunk header or its declared payload runs past the end
// of `bytes` - the caller reports this as Malformed.
std::optional<ParsedChunks> walk_chunks(std::span<const std::byte> bytes) {
    ParsedChunks result;
    std::size_t offset = riff_header_size;

    while (offset + chunk_header_size <= bytes.size()) {
        const std::uint32_t chunk_size = read_u32(bytes, offset + 4);
        const std::size_t payload_start = offset + chunk_header_size;
        const auto chunk_size_sz = static_cast<std::size_t>(chunk_size);

        if (payload_start + chunk_size_sz > bytes.size()) {
            return std::nullopt; // chunk claims more bytes than are actually present
        }

        if (matches_ascii(bytes, offset, "fmt ")) {
            if (chunk_size_sz < fmt_payload_min_size) {
                return std::nullopt;
            }
            result.fmt = FmtFields{
                .audio_format = read_u16(bytes, payload_start),
                .channels = read_u16(bytes, payload_start + 2),
                .sample_rate = read_u32(bytes, payload_start + 4),
                .bits_per_sample = read_u16(bytes, payload_start + 14),
            };
        } else if (matches_ascii(bytes, offset, "data")) {
            result.data = DataChunk{payload_start, chunk_size_sz};
        }

        std::size_t next_offset = payload_start + chunk_size_sz;
        if (chunk_size_sz % 2 == 1 && next_offset < bytes.size()) {
            next_offset += 1; // consume this chunk's padding byte, per the WAV format
        }
        offset = next_offset;
    }

    return result;
}

} // namespace

WavDecodeResult decode_wav(std::span<const std::byte> bytes) {
    if (bytes.size() < riff_header_size || !matches_ascii(bytes, 0, "RIFF") ||
        !matches_ascii(bytes, 8, "WAVE")) {
        return WavDecodeResult{WavDecodeStatus::Malformed, {}};
    }

    const std::optional<ParsedChunks> chunks = walk_chunks(bytes);
    if (!chunks.has_value() || !chunks->fmt.has_value() || !chunks->data.has_value()) {
        return WavDecodeResult{WavDecodeStatus::Malformed, {}};
    }

    const FmtFields& fmt = *chunks->fmt;
    constexpr std::uint16_t pcm_audio_format = 1;
    if (fmt.audio_format != pcm_audio_format || fmt.channels != canonical_channels ||
        fmt.sample_rate != canonical_sample_rate || fmt.bits_per_sample != canonical_bits_per_sample) {
        return WavDecodeResult{WavDecodeStatus::UnsupportedFormat, {}};
    }

    const DataChunk& data = *chunks->data;
    constexpr std::size_t bytes_per_sample = canonical_bits_per_sample / 8;
    if (data.size % bytes_per_sample != 0) {
        return WavDecodeResult{WavDecodeStatus::Malformed, {}};
    }

    DecodedClip clip{.sample_rate = canonical_sample_rate, .channels = canonical_channels, .samples = {}};
    clip.samples.reserve(data.size / bytes_per_sample);
    for (std::size_t offset = 0; offset < data.size; offset += bytes_per_sample) {
        clip.samples.push_back(read_i16(bytes, data.offset + offset));
    }

    return WavDecodeResult{WavDecodeStatus::Ok, std::move(clip)};
}

} // namespace atlas::audio
