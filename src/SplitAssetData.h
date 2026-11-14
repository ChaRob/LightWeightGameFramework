#pragma once

#include "AssetPackageFormat.h"

#include <filesystem>
#include <fstream>
#include <vector>

namespace lwgf::detail
{
    // _base is the path without ".1", etc. _partSize includes the header.
    // The caller owns the generation ID, staging, publication and index.
    class SplitAssetWriter
    {
    public:
        SplitAssetWriter(std::filesystem::path _base, std::uint64_t _partSize,
                         PackageId _id);
        bool Write(std::span<const std::byte> _bytes);
        bool Finish();
        std::uint64_t GetSize() const { return m_size; }
        // Valid after successful Finish(). One payload CRC per physical part.
        const std::vector<std::uint32_t>& GetPartCrcs() const { return m_partCrcs; }

    private:
        bool ClosePart();
        bool OpenPart(std::uint64_t _number);

        std::filesystem::path m_base;
        std::uint64_t m_partSize;
        PackageId m_id;
        std::uint64_t m_size = 0;
        std::ofstream m_file;
        Crc32 m_crc;
        std::vector<std::uint32_t> m_partCrcs;
        bool m_failed = false;
        bool m_finished = false;
    };

    // One reader owns its own file position. Not safe for concurrent calls on
    // the same instance. The published package must not change while in use.
    class SplitAssetReader
    {
    public:
        SplitAssetReader(std::filesystem::path _base, std::uint64_t _partSize,
                         std::uint64_t _totalSize, PackageId _id);
        // Header/size only. Does not read the payload.
        bool Validate();
        // Explicit full scan, intended for packaging/diagnostics.
        bool VerifyPayload(std::span<const std::uint32_t> _partCrcs);
        // Offset is relative to the complete logical data stream.
        // On failure, the destination may contain a partially read prefix.
        bool ReadAt(std::uint64_t _offset, std::span<std::byte> _bytes);

    private:
        bool OpenVerifiedPart(std::uint64_t _number, std::uint64_t _expectedPayload,
                              std::ifstream& _file) const;

        std::filesystem::path m_base;
        std::uint64_t m_partSize;
        std::uint64_t m_totalSize;
        PackageId m_id;
        std::ifstream m_file;
        std::uint64_t m_openPart = 0;
        bool m_valid = false;
    };
}
