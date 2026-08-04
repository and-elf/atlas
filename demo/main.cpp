#include "presentation_app.hpp"

#ifdef ATLAS_DEMO_REAL_SDL3_BACKEND
#    include "atlas/input/sdl3_raw_signal_source.hpp"
#    include "atlas/render/sdl3_frame_backend.hpp"
#    include "atlas/resource/resource_registry.hpp"
#    include "atlas/windowing/sdl3_shared_window.hpp"

#    include <filesystem>
#    include <string_view>
#    include <unordered_map>

#    ifdef ATLAS_DEMO_REAL_AUDIO_BACKEND
#        include "atlas/audio/decode_cache.hpp"
#        include "atlas/audio/sdl3_audio_backend.hpp"
#    endif
#else
#    include "atlas/input/null_raw_signal_source.hpp"
#    include "atlas/render/null_frame_backend.hpp"
#endif

// demo-host (issue #70, App shape per PR #185 review feedback; issue #71
// parts 1/2 for the PresentationApp variant below): the actual process
// entry point. Everything demo-host does - composing a real Host+Context+
// DemoRuntimeHost, running a real-time-paced tick loop, argv parsing,
// default SIGINT/SIGTERM handling, and now polling input/building frames/
// rendering audio each tick - lives in atlas::demo::App/PresentationApp
// (app.hpp/app.cpp, presentation_app.hpp), unit tested there
// (demo/tests/app_test.cpp, demo/tests/presentation_app_test.cpp). This
// file itself is not unit tested - constructing the real App and calling
// run() is exactly the "CLI entry point" convention
// tools/atlas-cgen/src/main.cpp documents and cmake/CodeCoverage.cmake
// excludes this file the same way.
//
// ATLAS_DEMO_REAL_SDL3_BACKEND (issue #197): demo/CMakeLists.txt defines
// this only when both ATLAS_RENDER_BACKEND and ATLAS_INPUT_BACKEND are
// SDL3 - a configure-time choice, never a runtime factory/plugin lookup
// (spec §4), matching every other real-vs-null backend selection in this
// codebase. The real branch below constructs one Sdl3SharedWindow (#174) so
// Sdl3RawSignalSource and Sdl3FrameBackend present into/read focus from the
// same single window, plus a real ResourceRegistry (issue #200) pointed at
// demo/resources/Mesh.blob/Texture.blob - the player entity's placeholder
// mesh/texture (PresentationApp seeds its Renderable unconditionally, see
// player_resources.hpp) actually resolves and draws against a real GPU
// backend. Every other CI leg (which stays on the default NULL/NULL
// backends) takes the unchanged Null branch, where a ResourceRegistry is
// never even constructed - NullFrameBackend has no use for one.
//
// ATLAS_DEMO_REAL_AUDIO_BACKEND (issue #201): audio is a third, independent
// backend axis from render/input - demo/CMakeLists.txt defines this only
// when ATLAS_AUDIO_BACKEND is SDL3, regardless of what render/input select.
// Nested inside the real-render/input branch above: when it's also defined,
// the same ResourceRegistry gains a "Sound" entry pointed at
// demo/resources/Sound.blob (issue #201's own real door-open WAV asset), a
// DecodeCache is constructed from it (must outlive the real Sdl3AudioBackend
// and, in turn, the PresentationApp that owns that backend - both declared
// as locals here, before app, for exactly that reason), and a real
// Sdl3AudioBackend is passed as PresentationApp's 5th constructor argument.
// When it isn't defined (real render/input, still-Null audio - a real,
// distinct configuration this issue introduces), the unchanged 4-argument
// constructor is used, which delegates to a default-constructed
// NullAudioBackend (see presentation_app.hpp's own constructor doc
// comment). This makes three branches total, not two: (a) real
// render+input+audio, (b) real render+input only (Null audio), (c) all-Null
// (unaffected by this issue either way).
int main(int argc, char** argv) {
#ifdef ATLAS_DEMO_REAL_SDL3_BACKEND
    atlas::windowing::Sdl3SharedWindow window;
    constexpr std::string_view resources_dir = DEMO_RESOURCES_DIR;
#    ifdef ATLAS_DEMO_REAL_AUDIO_BACKEND
    const atlas::resource::ResourceRegistry registry{{
        {"Mesh", std::filesystem::path{resources_dir} / "Mesh.blob"},
        {"Texture", std::filesystem::path{resources_dir} / "Texture.blob"},
        {"Sound", std::filesystem::path{resources_dir} / "Sound.blob"},
    }};
    atlas::audio::DecodeCache decode_cache{registry, "Sound"};
    atlas::demo::PresentationApp<atlas::input::Sdl3RawSignalSource,
                                 atlas::render::Sdl3FrameBackend,
                                 atlas::audio::Sdl3AudioBackend>
        app(argc,
            argv,
            atlas::input::Sdl3RawSignalSource{window},
            atlas::render::Sdl3FrameBackend{registry, window},
            atlas::audio::Sdl3AudioBackend{decode_cache});
#    else
    const atlas::resource::ResourceRegistry registry{{
        {"Mesh", std::filesystem::path{resources_dir} / "Mesh.blob"},
        {"Texture", std::filesystem::path{resources_dir} / "Texture.blob"},
    }};
    atlas::demo::PresentationApp<atlas::input::Sdl3RawSignalSource, atlas::render::Sdl3FrameBackend> app(
        argc,
        argv,
        atlas::input::Sdl3RawSignalSource{window},
        atlas::render::Sdl3FrameBackend{registry, window});
#    endif
#else
    atlas::demo::PresentationApp<atlas::input::NullRawSignalSource, atlas::render::NullFrameBackend> app(
        argc, argv, atlas::input::NullRawSignalSource{}, atlas::render::NullFrameBackend{});
#endif
    return app.run();
}
