#include <framework/AssetReader.h>
#include "AudioAssetVfs.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

#define CHECK(_condition) do { if (!(_condition)) { \
    std::cerr << "Check failed at line " << __LINE__ << ": " << #_condition << '\n'; \
    return __LINE__; } } while (false)

namespace
{
    namespace fs = std::filesystem;
    using namespace lwgf::detail;

    struct File
    {
        ma_vfs* vfs;
        ma_vfs_file handle = nullptr;
        ~File() { if (handle) ma_vfs_close(vfs, handle); }
    };

    struct Decoder
    {
        ma_decoder value{};
        bool initialized = false;
        ~Decoder() { if (initialized) ma_decoder_uninit(&value); }
    };

    struct Engine
    {
        ma_engine value{};
        bool initialized = false;
        ~Engine() { if (initialized) ma_engine_uninit(&value); }
    };

    struct Sound
    {
        ma_sound value{};
        bool initialized = false;
        ~Sound() { if (initialized) ma_sound_uninit(&value); }
    };

    int CheckVfs(const AssetReader& _assets)
    {
        AudioAssetVfs bridge(&_assets);
        auto* vfs = bridge.Get();
        File first{vfs};
        File second{vfs};
        File invalid{vfs};

        CHECK(ma_vfs_open(vfs, "audio/music.wav", MA_OPEN_MODE_READ, &first.handle) == MA_SUCCESS);
        CHECK(ma_vfs_open_w(vfs, L"audio/music.wav", MA_OPEN_MODE_READ, &second.handle) == MA_SUCCESS);
        CHECK(ma_vfs_open(vfs, "audio/music.wav", MA_OPEN_MODE_WRITE, &invalid.handle) != MA_SUCCESS);
        CHECK(ma_vfs_open(vfs, "audio/missing.wav", MA_OPEN_MODE_READ, &invalid.handle) != MA_SUCCESS);
        CHECK(ma_vfs_open(vfs, "../audio/music.wav", MA_OPEN_MODE_READ, &invalid.handle) != MA_SUCCESS);
        CHECK(ma_vfs_open(vfs, "audio/MUSIC.wav", MA_OPEN_MODE_READ, &invalid.handle) != MA_SUCCESS);
        CHECK(invalid.handle == nullptr);

        ma_file_info info{};
        CHECK(ma_vfs_info(vfs, first.handle, &info) == MA_SUCCESS);
        CHECK(info.sizeInBytes > 4096);
        std::array<std::byte, 32> a{};
        std::array<std::byte, 32> b{};
        size_t read = 0;
        CHECK(ma_vfs_read(vfs, first.handle, a.data(), a.size(), &read) == MA_SUCCESS);
        CHECK(read == a.size());
        ma_int64 cursor = -1;
        CHECK(ma_vfs_tell(vfs, second.handle, &cursor) == MA_SUCCESS && cursor == 0);
        CHECK(ma_vfs_read(vfs, second.handle, b.data(), b.size(), &read) == MA_SUCCESS);
        CHECK(read == b.size() && a == b);

        CHECK(ma_vfs_seek(vfs, first.handle, -8, ma_seek_origin_end) == MA_SUCCESS);
        CHECK(ma_vfs_read(vfs, first.handle, a.data(), a.size(), &read) == MA_SUCCESS);
        CHECK(read == 8);
        CHECK(ma_vfs_read(vfs, first.handle, a.data(), a.size(), &read) == MA_AT_END);
        CHECK(read == 0);
        CHECK(ma_vfs_seek(vfs, first.handle, 1, ma_seek_origin_end) != MA_SUCCESS);
        CHECK(ma_vfs_seek(vfs, first.handle, std::numeric_limits<ma_int64>::min(),
                          ma_seek_origin_current) != MA_SUCCESS);
        CHECK(ma_vfs_tell(vfs, first.handle, &cursor) == MA_SUCCESS &&
              static_cast<ma_uint64>(cursor) == info.sizeInBytes);
        CHECK(ma_vfs_seek(vfs, first.handle, 0, ma_seek_origin_start) == MA_SUCCESS);
        CHECK(ma_vfs_read(vfs, first.handle, a.data(), a.size(), &read) == MA_SUCCESS);
        CHECK(a == b);
        CHECK(ma_vfs_tell(vfs, second.handle, &cursor) == MA_SUCCESS && cursor == 32);
        return 0;
    }

    // The real miniaudio decoder must produce identical PCM in both modes.
    // Read in small chunks across many physical parts, then seek backwards.
    int CompareDecoder(const AssetReader& _directory, const AssetReader& _package,
                       const char* _path)
    {
        AudioAssetVfs directory(&_directory);
        AudioAssetVfs package(&_package);
        Decoder a;
        Decoder b;
        const auto config = ma_decoder_config_init(ma_format_f32, 2, 48000);
        CHECK(ma_decoder_init_vfs(directory.Get(), _path, &config, &a.value) == MA_SUCCESS);
        a.initialized = true;
        CHECK(ma_decoder_init_vfs(package.Get(), _path, &config, &b.value) == MA_SUCCESS);
        b.initialized = true;
        std::array<float, 1024 * 2> left{};
        std::array<float, 1024 * 2> right{};
        ma_uint64 total = 0;
        for (;;)
        {
            ma_uint64 countA = 0;
            ma_uint64 countB = 0;
            const auto resultA = ma_decoder_read_pcm_frames(&a.value, left.data(), 1024, &countA);
            const auto resultB = ma_decoder_read_pcm_frames(&b.value, right.data(), 1024, &countB);
            CHECK((resultA == MA_SUCCESS || resultA == MA_AT_END) && resultA == resultB);
            CHECK(countA == countB && countA <= 1024);
            CHECK(std::equal(left.begin(), left.begin() + countA * 2, right.begin()));
            total += countA;
            if (countA == 0 || resultA == MA_AT_END)
                break;
        }
        CHECK(total > 0);
        CHECK(ma_decoder_seek_to_pcm_frame(&a.value, total / 2) == MA_SUCCESS);
        CHECK(ma_decoder_seek_to_pcm_frame(&b.value, total / 2) == MA_SUCCESS);
        ma_uint64 countA = 0;
        ma_uint64 countB = 0;
        const auto resultA = ma_decoder_read_pcm_frames(&a.value, left.data(), 1024, &countA);
        const auto resultB = ma_decoder_read_pcm_frames(&b.value, right.data(), 1024, &countB);
        CHECK((resultA == MA_SUCCESS || resultA == MA_AT_END) && resultA == resultB);
        CHECK(countA == countB && countA > 0 && countA <= 1024);
        CHECK(std::equal(left.begin(), left.begin() + countA * 2, right.begin()));
        return 0;
    }

    // Exercise the same resource-manager VFS and flags as AudioSystem with a
    // no-device engine. This tests the real streaming/decoded/copy backend.
    bool MixNonSilent(ma_engine& _engine)
    {
        std::array<float, 512 * 2> pcm{};
        for (int attempt = 0; attempt < 500; ++attempt)
        {
            ma_uint64 read = 0;
            if (ma_engine_read_pcm_frames(&_engine, pcm.data(), 512, &read) != MA_SUCCESS)
                return false;
            if (std::any_of(pcm.begin(), pcm.begin() + read * 2,
                            [](float _value) { return _value != 0.0f; }))
                return true;
            // Resource manager streaming jobs are asynchronous even when the
            // engine itself is advanced manually. Yield to the worker.
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
        return false;
    }

    int CheckEngine(const AssetReader& _assets)
    {
        AudioAssetVfs bridge(&_assets);
        Engine engine;
        auto config = ma_engine_config_init();
        config.noDevice = MA_TRUE;
        config.channels = 2;
        config.sampleRate = 48000;
        config.pResourceManagerVFS = bridge.Get();
        CHECK(ma_engine_init(&config, &engine.value) == MA_SUCCESS);
        engine.initialized = true;

        {
            Sound effect;
            Sound voice;
            CHECK(ma_sound_init_from_file(&engine.value, "audio/click.wav", AudioEffectFlags,
                                          nullptr, nullptr, &effect.value) == MA_SUCCESS);
            effect.initialized = true;
            CHECK(ma_sound_init_copy(&engine.value, &effect.value, AudioEffectFlags,
                                     nullptr, &voice.value) == MA_SUCCESS);
            voice.initialized = true;
            CHECK(ma_sound_start(&voice.value) == MA_SUCCESS);
            CHECK(MixNonSilent(engine.value));
            CHECK(ma_sound_stop(&voice.value) == MA_SUCCESS);
        }

        {
            Sound music;
            CHECK(ma_sound_init_from_file(&engine.value, "audio/music.wav",
                                          AudioMusicFlags | MA_SOUND_FLAG_LOOPING,
                                          nullptr, nullptr, &music.value) == MA_SUCCESS);
            music.initialized = true;
            CHECK(ma_sound_is_looping(&music.value));
            CHECK(ma_sound_start(&music.value) == MA_SUCCESS);
            CHECK(MixNonSilent(engine.value));
            CHECK(ma_sound_stop(&music.value) == MA_SUCCESS);
            CHECK(ma_sound_seek_to_pcm_frame(&music.value, 0) == MA_SUCCESS);
            CHECK(ma_sound_start(&music.value) == MA_SUCCESS);
            CHECK(MixNonSilent(engine.value));

            Sound invalid;
            CHECK(ma_sound_init_from_file(&engine.value, "audio/missing.wav", AudioMusicFlags,
                                          nullptr, nullptr, &invalid.value) != MA_SUCCESS);
            CHECK(ma_sound_is_playing(&music.value));
            CHECK(ma_sound_stop(&music.value) == MA_SUCCESS);
        }
        return 0;
    }

    int RunTests(const fs::path& _source, const fs::path& _package)
    {
        AssetReader directory;
        AssetReader package;
        CHECK(directory.OpenDirectory(_source));
        CHECK(package.OpenPackage(_package));

        CHECK(CheckVfs(directory) == 0);
        CHECK(CheckVfs(package) == 0);
        CHECK(CompareDecoder(directory, package, "audio/click.wav") == 0);
        CHECK(CompareDecoder(directory, package, "audio/music.wav") == 0);
        CHECK(CheckEngine(directory) == 0);
        CHECK(CheckEngine(package) == 0);
        return 0;
    }
}

#ifdef _WIN32
int wmain(int _argc, wchar_t** _argv)
#else
int main(int _argc, char** _argv)
#endif
{
    if (_argc != 3)
        return 1;
    return RunTests(std::filesystem::path(_argv[1]), std::filesystem::path(_argv[2]));
}
