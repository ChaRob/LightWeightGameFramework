#pragma once

#include <miniaudio.h>

class AssetReader;

namespace lwgf::detail
{
    // miniaudio expects the callbacks table at offset zero of a custom VFS.
    // Each opened handle owns an independent AssetStream.
    // The borrowed reader must remain configured and alive until engine teardown.
    struct AudioAssetVfs
    {
        explicit AudioAssetVfs(const AssetReader* _assets);

        AudioAssetVfs(const AudioAssetVfs&) = delete;
        AudioAssetVfs& operator=(const AudioAssetVfs&) = delete;

        ma_vfs* Get() { return static_cast<ma_vfs*>(this); }

        ma_vfs_callbacks callbacks{};
        const AssetReader* assets;
    };

    inline constexpr ma_uint32 AudioEffectFlags =
        MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH;
    inline constexpr ma_uint32 AudioMusicFlags =
        MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH;
}
