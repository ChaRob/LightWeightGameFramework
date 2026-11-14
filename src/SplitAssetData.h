#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>

namespace lwgf::detail
{
    inline constexpr std::uint64_t DefaultAssetPartSize = 512ull * 1024 * 1024;

    // Private implementation primitive: _base is the path without ".1", etc.
    // The caller owns staging, publication and package metadata.
    class SplitAssetWriter
    {
    public:
        SplitAssetWriter(std::filesystem::path _base, std::uint64_t _partSize);
        bool Write(std::span<const std::byte> _bytes);
        bool Finish();
        std::uint64_t GetSize() const { return m_size; }

    private:
        bool ClosePart();
        bool OpenPart(std::uint64_t _number);

        std::filesystem::path m_base;
        std::uint64_t m_partSize;
        std::uint64_t m_size = 0;
        std::ofstream m_file;
        bool m_failed = false;
        bool m_finished = false;
    };

    // One reader owns its own file position. Not safe for concurrent calls on
    // the same instance. The published package must not change while in use.
    class SplitAssetReader
    {
    public:
        SplitAssetReader(std::filesystem::path _base, std::uint64_t _partSize,
                         std::uint64_t _totalSize);
        bool Validate();
        // Offset is relative to the complete logical data stream.
        // On failure, the destination may contain a partially read prefix.
        bool ReadAt(std::uint64_t _offset, std::span<std::byte> _bytes);

    private:
        std::filesystem::path m_base;
        std::uint64_t m_partSize;
        std::uint64_t m_totalSize;
        std::ifstream m_file;
        std::uint64_t m_openPart = 0;
        bool m_valid = false;
    };
}
