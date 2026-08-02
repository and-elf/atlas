#include "atlas/render/mesh_upload_cache.hpp"

#include <SDL3/SDL.h>
#include <cstring>
#include <optional>

namespace atlas::render {

namespace {

// Uploads already-decoded CPU-side bytes to a new GPU buffer of the given
// usage flag, via a transfer buffer - the same create-transfer-buffer/map/
// memcpy/copy-pass-upload/submit pattern create_sdl3_triangle_pipeline
// (issue #153, since superseded and removed as part of this issue)
// established. Returns nullptr on any failure (SDL_GetError() already
// reports why); the caller (get_or_upload below) treats that as this
// resource's upload having failed, not a crash. A size_bytes of zero
// (decode_mesh's own well-formed "zero vertices/indices" case) also
// returns nullptr, but is not itself a failure - see MeshUploadResult's own
// doc comment.
SDL_GPUBuffer* upload_gpu_buffer(SDL_GPUDevice* device,
                                 SDL_GPUBufferUsageFlags usage,
                                 const void* data,
                                 std::uint32_t size_bytes) {
    if (size_bytes == 0) {
        return nullptr;
    }

    const SDL_GPUBufferCreateInfo buffer_info{.usage = usage, .size = size_bytes, .props = 0};
    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &buffer_info);
    if (buffer == nullptr) {
        return nullptr;
    }

    const SDL_GPUTransferBufferCreateInfo transfer_info{
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = size_bytes, .props = 0};
    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_info);
    if (transfer_buffer == nullptr) {
        SDL_ReleaseGPUBuffer(device, buffer);
        return nullptr;
    }

    void* mapped = SDL_MapGPUTransferBuffer(device, transfer_buffer, /*cycle=*/false);
    if (mapped == nullptr) {
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return nullptr;
    }
    std::memcpy(mapped, data, size_bytes);
    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

    SDL_GPUCommandBuffer* command_buffer = SDL_AcquireGPUCommandBuffer(device);
    if (command_buffer == nullptr) {
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return nullptr;
    }

    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    if (copy_pass == nullptr) {
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return nullptr;
    }

    const SDL_GPUTransferBufferLocation source{.transfer_buffer = transfer_buffer, .offset = 0};
    const SDL_GPUBufferRegion destination{.buffer = buffer, .offset = 0, .size = size_bytes};
    SDL_UploadToGPUBuffer(copy_pass, &source, &destination, /*cycle=*/false);
    SDL_EndGPUCopyPass(copy_pass);

    if (!SDL_SubmitGPUCommandBuffer(command_buffer)) {
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return nullptr;
    }

    // Safe to release once the copy is submitted - SDL_GPU itself defers
    // actual reclamation until the submitted work is safe to reclaim.
    SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
    return buffer;
}

} // namespace

MeshUploadCache::MeshUploadCache(const resource::ResourceRegistry& registry,
                                 std::string_view type_name,
                                 SDL_GPUDevice* device)
    : registry_(&registry), type_name_(type_name), device_(device) {}

const MeshUploadResult& MeshUploadCache::get_or_upload(ResourceId id) {
    const auto cached_it = cache_.find(id);
    if (cached_it != cache_.end()) {
        return cached_it->second;
    }

    const resource::Resolution resolution = registry_->resolve(type_name_, id);

    MeshUploadResult result;
    switch (resolution.status) {
        case resource::ResolutionStatus::Unresolved:
            result.status = MeshUploadCacheStatus::Unresolved;
            break;
        case resource::ResolutionStatus::ResolutionFailed:
            result.status = MeshUploadCacheStatus::ResolutionFailed;
            break;
        case resource::ResolutionStatus::Resolved: {
            const std::optional<DecodedMesh> decoded = decode_mesh(resolution.bytes);
            if (!decoded.has_value()) {
                result.status = MeshUploadCacheStatus::DecodeFailed;
                break;
            }

            const auto vertex_bytes = static_cast<std::uint32_t>(decoded->vertices.size() * sizeof(Vertex));
            const auto index_bytes =
                static_cast<std::uint32_t>(decoded->indices.size() * sizeof(std::uint32_t));

            SDL_GPUBuffer* vertex_buffer = upload_gpu_buffer(
                device_, SDL_GPU_BUFFERUSAGE_VERTEX, decoded->vertices.data(), vertex_bytes);
            SDL_GPUBuffer* index_buffer =
                upload_gpu_buffer(device_, SDL_GPU_BUFFERUSAGE_INDEX, decoded->indices.data(), index_bytes);

            // A non-empty declared vertex/index count whose upload still
            // came back null is a genuine GPU-side failure (out of memory,
            // device lost) - distinct from decode_mesh's own "declares zero
            // vertices/indices" case, which upload_gpu_buffer's own
            // size_bytes == 0 guard already returns nullptr for without
            // that being an error.
            if ((vertex_bytes > 0 && vertex_buffer == nullptr) ||
                (index_bytes > 0 && index_buffer == nullptr)) {
                if (vertex_buffer != nullptr) {
                    SDL_ReleaseGPUBuffer(device_, vertex_buffer);
                }
                if (index_buffer != nullptr) {
                    SDL_ReleaseGPUBuffer(device_, index_buffer);
                }
                result.status = MeshUploadCacheStatus::UploadFailed;
                break;
            }

            result.status = MeshUploadCacheStatus::Ok;
            result.vertex_buffer = vertex_buffer;
            result.index_buffer = index_buffer;
            result.index_count = static_cast<std::uint32_t>(decoded->indices.size());
            break;
        }
    }

    return cache_.emplace(id, result).first->second;
}

void MeshUploadCache::release() noexcept {
    for (auto& [id, result] : cache_) {
        if (result.vertex_buffer != nullptr) {
            SDL_ReleaseGPUBuffer(device_, result.vertex_buffer);
        }
        if (result.index_buffer != nullptr) {
            SDL_ReleaseGPUBuffer(device_, result.index_buffer);
        }
    }
    cache_.clear();
}

} // namespace atlas::render
