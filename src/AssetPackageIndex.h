#pragma once

#include "AssetPackageFormat.h"

#include <filesystem>
#include <string>
#include <vector>

namespace lwgf::detail
{
    // Internal V1 metadata; paths are not OS file paths.
    struct AssetIndexEntry
    {
        std::string path;
        std::uint64_t offset = 0;
        std::uint64_t size = 0;
    };

    struct AssetPackageIndex
    {
        PackageId id{};
        std::uint64_t partSize = DefaultAssetPartSize;
        std::uint64_t totalSize = 0;
        std::vector<std::uint32_t> partCrcs;
        std::vector<AssetIndexEntry> entries;
    };

    // Metadata-only allocation limit to reject unreasonable/corrupt indexes.
    inline constexpr std::size_t MaxAssetIndexSize = 64 * 1024 * 1024;

    // On failure the output argument is unchanged. Write requires a new file
    // in private staging; the caller handles cleanup/publication.
    bool EncodeAssetIndex(const AssetPackageIndex& _index, std::vector<std::byte>& _bytes);
    bool DecodeAssetIndex(std::span<const std::byte> _bytes, AssetPackageIndex& _index);
    bool WriteAssetIndex(const std::filesystem::path& _path, const AssetPackageIndex& _index);
    bool ReadAssetIndex(const std::filesystem::path& _path, AssetPackageIndex& _index);
}
