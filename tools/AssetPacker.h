#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace lwgf::tools
{
    // Output must be a dedicated package directory, not the game binary directory.
    // Existing valid package-only directories can be replaced; unrelated files
    // are never deleted. Source and destination must not overlap.
    bool PackAssets(const std::filesystem::path& _source,
                    const std::filesystem::path& _output,
                    std::uint64_t _partSize, std::string& _error);

    bool VerifyAssets(const std::filesystem::path& _directory, std::string& _error);
}
