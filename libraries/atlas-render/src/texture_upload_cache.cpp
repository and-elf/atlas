#include "atlas/render/texture_upload_cache.hpp"

#include <SDL3/SDL.h>
#include <cstring>
#include <optional>

namespace atlas::render {

namespace {

// The pixel format decode_texture() always produces (texture_asset.hpp:
// "row-major, top row first, no padding" 8-bit-per-channel RGBA) - the one
// GPU texture format this cache ever creates.
constexpr SDL_GPUTextureFormat texture_format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

} // namespace

TextureUploadCache::TextureUploadCache(const resource::ResourceRegistry& registry,
                                       std::string_view type_name,
                                       SDL_GPUDevice* device)
    : registry_(&registry), type_name_(type_name), device_(device) {}

const TextureUploadResult& TextureUploadCache::get_or_upload(ResourceId id) {
    const auto cached_it = cache_.find(id);
    if (cached_it != cache_.end()) {
        return cached_it->second;
    }

    const resource::Resolution resolution = registry_->resolve(type_name_, id);

    TextureUploadResult result;
    switch (resolution.status) {
        case resource::ResolutionStatus::Unresolved:
            result.status = TextureUploadCacheStatus::Unresolved;
            break;
        case resource::ResolutionStatus::ResolutionFailed:
            result.status = TextureUploadCacheStatus::ResolutionFailed;
            break;
        case resource::ResolutionStatus::Resolved: {
            const std::optional<DecodedTexture> decoded = decode_texture(resolution.bytes);
            if (!decoded.has_value()) {
                result.status = TextureUploadCacheStatus::DecodeFailed;
                break;
            }

            // A zero-dimension texture (decode_texture's own well-formed
            // "empty texture" case) has nothing to upload - an SDL_GPU
            // texture of size 0 is not a meaningful concept, and there is
            // nothing for a draw call to ever sample, so this is not
            // itself a failure; Sdl3FrameBackend::submit() skips the draw
            // when texture is left null below.
            if (decoded->width == 0 || decoded->height == 0) {
                result.status = TextureUploadCacheStatus::Ok;
                result.width = decoded->width;
                result.height = decoded->height;
                break;
            }

            const SDL_GPUTextureCreateInfo texture_info{
                .type = SDL_GPU_TEXTURETYPE_2D,
                .format = texture_format,
                .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
                .width = decoded->width,
                .height = decoded->height,
                .layer_count_or_depth = 1,
                .num_levels = 1,
                .sample_count = SDL_GPU_SAMPLECOUNT_1,
                .props = 0,
            };
            SDL_GPUTexture* texture = SDL_CreateGPUTexture(device_, &texture_info);
            if (texture == nullptr) {
                result.status = TextureUploadCacheStatus::UploadFailed;
                break;
            }

            const auto pixel_bytes = static_cast<Uint32>(decoded->pixels.size());
            const SDL_GPUTransferBufferCreateInfo transfer_info{
                .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = pixel_bytes, .props = 0};
            SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(device_, &transfer_info);
            if (transfer_buffer == nullptr) {
                SDL_ReleaseGPUTexture(device_, texture);
                result.status = TextureUploadCacheStatus::UploadFailed;
                break;
            }

            void* mapped = SDL_MapGPUTransferBuffer(device_, transfer_buffer, /*cycle=*/false);
            if (mapped == nullptr) {
                SDL_ReleaseGPUTransferBuffer(device_, transfer_buffer);
                SDL_ReleaseGPUTexture(device_, texture);
                result.status = TextureUploadCacheStatus::UploadFailed;
                break;
            }
            std::memcpy(mapped, decoded->pixels.data(), decoded->pixels.size());
            SDL_UnmapGPUTransferBuffer(device_, transfer_buffer);

            SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device_);
            if (command_buffer == nullptr) {
                SDL_ReleaseGPUTransferBuffer(device_, transfer_buffer);
                SDL_ReleaseGPUTexture(device_, texture);
                result.status = TextureUploadCacheStatus::UploadFailed;
                break;
            }

            SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
            if (copy_pass == nullptr) {
                SDL_ReleaseGPUTransferBuffer(device_, transfer_buffer);
                SDL_ReleaseGPUTexture(device_, texture);
                result.status = TextureUploadCacheStatus::UploadFailed;
                break;
            }

            const SDL_GPUTextureTransferInfo source{
                .transfer_buffer = transfer_buffer,
                .offset = 0,
                .pixels_per_row = decoded->width,
                .rows_per_layer = decoded->height,
            };
            const SDL_GPUTextureRegion destination{
                .texture = texture,
                .mip_level = 0,
                .layer = 0,
                .x = 0,
                .y = 0,
                .z = 0,
                .w = decoded->width,
                .h = decoded->height,
                .d = 1,
            };
            SDL_UploadToGPUTexture(copy_pass, &source, &destination, /*cycle=*/false);
            SDL_EndGPUCopyPass(copy_pass);

            if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
                SDL_ReleaseGPUTransferBuffer(device_, transfer_buffer);
                SDL_ReleaseGPUTexture(device_, texture);
                result.status = TextureUploadCacheStatus::UploadFailed;
                break;
            }

            // Safe to release once the copy is submitted - SDL_GPU itself
            // defers actual reclamation until the submitted work is safe
            // to reclaim.
            SDL_ReleaseGPUTransferBuffer(device_, transfer_buffer);

            result.status = TextureUploadCacheStatus::Ok;
            result.texture = texture;
            result.width = decoded->width;
            result.height = decoded->height;
            break;
        }
    }

    return cache_.emplace(id, result).first->second;
}

void TextureUploadCache::release() noexcept {
    for (auto& [id, result] : cache_) {
        if (result.texture != nullptr) {
            SDL_ReleaseGPUTexture(device_, result.texture);
        }
    }
    cache_.clear();
}

} // namespace atlas::render
