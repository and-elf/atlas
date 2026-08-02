#include "atlas/audio/wav_decoder.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace atlas::audio {
namespace {

void append_bytes(std::vector<std::byte>& out, const void* data, std::size_t size) {
    const auto* bytes = static_cast<const std::byte*>(data);
    out.insert(out.end(), bytes, bytes + size);
}

void append_u32(std::vector<std::byte>& out, std::uint32_t value) {
    append_bytes(out, &value, sizeof(value));
}

void append_u16(std::vector<std::byte>& out, std::uint16_t value) {
    append_bytes(out, &value, sizeof(value));
}

void append_ascii(std::vector<std::byte>& out, std::string_view text) {
    append_bytes(out, text.data(), text.size());
}

struct WavParams {
    std::uint16_t audio_format = 1; // PCM
    std::uint16_t channels = 1;
    std::uint32_t sample_rate = 48000;
    std::uint16_t bits_per_sample = 16;
    std::vector<std::int16_t> samples{0, 100, -100, 32767, -32768};
    bool include_data_chunk = true;
    // Inserted verbatim between the fmt chunk and the data chunk - lets a test prove
    // an unrelated chunk (e.g. a real encoder's "LIST"/"fact" metadata) is skipped
    // rather than misread as the data chunk.
    std::vector<std::byte> extra_chunk_before_data;
};

std::vector<std::byte> make_wav(const WavParams& params) {
    std::vector<std::byte> fmt_chunk;
    append_ascii(fmt_chunk, "fmt ");
    append_u32(fmt_chunk, 16); // fmt chunk size (no extension)
    append_u16(fmt_chunk, params.audio_format);
    append_u16(fmt_chunk, params.channels);
    append_u32(fmt_chunk, params.sample_rate);
    const std::uint32_t block_align = static_cast<std::uint32_t>(params.channels) *
                                      (static_cast<std::uint32_t>(params.bits_per_sample) / 8);
    append_u32(fmt_chunk, params.sample_rate * block_align); // byte_rate
    append_u16(fmt_chunk, static_cast<std::uint16_t>(block_align));
    append_u16(fmt_chunk, params.bits_per_sample);

    std::vector<std::byte> data_payload;
    for (const std::int16_t sample : params.samples) {
        append_bytes(data_payload, &sample, sizeof(sample));
    }

    std::vector<std::byte> data_chunk;
    if (params.include_data_chunk) {
        append_ascii(data_chunk, "data");
        append_u32(data_chunk, static_cast<std::uint32_t>(data_payload.size()));
        data_chunk.insert(data_chunk.end(), data_payload.begin(), data_payload.end());
    }

    const auto riff_size = static_cast<std::uint32_t>(
        4 /* "WAVE" */ + fmt_chunk.size() + params.extra_chunk_before_data.size() + data_chunk.size());

    std::vector<std::byte> wav;
    append_ascii(wav, "RIFF");
    append_u32(wav, riff_size);
    append_ascii(wav, "WAVE");
    wav.insert(wav.end(), fmt_chunk.begin(), fmt_chunk.end());
    wav.insert(wav.end(), params.extra_chunk_before_data.begin(), params.extra_chunk_before_data.end());
    wav.insert(wav.end(), data_chunk.begin(), data_chunk.end());
    return wav;
}

std::vector<std::byte> make_canonical_wav() {
    return make_wav(WavParams{});
}

TEST(DecodeWav, ValidCanonicalWavDecodesSamplesCorrectly) {
    const WavDecodeResult result = decode_wav(make_canonical_wav());

    ASSERT_EQ(result.status, WavDecodeStatus::Ok);
    EXPECT_EQ(result.clip.sample_rate, 48000U);
    EXPECT_EQ(result.clip.channels, 1U);
    EXPECT_EQ(result.clip.samples, (std::vector<std::int16_t>{0, 100, -100, 32767, -32768}));
}

TEST(DecodeWav, EmptyByteSpanIsMalformed) {
    const WavDecodeResult result = decode_wav({});

    EXPECT_EQ(result.status, WavDecodeStatus::Malformed);
}

TEST(DecodeWav, RejectsMissingRiffMagic) {
    std::vector<std::byte> wav = make_canonical_wav();
    wav[0] = std::byte{'X'};

    const WavDecodeResult result = decode_wav(wav);

    EXPECT_EQ(result.status, WavDecodeStatus::Malformed);
}

TEST(DecodeWav, RejectsMissingWaveMagic) {
    std::vector<std::byte> wav = make_canonical_wav();
    wav[8] = std::byte{'X'};

    const WavDecodeResult result = decode_wav(wav);

    EXPECT_EQ(result.status, WavDecodeStatus::Malformed);
}

TEST(DecodeWav, RejectsMissingDataChunk) {
    WavParams params;
    params.include_data_chunk = false;

    const WavDecodeResult result = decode_wav(make_wav(params));

    EXPECT_EQ(result.status, WavDecodeStatus::Malformed);
}

TEST(DecodeWav, RejectsTruncatedDataChunkClaimingMoreBytesThanArePresent) {
    std::vector<std::byte> wav = make_canonical_wav();
    wav.resize(wav.size() - 4); // truncate the last two samples' worth of bytes

    const WavDecodeResult result = decode_wav(wav);

    EXPECT_EQ(result.status, WavDecodeStatus::Malformed);
}

TEST(DecodeWav, RejectsDataChunkSizeNotAMultipleOfTheSampleWidth) {
    std::vector<std::byte> wav = make_canonical_wav();
    // Shrink the data chunk's declared size by one byte, leaving a dangling half-sample.
    const std::uint32_t original_size = 5 * sizeof(std::int16_t);
    const std::uint32_t odd_size = original_size - 1;
    std::memcpy(wav.data() + 40, &odd_size, sizeof(odd_size));

    const WavDecodeResult result = decode_wav(wav);

    EXPECT_EQ(result.status, WavDecodeStatus::Malformed);
}

TEST(DecodeWav, RejectsNonPcmAudioFormat) {
    WavParams params;
    params.audio_format = 3; // IEEE float, not PCM

    const WavDecodeResult result = decode_wav(make_wav(params));

    EXPECT_EQ(result.status, WavDecodeStatus::UnsupportedFormat);
}

TEST(DecodeWav, RejectsNonCanonicalSampleRate) {
    WavParams params;
    params.sample_rate = 44100;

    const WavDecodeResult result = decode_wav(make_wav(params));

    EXPECT_EQ(result.status, WavDecodeStatus::UnsupportedFormat);
}

TEST(DecodeWav, RejectsStereoChannelCount) {
    WavParams params;
    params.channels = 2;

    const WavDecodeResult result = decode_wav(make_wav(params));

    EXPECT_EQ(result.status, WavDecodeStatus::UnsupportedFormat);
}

TEST(DecodeWav, RejectsNonSixteenBitDepth) {
    WavParams params;
    params.bits_per_sample = 8;

    const WavDecodeResult result = decode_wav(make_wav(params));

    EXPECT_EQ(result.status, WavDecodeStatus::UnsupportedFormat);
}

TEST(DecodeWav, SkipsAnUnrelatedChunkBetweenFmtAndData) {
    WavParams params;
    append_ascii(params.extra_chunk_before_data, "LIST");
    append_u32(params.extra_chunk_before_data, 4);
    append_ascii(params.extra_chunk_before_data, "INFO");

    const WavDecodeResult result = decode_wav(make_wav(params));

    ASSERT_EQ(result.status, WavDecodeStatus::Ok);
    EXPECT_EQ(result.clip.samples, (std::vector<std::int16_t>{0, 100, -100, 32767, -32768}));
}

TEST(DecodeWav, SkipsAnOddSizedChunkIncludingItsPaddingByte) {
    WavParams params;
    // An odd-sized chunk is padded to an even boundary by the WAV format itself -
    // a decoder that doesn't account for the pad byte would misread "data" one
    // byte later than it actually starts.
    append_ascii(params.extra_chunk_before_data, "LIST");
    append_u32(params.extra_chunk_before_data, 1);
    params.extra_chunk_before_data.push_back(std::byte{'x'});
    params.extra_chunk_before_data.push_back(std::byte{0}); // pad byte

    const WavDecodeResult result = decode_wav(make_wav(params));

    ASSERT_EQ(result.status, WavDecodeStatus::Ok);
    EXPECT_EQ(result.clip.samples, (std::vector<std::int16_t>{0, 100, -100, 32767, -32768}));
}

TEST(DecodeWav, RejectsTruncatedRiffHeader) {
    const WavDecodeResult result = decode_wav(std::vector<std::byte>(8, std::byte{0}));

    EXPECT_EQ(result.status, WavDecodeStatus::Malformed);
}

TEST(DecodeWav, RejectsFmtChunkTooSmallToHoldTheRequiredFields) {
    std::vector<std::byte> wav;
    append_ascii(wav, "RIFF");
    append_u32(wav, 0);
    append_ascii(wav, "WAVE");
    append_ascii(wav, "fmt ");
    append_u32(wav, 4); // declared size is fully present but smaller than the 16 required fields need
    append_u32(wav, 0);

    const WavDecodeResult result = decode_wav(wav);

    EXPECT_EQ(result.status, WavDecodeStatus::Malformed);
}

TEST(DecodeWav, RejectsTruncatedChunkHeader) {
    std::vector<std::byte> wav = make_canonical_wav();
    // Cut off right after "fmt "'s own chunk header, mid-way through its declared payload.
    wav.resize(20);

    const WavDecodeResult result = decode_wav(wav);

    EXPECT_EQ(result.status, WavDecodeStatus::Malformed);
}

} // namespace
} // namespace atlas::audio
